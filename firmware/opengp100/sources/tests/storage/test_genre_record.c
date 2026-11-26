#include "storage/genre_record.h"
#include "storage/flash_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct test_flash {
    uint8_t bytes[GP100_GENRE_FLASH_BYTES];
    uint32_t erased[GP100_GENRE_SECTOR_COUNT];
    size_t erase_count;
    size_t write_count;
} test_flash_t;

static bool erase_sector(void *context, uint32_t address)
{
    test_flash_t *flash = context;
    size_t offset;

    if (flash == NULL || address < GP100_GENRE_FLASH_BASE ||
        address >= GP100_GENRE_FLASH_BASE +
                       GP100_GENRE_FLASH_BYTES ||
        flash->erase_count >= GP100_GENRE_SECTOR_COUNT) {
        return false;
    }
    offset = (size_t)(address - GP100_GENRE_FLASH_BASE);
    flash->erased[flash->erase_count++] = address;
    memset(flash->bytes + offset, 0xff, GP100_FLASH_SECTOR_SIZE);
    return true;
}

static bool write_record(void *context, uint32_t address,
                         const uint8_t *data, size_t size)
{
    test_flash_t *flash = context;
    size_t offset;
    size_t index;

    if (flash == NULL || data == NULL ||
        address < GP100_GENRE_FLASH_BASE) {
        return false;
    }
    offset = (size_t)(address - GP100_GENRE_FLASH_BASE);
    if (size == 0u || size > sizeof(flash->bytes) - offset) {
        return false;
    }
    for (index = 0u; index < size; ++index) {
        flash->bytes[offset + index] &= data[index];
    }
    ++flash->write_count;
    return true;
}

static void make_catalog(gp100_genre_catalog_t *catalog, uint8_t seed)
{
    size_t genre;
    size_t byte;

    for (genre = 0u; genre < GP100_PRESET_GENRE_COUNT; ++genre) {
        for (byte = 0u; byte < GP100_GENRE_NAME_SIZE; ++byte) {
            catalog->names[genre][byte] =
                (uint8_t)(seed + genre * 11u + byte);
        }
    }
}

static int test_codec_and_scan(void)
{
    uint8_t region[GP100_GENRE_FLASH_BYTES];
    gp100_genre_catalog_t first;
    gp100_genre_catalog_t second;
    gp100_genre_catalog_t loaded;
    gp100_genre_record_t record;
    size_t slot;
    size_t next_slot;
    uint32_t address;

    memset(region, 0xff, sizeof(region));
    make_catalog(&first, 3u);
    make_catalog(&second, 17u);
    if (!gp100_genre_record_encode(&first, &record) ||
        !gp100_genre_record_validate(&record) ||
        gp100_genre_record_stored_crc(&record) !=
            gp100_crc32(record.bytes,
                             GP100_GENRE_PAYLOAD_SIZE)) {
        return 1;
    }
    memcpy(region, record.bytes, sizeof(record.bytes));
    if (!gp100_genre_record_encode(&second, &record)) {
        return 2;
    }
    memcpy(region + 7u * GP100_GENRE_RECORD_SIZE,
           record.bytes, sizeof(record.bytes));
    if (!gp100_genre_record_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 7u || next_slot != 8u ||
        memcmp(&loaded, &second, sizeof(second)) != 0) {
        return 3;
    }
    region[7u * GP100_GENRE_RECORD_SIZE + 9u] ^= 1u;
    if (!gp100_genre_record_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 1u ||
        memcmp(&loaded, &first, sizeof(first)) != 0) {
        return 4;
    }
    if (!gp100_genre_record_slot_address(
            GP100_GENRE_SLOT_COUNT - 1u, &address) ||
        address != UINT32_C(0x007fdcc0) ||
        gp100_genre_record_slot_address(
            GP100_GENRE_SLOT_COUNT, &address)) {
        return 5;
    }
    memset(region, 0xff, sizeof(region));
    if (gp100_genre_record_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 0u) {
        return 6;
    }
    return 0;
}

static int test_writer(void)
{
    test_flash_t flash;
    gp100_genre_catalog_t catalog;
    gp100_genre_catalog_t loaded;
    gp100_genre_writer_t writer;
    const gp100_genre_writer_hooks_t hooks = {
        &flash, erase_sector, write_record
    };
    size_t slot;
    size_t next_slot;
    size_t sector;

    memset(&flash, 0xff, sizeof(flash));
    flash.erase_count = 0u;
    flash.write_count = 0u;
    make_catalog(&catalog, 29u);
    if (!gp100_genre_writer_init(&writer, 4u) ||
        gp100_genre_writer_append(&writer, &catalog, &hooks) !=
            GP100_GENRE_WRITER_WRITTEN ||
        writer.next_slot != 5u || flash.erase_count != 0u ||
        flash.write_count != 1u ||
        !gp100_genre_record_find_latest(
            flash.bytes, sizeof(flash.bytes), &loaded, &slot, &next_slot) ||
        slot != 4u || next_slot != 5u ||
        memcmp(&loaded, &catalog, sizeof(catalog)) != 0) {
        return 1;
    }

    memset(&flash, 0, sizeof(flash));
    flash.erase_count = 0u;
    if (!gp100_genre_writer_init(
            &writer, GP100_GENRE_SLOT_COUNT) ||
        gp100_genre_writer_append(&writer, &catalog, &hooks) !=
            GP100_GENRE_WRITER_WRITTEN ||
        writer.next_slot != 1u ||
        flash.erase_count != GP100_GENRE_SECTOR_COUNT ||
        flash.write_count != 1u) {
        return 2;
    }
    for (sector = 0u; sector < GP100_GENRE_SECTOR_COUNT; ++sector) {
        if (flash.erased[sector] != GP100_GENRE_FLASH_BASE +
                sector * GP100_FLASH_SECTOR_SIZE) {
            return 3;
        }
    }
    if (!gp100_genre_record_find_latest(
            flash.bytes, sizeof(flash.bytes), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 1u ||
        memcmp(&loaded, &catalog, sizeof(catalog)) != 0) {
        return 4;
    }
    return 0;
}

static int test_supplied_flash(const char *path)
{
    FILE *file = fopen(path, "rb");
    uint8_t region[GP100_GENRE_FLASH_BYTES];
    gp100_genre_catalog_t catalog;
    size_t slot;
    size_t next_slot;

    if (file == NULL ||
        fseek(file, (long)GP100_GENRE_FLASH_BASE, SEEK_SET) != 0 ||
        fread(region, 1u, sizeof(region), file) != sizeof(region)) {
        if (file != NULL) {
            fclose(file);
        }
        return 1;
    }
    fclose(file);
    if (!gp100_genre_record_find_latest(
            region, sizeof(region), &catalog, &slot, &next_slot) ||
        next_slot != slot + 1u ||
        memcmp(catalog.names[0], "Metal", sizeof("Metal")) != 0 ||
        memcmp(catalog.names[9], "Bass", sizeof("Bass")) != 0 ||
        memcmp(catalog.names[10], "Acoustic", sizeof("Acoustic")) != 0) {
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result = test_codec_and_scan();

    if (result != 0) {
        fprintf(stderr, "genre codec/scan test failed: %d\n", result);
        return 1;
    }
    result = test_writer();
    if (result != 0) {
        fprintf(stderr, "genre writer test failed: %d\n", result);
        return 1;
    }
    if (argc > 1) {
        result = test_supplied_flash(argv[1]);
        if (result != 0) {
            fprintf(stderr, "supplied genre region test failed: %d\n",
                    result);
            return 1;
        }
    }
    return 0;
}
