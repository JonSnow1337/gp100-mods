#include "factory_presets.h"

#include <string.h>

#define GP100_FP_ANIM_FLASH_OFFSET 0x001CC000u
#define GP100_FP_ANIM_AREA_BYTES   0x00100000u
#define GP100_FP_FLASH_MAPPED_BASE 0x60000000u

/* Capability marker: the gp100-mods app looks for this text in a bundled firmware to know it can carry factory presets. */
__attribute__((used)) const char gp100_factory_presets_marker[] = "GP100-FACTORY-PRESETS-1";

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t gp100_factory_presets_parse(const uint8_t *block, size_t max_bytes)
{
    uint16_t count;

    if (block == NULL || max_bytes < GP100_FACTORY_PRESETS_HEADER_BYTES || memcmp(block, "GPPR", 4) != 0) { return 0u; }
    if (rd16(block + 4) != 1u || rd16(block + 6) != GP100_FACTORY_PRESETS_HEADER_BYTES) { return 0u; }
    count = rd16(block + 8);
    if (rd16(block + 10) != GP100_PRESET_RECORD_SIZE || count == 0u || count > GP100_PRESETS_PER_BANK) { return 0u; }
    if (max_bytes - GP100_FACTORY_PRESETS_HEADER_BYTES < (size_t)count * GP100_PRESET_RECORD_SIZE) { return 0u; }
    return count;
}

bool gp100_factory_presets_get(const uint8_t *block, size_t max_bytes, uint16_t slot, gp100_preset_record_t *out)
{
    gp100_preset_record_t rec;

    if (out == NULL || slot >= gp100_factory_presets_parse(block, max_bytes)) { return false; }
    memcpy(rec.bytes, block + GP100_FACTORY_PRESETS_HEADER_BYTES + (size_t)slot * GP100_PRESET_RECORD_SIZE,
           GP100_PRESET_RECORD_SIZE);
    if (gp100_preset_validate(&rec) != GP100_PRESET_VALID) { return false; }
    *out = rec;
    return true;
}

bool gp100_factory_preset_load(uint16_t slot, gp100_preset_record_t *out)
{
    const uint8_t *area = (const uint8_t *)(uintptr_t)(GP100_FP_FLASH_MAPPED_BASE + GP100_FP_ANIM_FLASH_OFFSET);
    uint32_t total;
    size_t start;

    if (*(const volatile char *)gp100_factory_presets_marker != 'G') { return false; }   /* a real read keeps the marker in the image */
    /* The block follows the boot-animation area; its size is in the animation header (an empty area is just the header). */
    if (memcmp(area, "GPBA", 4) != 0) { return false; }
    total = rd32(area + 0x10);
    if (total < 0x20u || total > GP100_FP_ANIM_AREA_BYTES) { return false; }
    start = (size_t)((total + 0xFFFu) & ~0xFFFu);
    if (start >= GP100_FP_ANIM_AREA_BYTES) { return false; }
    return gp100_factory_presets_get(area + start, GP100_FP_ANIM_AREA_BYTES - start, slot, out);
}
