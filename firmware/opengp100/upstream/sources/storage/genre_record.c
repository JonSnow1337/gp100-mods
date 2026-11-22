#include "genre_record.h"

#include "flash_update.h"

#include <string.h>

_Static_assert(sizeof(((gp100_genre_catalog_t *)0)->names) ==
                   GP100_GENRE_PAYLOAD_SIZE,
               "The complete genre catalog must be one journal payload");
_Static_assert((GP100_GENRE_SLOT_COUNT - 1u) *
                       GP100_GENRE_RECORD_SIZE == 0x4cc0u,
               "The last genre record must begin at 0x007fdcc0");

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

uint32_t gp100_genre_record_stored_crc(
    const gp100_genre_record_t *record)
{
    if (record == NULL) {
        return 0u;
    }
    return read_u32_le(record->bytes + GP100_GENRE_CRC_OFFSET);
}

bool gp100_genre_record_validate(
    const gp100_genre_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    return gp100_genre_record_stored_crc(record) ==
           gp100_crc32(record->bytes, GP100_GENRE_PAYLOAD_SIZE);
}

bool gp100_genre_record_encode(
    const gp100_genre_catalog_t *catalog,
    gp100_genre_record_t *record)
{
    uint32_t crc;

    if (catalog == NULL || record == NULL) {
        return false;
    }
    memcpy(record->bytes, catalog->names, sizeof(catalog->names));
    crc = gp100_crc32(record->bytes, GP100_GENRE_PAYLOAD_SIZE);
    write_u32_le(record->bytes + GP100_GENRE_CRC_OFFSET, crc);
    return true;
}

bool gp100_genre_record_decode(
    const gp100_genre_record_t *record,
    gp100_genre_catalog_t *catalog)
{
    if (record == NULL || catalog == NULL ||
        !gp100_genre_record_validate(record)) {
        return false;
    }
    memcpy(catalog->names, record->bytes, sizeof(catalog->names));
    return true;
}

bool gp100_genre_record_slot_address(size_t slot, uint32_t *address)
{
    if (slot >= GP100_GENRE_SLOT_COUNT || address == NULL) {
        return false;
    }
    *address = GP100_GENRE_FLASH_BASE +
               (uint32_t)(slot * GP100_GENRE_RECORD_SIZE);
    return true;
}

bool gp100_genre_record_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_genre_catalog_t *catalog,
    size_t *record_slot,
    size_t *next_slot)
{
    size_t slot;

    if (flash_region == NULL || catalog == NULL ||
        flash_region_size < GP100_GENRE_FLASH_BYTES) {
        return false;
    }
    for (slot = GP100_GENRE_SLOT_COUNT; slot != 0u; --slot) {
        gp100_genre_record_t candidate;
        size_t candidate_slot = slot - 1u;

        memcpy(candidate.bytes,
               flash_region + candidate_slot * GP100_GENRE_RECORD_SIZE,
               sizeof(candidate.bytes));
        if (gp100_genre_record_decode(&candidate, catalog)) {
            if (record_slot != NULL) {
                *record_slot = candidate_slot;
            }
            if (next_slot != NULL) {
                *next_slot = candidate_slot + 1u;
            }
            return true;
        }
    }
    if (record_slot != NULL) {
        *record_slot = 0u;
    }
    if (next_slot != NULL) {
        *next_slot = 0u;
    }
    return false;
}

bool gp100_genre_writer_init(
    gp100_genre_writer_t *writer,
    size_t next_slot)
{
    if (writer == NULL || next_slot > GP100_GENRE_SLOT_COUNT) {
        return false;
    }
    writer->next_slot = next_slot;
    return true;
}

gp100_genre_writer_result_t gp100_genre_writer_append(
    gp100_genre_writer_t *writer,
    const gp100_genre_catalog_t *catalog,
    const gp100_genre_writer_hooks_t *hooks)
{
    gp100_genre_record_t record;
    uint32_t address;
    size_t write_slot;
    size_t sector;

    if (writer == NULL || catalog == NULL || hooks == NULL ||
        hooks->erase_sector == NULL || hooks->write == NULL ||
        writer->next_slot > GP100_GENRE_SLOT_COUNT ||
        !gp100_genre_record_encode(catalog, &record)) {
        return GP100_GENRE_WRITER_INVALID;
    }
    write_slot = writer->next_slot;
    if (write_slot == GP100_GENRE_SLOT_COUNT) {
        for (sector = 0u; sector < GP100_GENRE_SECTOR_COUNT; ++sector) {
            address = GP100_GENRE_FLASH_BASE +
                (uint32_t)(sector * GP100_FLASH_SECTOR_SIZE);
            if (!hooks->erase_sector(hooks->context, address)) {
                return GP100_GENRE_WRITER_IO_ERROR;
            }
        }
        write_slot = 0u;
    }
    if (!gp100_genre_record_slot_address(
            write_slot, &address) ||
        !hooks->write(hooks->context, address,
                      record.bytes, sizeof(record.bytes))) {
        return GP100_GENRE_WRITER_IO_ERROR;
    }
    writer->next_slot = write_slot + 1u;
    return GP100_GENRE_WRITER_WRITTEN;
}
