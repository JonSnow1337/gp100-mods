#include "settings_store.h"
#include "flash_commit.h"

#include <string.h>

#define SETTINGS_STORE_LEGACY_TAG UINT32_C(0x3153474f) /* OGS1 */
#define SETTINGS_STORE_TAG UINT32_C(0x3253474f) /* OGS2 */
#define SETTINGS_STORE_SLOT_COUNT \
    (GP100_SETTINGS_STORE_BYTES / GP100_SETTINGS_STORE_SLOT_BYTES)
#define SETTINGS_WRITE_DELAY_TICKS 10u

_Static_assert(GP100_SETTINGS_EXTENDED_RECORD_SIZE <= GP100_FLASH_PAGE_SIZE,
    "One page must fit the settings payload");
_Static_assert(GP100_SETTINGS_SECTOR_COUNT >= 2u,
    "Settings rollover needs a sector outside the current copy");

static size_t find_latest(const uint8_t *region, uint32_t *generation,
                          size_t *record_size)
{
    size_t latest = SETTINGS_STORE_SLOT_COUNT;

    for (size_t slot = 0u; slot < SETTINGS_STORE_SLOT_COUNT; ++slot) {
        const uint8_t *candidate = region + slot * GP100_SETTINGS_STORE_SLOT_BYTES;
        uint32_t sequence;
        size_t size = GP100_SETTINGS_EXTENDED_RECORD_SIZE;

        if (!gp100_flash_commit_read(candidate, size, SETTINGS_STORE_TAG, &sequence)) {
            size = GP100_SETTINGS_RECORD_SIZE;
            if (!gp100_flash_commit_read(candidate, size, SETTINGS_STORE_LEGACY_TAG,
                                         &sequence)) {
                continue;
            }
        }
        if (latest == SETTINGS_STORE_SLOT_COUNT ||
            gp100_flash_generation_newer(sequence, *generation)) {
            latest = slot;
            *generation = sequence;
            if (record_size != NULL) {
                *record_size = size;
            }
        }
    }
    return latest;
}

bool gp100_settings_store_load(const uint8_t *region, const uint8_t *legacy,
                               gp100_settings_record_t *record)
{
    uint32_t generation;
    size_t record_size;

    if (region == NULL || record == NULL) {
        return false;
    }
    const size_t latest = find_latest(region, &generation, &record_size);
    if (latest != SETTINGS_STORE_SLOT_COUNT) {
        memset(record, 0, sizeof(*record));
        memcpy(record->bytes, region + latest * GP100_SETTINGS_STORE_SLOT_BYTES,
               record_size);
        return true;
    }
    return gp100_settings_find_latest_compatible(legacy,
        GP100_SETTINGS_FLASH_BYTES, record, NULL, NULL, NULL);
}

static bool append_settings(const uint8_t *region,
                            const gp100_flash_update_hooks_t *hooks,
                            const gp100_settings_record_t *record)
{
    static const uint8_t default_limiter[GP100_SETTINGS_EXTENDED_RECORD_SIZE -
                                         GP100_SETTINGS_RECORD_SIZE] = {0};
    uint32_t generation = 0u;
    const size_t latest = find_latest(region, &generation, NULL);
    const bool has_current = latest != SETTINGS_STORE_SLOT_COUNT;
    const size_t sector_end = has_current
        ? (latest / GP100_SETTINGS_STORE_SLOTS_PER_SECTOR + 1u) *
            GP100_SETTINGS_STORE_SLOTS_PER_SECTOR
        : GP100_SETTINGS_STORE_SLOTS_PER_SECTOR;
    size_t slot = has_current ? latest + 1u : 0u;

    /* Skip torn slots until we can append a full pair of erased pages. */
    for (; slot < sector_end; ++slot) {
        if (gp100_flash_bytes_are_erased(
                region + slot * GP100_SETTINGS_STORE_SLOT_BYTES,
                GP100_SETTINGS_STORE_SLOT_BYTES)) {
            break;
        }
    }
    if (slot == sector_end) {
        /* Reclaim only the next sector. The current record remains intact. */
        slot = has_current ? sector_end % SETTINGS_STORE_SLOT_COUNT : 0u;
        const size_t offset = slot * GP100_SETTINGS_STORE_SLOT_BYTES;
        if (!gp100_flash_bytes_are_erased(region + offset, GP100_FLASH_SECTOR_SIZE) &&
            !hooks->erase_sector(hooks->context,
                                  GP100_SETTINGS_STORE_BASE + (uint32_t)offset)) {
            return false;
        }
    }
    /* Keep the original journal format when the limiter is at factory defaults. */
    const bool extended = memcmp(record->bytes + GP100_SETTINGS_RECORD_SIZE,
                                 default_limiter, sizeof(default_limiter)) != 0;
    return gp100_flash_commit_write(hooks, GP100_SETTINGS_STORE_BASE +
        (uint32_t)(slot * GP100_SETTINGS_STORE_SLOT_BYTES), record->bytes,
        extended ? GP100_SETTINGS_EXTENDED_RECORD_SIZE : GP100_SETTINGS_RECORD_SIZE,
        extended ? SETTINGS_STORE_TAG : SETTINGS_STORE_LEGACY_TAG, generation + 1u);
}

void gp100_settings_writer_mark_dirty(gp100_settings_writer_t *writer)
{
    writer->dirty_countdown = SETTINGS_WRITE_DELAY_TICKS;
}

gp100_settings_writer_result_t gp100_settings_writer_service(
    gp100_settings_writer_t *writer, gp100_settings_record_t *record,
    const uint8_t *region, const gp100_flash_update_hooks_t *hooks)
{
    if (writer == NULL || record == NULL || region == NULL || hooks == NULL ||
        hooks->read == NULL || hooks->erase_sector == NULL ||
        hooks->program_page == NULL || writer->dirty_countdown > SETTINGS_WRITE_DELAY_TICKS) {
        return GP100_SETTINGS_WRITER_INVALID;
    }
    if (writer->dirty_countdown == 0u) {
        return GP100_SETTINGS_WRITER_IDLE;
    }
    if (writer->dirty_countdown > 1u) {
        --writer->dirty_countdown;
        return GP100_SETTINGS_WRITER_WAITING;
    }

    gp100_settings_record_t next_record = *record;
    gp100_settings_update_crc(&next_record);
    if (!append_settings(region, hooks, &next_record)) {
        writer->dirty_countdown = SETTINGS_WRITE_DELAY_TICKS;
        return GP100_SETTINGS_WRITER_IO_ERROR;
    }
    *record = next_record;
    writer->dirty_countdown = 0u;
    return GP100_SETTINGS_WRITER_WRITTEN;
}
