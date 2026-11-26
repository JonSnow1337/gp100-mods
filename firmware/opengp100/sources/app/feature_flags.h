#ifndef GP100_FEATURE_FLAGS_H
#define GP100_FEATURE_FLAGS_H

/*
 * Build-time feature switches stored in a small reserved flash block, written by the gp100-mods app into the update file.
 * Little-endian layout, at flash offset GP100_FEATURE_FLAGS_FLASH_OFFSET:
 *   0x00  char[4]  magic "GPFL"
 *   0x04  u16      version (1)
 *   0x06  u16      block size (0x40)
 *   0x08  u32      flags
 *   0x0C  u32      ~flags (integrity: a damaged or erased block means "no flags")
 *   0x10  ...      unused (0xFF)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_FEATURE_FLAGS_FLASH_OFFSET 0x001CBF00u
#define GP100_FEATURE_FLAGS_BYTES        0x40u

#define GP100_FEATURE_STARTER_PRESET     (1u << 0)   /* offer the built-in "Starter" preset in slot P01 while it is empty */

#define GP100_FEATURE_FINE_MASTER_VOLUME (1u << 1)   /* finer steps at the quiet end of MASTER (see audio_output.h) */

/* Parse a block. Returns the flags, or 0 if the block is missing or invalid. */
uint32_t gp100_feature_flags_parse(const uint8_t *block, size_t bytes);

/* Flags of the running firmware (reads the block from the XIP flash window). */
uint32_t gp100_feature_flags(void);

static inline bool gp100_feature_enabled(uint32_t flag) { return (gp100_feature_flags() & flag) != 0u; }

#endif
