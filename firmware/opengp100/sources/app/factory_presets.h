#ifndef GP100_FACTORY_PRESETS_H
#define GP100_FACTORY_PRESETS_H

/*
 * Factory presets written into the update file by the gp100-mods app. They sit right after the boot-animation area
 * (flash 0x1CC000), rounded up to the next 4 KiB, so the block moves with the animation size. Little-endian layout:
 *   0x00  char[4]  magic "GPPR"
 *   0x04  u16      version (1)
 *   0x06  u16      header size (0x20)
 *   0x08  u16      record count (slot P01 = record 0, ...)
 *   0x0A  u16      record size (0x30E)
 *   0x0C  ...      reserved (0xFF)
 *   0x20  records, each a complete preset record with its own CRC; an erased (all 0xFF) record leaves that slot empty.
 * A slot shows its factory preset only while nothing has been saved there; saving replaces it, factory reset brings it back.
 */

#include "storage/preset_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_FACTORY_PRESETS_HEADER_BYTES 0x20u

/* Parse a block that starts at `block` (max_bytes readable). Returns the record count, or 0 if there is no valid block. */
uint16_t gp100_factory_presets_parse(const uint8_t *block, size_t max_bytes);

/* Copy the factory preset for `slot` out of a parsed block. False if the slot has none or the record is damaged. */
bool gp100_factory_presets_get(const uint8_t *block, size_t max_bytes, uint16_t slot, gp100_preset_record_t *out);

/* Same, reading the running firmware's flash. */
bool gp100_factory_preset_load(uint16_t slot, gp100_preset_record_t *out);

#endif
