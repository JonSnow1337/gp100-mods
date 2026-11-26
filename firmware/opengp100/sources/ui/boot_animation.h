#ifndef GP100_BOOT_ANIMATION_H
#define GP100_BOOT_ANIMATION_H

/*
 * User boot animation stored in a reserved flash area (written by the gp100-mods app into the update file).
 * Little-endian layout, offsets from the start of the area:
 *   0x00  char[4]  magic "GPBA"
 *   0x04  u16      version (1)
 *   0x06  u16      header size (0x20)
 *   0x08  u16      width            (must equal the canvas width, 160)
 *   0x0A  u16      height           (must equal the canvas height, 128)
 *   0x0C  u16      frame count      (0 = no animation installed -> built-in logo)
 *   0x0E  u16      palette count    (1..256)
 *   0x10  u32      total size       (header + palette + table + frame data)
 *   0x14  u32      checksum         (sum of every byte from 0x20 to total size, mod 2^32)
 *   0x18  u32      reserved
 *   0x1C  u32      reserved
 *   0x20  u16[palette count]  RGB565 palette (the value written to the canvas)
 *   ...   frame table, 8 bytes each: u32 data offset (from area start), u16 duration ms, u8 encoding, u8 reserved
 *   ...   frame data. encoding 0 = raw width*height palette indices; 1 = RLE of (count 1..255, index) byte pairs.
 * Playback is time-driven (late frames are skipped) and limited to GP100_BOOT_ANIM_MAX_MS.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_BOOT_ANIM_FLASH_OFFSET 0x001CC000u
#define GP100_BOOT_ANIM_AREA_BYTES   0x00100000u
#define GP100_BOOT_ANIM_MAX_MS       10000u
#define GP100_BOOT_ANIM_HEADER_BYTES 0x20u

typedef struct gp100_boot_anim {
    const uint8_t *area;
    const uint16_t *palette;      /* little-endian halfwords; the area is 4-byte aligned in flash */
    const uint8_t *table;
    uint16_t width, height, frame_count, palette_count;
    uint32_t total_ms;
} gp100_boot_anim_t;

/* Validate an animation area and fill `anim`. Returns false if there is no valid animation. */
bool gp100_boot_anim_parse(const uint8_t *area, size_t max_bytes, uint16_t canvas_width,
                           uint16_t canvas_height, gp100_boot_anim_t *anim);

/* Index of the frame to show at `elapsed_ms`, or frame_count when playback has finished. */
uint16_t gp100_boot_anim_frame_at(const gp100_boot_anim_t *anim, uint32_t elapsed_ms);

/* Decode frame `index` to width*height palette indices. `raw` frames are returned in place (no copy):
 * *out points either into the area or into `scratch` (must hold width*height bytes). */
bool gp100_boot_anim_frame_pixels(const gp100_boot_anim_t *anim, uint16_t index, uint8_t *scratch,
                                  const uint8_t **out);

#endif
