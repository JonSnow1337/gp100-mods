#include "boot_animation.h"

#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool gp100_boot_anim_parse(const uint8_t *area, size_t max_bytes, uint16_t canvas_width,
                           uint16_t canvas_height, gp100_boot_anim_t *anim)
{
    uint32_t total, sum = 0u, ms = 0u;
    uint16_t frames, colors;
    size_t i, table_bytes, data_start;

    if (area == NULL || anim == NULL || max_bytes < GP100_BOOT_ANIM_HEADER_BYTES) { return false; }
    if (memcmp(area, "GPBA", 4) != 0 || rd16(area + 4) != 1u || rd16(area + 6) != GP100_BOOT_ANIM_HEADER_BYTES) { return false; }
    frames = rd16(area + 0x0c);
    colors = rd16(area + 0x0e);
    total = rd32(area + 0x10);
    if (frames == 0u || colors == 0u || colors > 256u) { return false; }
    if (rd16(area + 8) != canvas_width || rd16(area + 10) != canvas_height) { return false; }
    table_bytes = (size_t)frames * 8u;
    data_start = GP100_BOOT_ANIM_HEADER_BYTES + (size_t)colors * 2u + table_bytes;
    if (total < data_start || total > max_bytes) { return false; }
    for (i = GP100_BOOT_ANIM_HEADER_BYTES; i < total; ++i) { sum += area[i]; }
    if (sum != rd32(area + 0x14)) { return false; }

    anim->area = area;
    anim->palette = (const uint16_t *)(const void *)(area + GP100_BOOT_ANIM_HEADER_BYTES);
    anim->table = area + GP100_BOOT_ANIM_HEADER_BYTES + (size_t)colors * 2u;
    anim->width = canvas_width;
    anim->height = canvas_height;
    anim->frame_count = frames;
    anim->palette_count = colors;
    for (i = 0u; i < frames; ++i) {
        const uint8_t *e = anim->table + i * 8u;
        const uint32_t off = rd32(e);
        const uint8_t enc = e[6];
        if (off < data_start || off >= total || enc > 1u) { return false; }
        if (enc == 0u && (size_t)off + (size_t)canvas_width * canvas_height > total) { return false; }
        ms += rd16(e + 4);
    }
    if (ms == 0u || ms > GP100_BOOT_ANIM_MAX_MS) { return false; }
    anim->total_ms = ms;
    return true;
}

uint16_t gp100_boot_anim_frame_at(const gp100_boot_anim_t *anim, uint32_t elapsed_ms)
{
    uint32_t t = 0u;
    uint16_t i;
    for (i = 0u; i < anim->frame_count; ++i) {
        t += rd16(anim->table + (size_t)i * 8u + 4u);
        if (elapsed_ms < t) { return i; }
    }
    return anim->frame_count;
}

bool gp100_boot_anim_frame_pixels(const gp100_boot_anim_t *anim, uint16_t index, uint8_t *scratch,
                                  const uint8_t **out)
{
    const uint8_t *e, *src;
    size_t pixels = (size_t)anim->width * anim->height, produced = 0u;
    uint32_t off, end;

    if (index >= anim->frame_count || out == NULL) { return false; }
    e = anim->table + (size_t)index * 8u;
    off = rd32(e);
    src = anim->area + off;
    if (e[6] == 0u) { *out = src; return true; }
    if (scratch == NULL) { return false; }
    end = rd32(anim->area + 0x10);
    while (produced < pixels) {
        uint8_t count, value;
        if (off + 2u > end) { return false; }
        count = src[0]; value = src[1];
        src += 2; off += 2u;
        if (count == 0u || produced + count > pixels) { return false; }
        memset(scratch + produced, value, count);
        produced += count;
    }
    *out = scratch;
    return true;
}
