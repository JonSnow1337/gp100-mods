#ifndef GP100_PRESET_STORE_H
#define GP100_PRESET_STORE_H

#include "flash_update.h"
#include "preset_record.h"

#define GP100_PRESET_STORE_GROUP_SIZE 4u
#define GP100_PRESET_STORE_GROUP_COUNT \
    ((GP100_PRESETS_PER_BANK + GP100_PRESET_STORE_GROUP_SIZE - 1u) / \
     GP100_PRESET_STORE_GROUP_SIZE)
#define GP100_PRESET_STORE_BYTES \
    (GP100_PRESET_STORE_GROUP_COUNT * 2u * GP100_FLASH_SECTOR_SIZE)
#define GP100_PRESET_LEGACY_BYTES \
    (GP100_PRESETS_PER_BANK * GP100_PRESET_RECORD_SIZE)

/* Region pointers are mapped flash, beginning at the respective store bases. */
bool gp100_preset_store_load(const uint8_t *region, const uint8_t *legacy,
                             uint16_t slot, gp100_preset_record_t *record);
bool gp100_preset_store_save(const uint8_t *region, const uint8_t *legacy,
                             const gp100_flash_update_hooks_t *hooks,
                             uint8_t workspace[GP100_FLASH_SECTOR_SIZE],
                             uint16_t slot, const gp100_preset_record_t *record);

#endif
