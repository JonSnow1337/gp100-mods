#include "preset_store.h"
#include "flash_commit.h"
#include "patch_chain.h"

#include <string.h>

#define PRESET_STORE_TAG UINT32_C(0x3150474f) /* OGP1 */
#define GROUP_PAYLOAD_BYTES \
    (GP100_PRESET_STORE_GROUP_SIZE * GP100_PRESET_RECORD_SIZE)
#define GROUP_SECTOR_BYTES (2u * GP100_FLASH_SECTOR_SIZE)

_Static_assert(GROUP_PAYLOAD_BYTES <=
    GP100_FLASH_SECTOR_SIZE - GP100_FLASH_PAGE_SIZE,
    "One sector must fit a preset group and its commit page");

static const uint8_t *latest_group(const uint8_t *group, uint32_t *generation)
{
    const uint8_t *second = group + GP100_FLASH_SECTOR_SIZE;
    uint32_t first_generation;
    uint32_t second_generation;
    const bool first_valid = gp100_flash_commit_read(
        group, GROUP_PAYLOAD_BYTES, PRESET_STORE_TAG, &first_generation);
    const bool second_valid = gp100_flash_commit_read(
        second, GROUP_PAYLOAD_BYTES, PRESET_STORE_TAG, &second_generation);

    if (second_valid && (!first_valid ||
            gp100_flash_generation_newer(second_generation, first_generation))) {
        *generation = second_generation;
        return second;
    }
    if (first_valid) {
        *generation = first_generation;
        return group;
    }
    return NULL;
}

bool gp100_preset_store_load(const uint8_t *region, const uint8_t *legacy,
                             uint16_t slot, gp100_preset_record_t *record)
{
    uint32_t generation;

    if (region == NULL || legacy == NULL || record == NULL ||
        slot >= GP100_PRESETS_PER_BANK) {
        return false;
    }
    const uint8_t *group = latest_group(region +
        (slot / GP100_PRESET_STORE_GROUP_SIZE) * GROUP_SECTOR_BYTES, &generation);
    const uint8_t *source = group == NULL
        ? legacy + slot * GP100_PRESET_RECORD_SIZE
        : group + (slot % GP100_PRESET_STORE_GROUP_SIZE) * GP100_PRESET_RECORD_SIZE;
    memcpy(record->bytes, source, sizeof(record->bytes));
    return gp100_patch_validate(record);
}

bool gp100_preset_store_save(const uint8_t *region, const uint8_t *legacy,
                             const gp100_flash_update_hooks_t *hooks,
                             uint8_t workspace[GP100_FLASH_SECTOR_SIZE],
                             uint16_t slot, const gp100_preset_record_t *record)
{
    uint32_t generation = 0u;

    if (region == NULL || legacy == NULL || hooks == NULL ||
        hooks->read == NULL || hooks->erase_sector == NULL ||
        hooks->program_page == NULL ||
        workspace == NULL || slot >= GP100_PRESETS_PER_BANK ||
        !gp100_patch_validate(record)) {
        return false;
    }
    const size_t group_index = slot / GP100_PRESET_STORE_GROUP_SIZE;
    const size_t group_offset = group_index * GROUP_SECTOR_BYTES;
    const uint8_t *first = region + group_offset;
    const uint8_t *current = latest_group(first, &generation);
    const size_t record_offset =
        (slot % GP100_PRESET_STORE_GROUP_SIZE) * GP100_PRESET_RECORD_SIZE;

    if (current == NULL) {
        const size_t legacy_offset = group_index * GROUP_PAYLOAD_BYTES;
        const size_t remaining = GP100_PRESET_LEGACY_BYTES - legacy_offset;
        const size_t length = remaining < GROUP_PAYLOAD_BYTES
            ? remaining : GROUP_PAYLOAD_BYTES;
        memset(workspace, UINT8_MAX, GROUP_PAYLOAD_BYTES);
        memcpy(workspace, legacy + legacy_offset, length);
    } else {
        if (memcmp(current + record_offset, record->bytes, sizeof(*record)) == 0) {
            return true;
        }
        memcpy(workspace, current, GROUP_PAYLOAD_BYTES);
    }
    memcpy(workspace + record_offset, record->bytes, sizeof(*record));

    const size_t target_offset = group_offset +
        (current == first ? GP100_FLASH_SECTOR_SIZE : 0u);
    const uint32_t address = GP100_PRESET_STORE_BASE + (uint32_t)target_offset;
    if (!gp100_flash_bytes_are_erased(region + target_offset, GP100_FLASH_SECTOR_SIZE) &&
        !hooks->erase_sector(hooks->context, address)) {
        return false;
    }
    return gp100_flash_commit_write(hooks, address, workspace, GROUP_PAYLOAD_BYTES,
                                    PRESET_STORE_TAG, generation + 1u);
}
