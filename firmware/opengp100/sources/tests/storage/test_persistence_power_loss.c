#include "storage/patch_chain.h"
#include "storage/flash_commit.h"
#include "storage/preset_store.h"
#include "storage/settings_store.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define USER_BYTES (0x00800000u - GP100_USER_DATA_START)

typedef enum cut_phase {
    CUT_BEFORE,
    CUT_DURING,
    CUT_AFTER,
    CORRUPT_WRITE
} cut_phase_t;

typedef struct simulated_flash {
    uint8_t bytes[USER_BYTES];
    size_t operations;
    size_t erase_count;
    size_t cut_at;
    cut_phase_t phase;
    bool power_lost;
} simulated_flash_t;

static simulated_flash_t flash;
static uint8_t checkpoint[USER_BYTES];
static size_t power_cuts;

static uint8_t *at(uint32_t address)
{
    assert(address >= GP100_USER_DATA_START &&
           address < GP100_USER_DATA_START + USER_BYTES);
    return flash.bytes + (address - GP100_USER_DATA_START);
}

static bool read_flash(void *context, uint32_t address, uint8_t *data,
                       size_t size)
{
    assert(context == &flash && !flash.power_lost);
    memcpy(data, at(address), size);
    return true;
}

static bool erase_sector(void *context, uint32_t address)
{
    assert(context == &flash && !flash.power_lost);
    assert(address % GP100_FLASH_SECTOR_SIZE == 0u);
    assert((address >= GP100_PRESET_STORE_BASE &&
            address < GP100_PRESET_STORE_BASE + GP100_PRESET_STORE_BYTES) ||
           (address >= GP100_SETTINGS_STORE_BASE &&
            address < GP100_SETTINGS_STORE_BASE + GP100_SETTINGS_STORE_BYTES));
    ++flash.erase_count;
    const bool cut = ++flash.operations == flash.cut_at;
    if (!cut || flash.phase != CUT_BEFORE) {
        const size_t length = cut && flash.phase == CUT_DURING
            ? GP100_FLASH_SECTOR_SIZE / 2u : GP100_FLASH_SECTOR_SIZE;
        memset(at(address), 0xff, length);
    }
    flash.power_lost = cut;
    return !cut;
}

static bool program_page(void *context, uint32_t address, const uint8_t *data,
                         size_t size)
{
    assert(context == &flash && !flash.power_lost);
    assert(address % GP100_FLASH_PAGE_SIZE == 0u && size == GP100_FLASH_PAGE_SIZE);
    assert((address >= GP100_PRESET_STORE_BASE &&
            address < GP100_PRESET_STORE_BASE + GP100_PRESET_STORE_BYTES) ||
           (address >= GP100_SETTINGS_STORE_BASE &&
            address < GP100_SETTINGS_STORE_BASE + GP100_SETTINGS_STORE_BYTES));
    uint8_t *page = at(address);
    assert(gp100_flash_bytes_are_erased(page, size));
    const bool cut = ++flash.operations == flash.cut_at;

    if (!cut || flash.phase != CUT_BEFORE) {
        const size_t stride = cut && (flash.phase == CUT_DURING ||
                                     flash.phase == CORRUPT_WRITE) ? 2u : 1u;
        for (size_t i = 0u; i < size; i += stride) {
            page[i] &= data[i];
        }
    }
    flash.power_lost = cut && flash.phase != CORRUPT_WRITE;
    return !flash.power_lost;
}

static const gp100_flash_update_hooks_t hooks = {
    &flash, read_flash, erase_sector, program_page
};

static void reset_controller(size_t cut_at, cut_phase_t phase)
{
    flash.operations = 0u;
    flash.erase_count = 0u;
    flash.cut_at = cut_at;
    flash.phase = phase;
    flash.power_lost = false;
}

static void from_checkpoint(size_t cut_at, cut_phase_t phase)
{
    memcpy(flash.bytes, checkpoint, sizeof(checkpoint));
    reset_controller(cut_at, phase);
}

static gp100_preset_record_t make_preset(const char *name)
{
    gp100_preset_record_t record = {0};
    gp100_patch_init(&record);
    assert(gp100_preset_set_name(&record, name));
    gp100_preset_update_crc(&record);
    assert(gp100_patch_validate(&record));
    return record;
}

static bool save_preset(uint16_t slot, const gp100_preset_record_t *record)
{
    uint8_t workspace[GP100_FLASH_SECTOR_SIZE];
    return gp100_preset_store_save(at(GP100_PRESET_STORE_BASE),
        at(GP100_USER_PRESET_BASE), &hooks, workspace, slot, record);
}

static bool load_preset(uint16_t slot, gp100_preset_record_t *record)
{
    return gp100_preset_store_load(at(GP100_PRESET_STORE_BASE),
        at(GP100_USER_PRESET_BASE), slot, record);
}

static void expect_preset(uint16_t slot, const gp100_preset_record_t *expected)
{
    gp100_preset_record_t actual;
    assert(load_preset(slot, &actual));
    assert(memcmp(&actual, expected, sizeof(actual)) == 0);
}

static void check_legacy_presets(void)
{
    assert(memcmp(at(GP100_USER_PRESET_BASE),
        checkpoint + GP100_USER_PRESET_BASE - GP100_USER_DATA_START,
        GP100_PRESET_LEGACY_BYTES) == 0);
}

static void cut_preset_save(uint16_t slot, const gp100_preset_record_t *previous,
                            const gp100_preset_record_t *next, size_t erases)
{
    memcpy(checkpoint, flash.bytes, sizeof(checkpoint));
    reset_controller(0u, CUT_BEFORE);
    assert(save_preset(slot, next));
    assert(flash.erase_count == erases);
    const size_t steps = flash.operations;
    assert(steps >= 2u);

    gp100_preset_record_t neighbors[GP100_PRESET_STORE_GROUP_SIZE];
    bool present[GP100_PRESET_STORE_GROUP_SIZE];
    const uint16_t first_slot = slot - slot % GP100_PRESET_STORE_GROUP_SIZE;
    for (size_t i = 0u; i < GP100_PRESET_STORE_GROUP_SIZE; ++i) {
        present[i] = first_slot + i != slot &&
            load_preset((uint16_t)(first_slot + i), &neighbors[i]);
    }

    for (size_t step = 1u; step <= steps; ++step) {
        for (cut_phase_t phase = CUT_BEFORE; phase <= CUT_AFTER; ++phase) {
            from_checkpoint(step, phase);
            assert(!save_preset(slot, next) && flash.power_lost);
            ++power_cuts;
            reset_controller(0u, CUT_BEFORE);
            expect_preset(slot, step == steps && phase == CUT_AFTER ? next : previous);
            for (size_t i = 0u; i < GP100_PRESET_STORE_GROUP_SIZE; ++i) {
                if (present[i]) {
                    expect_preset((uint16_t)(first_slot + i), &neighbors[i]);
                }
            }
            check_legacy_presets();
            assert(save_preset(slot, next));
            expect_preset(slot, next);
        }
    }
    /* A successful write call with corrupt readback cannot publish the entry. */
    from_checkpoint(erases + 1u, CORRUPT_WRITE);
    assert(!save_preset(slot, next) && !flash.power_lost);
    assert(flash.operations == erases + 1u);
    expect_preset(slot, previous);
    from_checkpoint(0u, CUT_BEFORE);
    assert(save_preset(slot, next));
}

static void test_presets(void)
{
    const gp100_preset_record_t original = make_preset("Original");
    const gp100_preset_record_t neighbor = make_preset("Neighbor");
    const gp100_preset_record_t latest = make_preset("Latest");
    const gp100_preset_record_t next = make_preset("Next");
    const gp100_preset_record_t last = make_preset("Last");
    const uint16_t last_slot = GP100_PRESETS_PER_BANK - 1u;
    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    memcpy(at(GP100_USER_PRESET_BASE), &original, sizeof(original));
    memcpy(at(GP100_USER_PRESET_BASE) + GP100_PRESET_RECORD_SIZE,
           &neighbor, sizeof(neighbor));
    memcpy(at(GP100_USER_PRESET_BASE) + last_slot * GP100_PRESET_RECORD_SIZE,
           &last, sizeof(last));

    expect_preset(0u, &original);
    cut_preset_save(0u, &original, &latest, 0u);
    cut_preset_save(0u, &latest, &next, 0u);
    cut_preset_save(0u, &next, &latest, 1u);
    cut_preset_save(last_slot, &last, &next, 0u);
    expect_preset(1u, &neighbor);
    const size_t count = flash.operations;
    assert(save_preset(last_slot, &next) && flash.operations == count);
}

static bool load_settings(gp100_settings_record_t *record)
{
    return gp100_settings_store_load(at(GP100_SETTINGS_STORE_BASE),
                                    at(GP100_SETTINGS_FLASH_BASE), record);
}

static void expect_settings(const gp100_settings_record_t *expected)
{
    gp100_settings_record_t actual;
    assert(load_settings(&actual));
    assert(memcmp(&actual, expected, sizeof(actual)) == 0);
}

static gp100_settings_writer_result_t write_settings(
    gp100_settings_record_t *record)
{
    gp100_settings_writer_t writer = {0};
    const uint8_t *region = at(GP100_SETTINGS_STORE_BASE);
    assert(gp100_settings_writer_service(&writer, record, region, &hooks) ==
           GP100_SETTINGS_WRITER_IDLE);
    gp100_settings_writer_mark_dirty(&writer);
    for (size_t tick = 0u; tick < 9u; ++tick) {
        assert(gp100_settings_writer_service(&writer, record, region, &hooks) ==
               GP100_SETTINGS_WRITER_WAITING);
    }
    return gp100_settings_writer_service(&writer, record, region, &hooks);
}

static void set_preset_selection(gp100_settings_record_t *record, int16_t slot)
{
    assert(gp100_settings_set_i16(record, GP100_SETTINGS_CURRENT_PRESET, slot));
    gp100_settings_update_crc(record);
}

static void cut_settings_save(const gp100_settings_record_t *previous,
                              gp100_settings_record_t *next, size_t erases)
{
    memcpy(checkpoint, flash.bytes, sizeof(checkpoint));
    reset_controller(0u, CUT_BEFORE);
    assert(write_settings(next) == GP100_SETTINGS_WRITER_WRITTEN);
    assert(flash.erase_count == erases && flash.operations == erases + 2u);
    const size_t steps = flash.operations;

    for (size_t step = 1u; step <= steps; ++step) {
        for (cut_phase_t phase = CUT_BEFORE; phase <= CUT_AFTER; ++phase) {
            from_checkpoint(step, phase);
            assert(write_settings(next) == GP100_SETTINGS_WRITER_IO_ERROR);
            assert(flash.power_lost);
            ++power_cuts;
            reset_controller(0u, CUT_BEFORE);
            expect_settings(step == steps && phase == CUT_AFTER ? next : previous);
            assert(memcmp(at(GP100_SETTINGS_FLASH_BASE),
                checkpoint + GP100_SETTINGS_FLASH_BASE - GP100_USER_DATA_START,
                GP100_SETTINGS_FLASH_BYTES) == 0);
            assert(write_settings(next) == GP100_SETTINGS_WRITER_WRITTEN);
            expect_settings(next);
        }
    }
    from_checkpoint(erases + 1u, CORRUPT_WRITE);
    assert(write_settings(next) == GP100_SETTINGS_WRITER_IO_ERROR);
    assert(!flash.power_lost && flash.operations == erases + 1u);
    expect_settings(previous);
    from_checkpoint(0u, CUT_BEFORE);
    assert(write_settings(next) == GP100_SETTINGS_WRITER_WRITTEN);
}

static void test_settings(void)
{
    gp100_settings_record_t legacy = {0};
    gp100_settings_record_t current;
    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    assert(gp100_settings_set_i16(&legacy, GP100_SETTINGS_MASTER_VOLUME, 70));
    set_preset_selection(&legacy, 3);
    memcpy(at(GP100_SETTINGS_FLASH_BASE) +
               (GP100_SETTINGS_SLOT_COUNT - 1u) * GP100_SETTINGS_RECORD_SIZE,
           legacy.bytes, GP100_SETTINGS_RECORD_SIZE);
    expect_settings(&legacy);

    /* Simulate an OpenGP100 journal created before the limiter existed. */
    reset_controller(0u, CUT_BEFORE);
    set_preset_selection(&legacy, 4);
    assert(gp100_flash_commit_write(&hooks, GP100_SETTINGS_STORE_BASE, legacy.bytes,
           GP100_SETTINGS_RECORD_SIZE, UINT32_C(0x3153474f), 1u));
    expect_settings(&legacy);

    current = legacy;
    assert(gp100_settings_set_i16(&current, GP100_SETTINGS_LIMITER_ENABLED, 1));
    assert(gp100_settings_set_i16(&current, GP100_SETTINGS_LIMITER_THRESHOLD_DB, 12));
    assert(gp100_settings_set_i16(&current, GP100_SETTINGS_LIMITER_CEILING_DB, -1));
    assert(gp100_settings_set_i16(&current, GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC, 33));
    assert(!gp100_settings_equal(&current, &legacy));
    cut_settings_save(&legacy, &current, 0u);
    uint8_t *extension = at(GP100_SETTINGS_STORE_BASE + GP100_SETTINGS_STORE_SLOT_BYTES +
                            GP100_SETTINGS_RECORD_SIZE);
    *extension ^= 1u;
    expect_settings(&legacy);
    *extension ^= 1u;
    expect_settings(&current);
    const size_t slots = GP100_SETTINGS_STORE_BYTES / GP100_SETTINGS_STORE_SLOT_BYTES;
    for (size_t slot = 2u; slot < slots; ++slot) {
        set_preset_selection(&current, (int16_t)(slot + 4u));
        reset_controller(0u, CUT_BEFORE);
        assert(write_settings(&current) == GP100_SETTINGS_WRITER_WRITTEN);
        assert(flash.operations == 2u && flash.erase_count == 0u);
    }
    const gp100_settings_record_t previous = current;
    set_preset_selection(&current, 0);
    cut_settings_save(&previous, &current, 1u);
    expect_settings(&current);
    assert(memcmp(at(GP100_SETTINGS_STORE_BASE) + GP100_FLASH_SECTOR_SIZE,
                  checkpoint + GP100_SETTINGS_STORE_BASE - GP100_USER_DATA_START +
                      GP100_FLASH_SECTOR_SIZE,
                  GP100_SETTINGS_STORE_BYTES - GP100_FLASH_SECTOR_SIZE) == 0);
}

static void test_drum_settings_checkpoint(void)
{
    gp100_settings_record_t saved = {0};
    gp100_settings_record_t live = {0};
    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    reset_controller(0u, CUT_BEFORE);

    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_GENRE, 1));
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_STYLE, 12));
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 1));
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_TEMPO, 120));
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_VOLUME, 51));
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_MASTER_VOLUME, 70));
    set_preset_selection(&live, 3);

    gp100_settings_record_t background = live;
    assert(gp100_settings_copy_drums(&background, &saved));
    assert(!gp100_settings_copy_drums(&background, &saved));
    assert(write_settings(&background) == GP100_SETTINGS_WRITER_WRITTEN);
    expect_settings(&background);
    int16_t value;
    assert(gp100_settings_get_i16(&background, GP100_SETTINGS_CURRENT_PRESET, &value) && value == 3);
    assert(gp100_settings_get_i16(&background, GP100_SETTINGS_MASTER_VOLUME, &value) && value == 70);
    assert(gp100_settings_get_i16(&background, GP100_SETTINGS_DRUM_VOLUME, &value) && value == 0);

    assert(gp100_settings_copy_drums(&background, &live));
    assert(!gp100_settings_copy_drums(&background, &live));
    assert(write_settings(&background) == GP100_SETTINGS_WRITER_WRITTEN);
    expect_settings(&live);
    saved = background;
    assert(gp100_settings_set_i16(&live, GP100_SETTINGS_DRUM_VOLUME, 47));
    expect_settings(&saved);
    assert(gp100_settings_copy_drums(&background, &live));
    assert(write_settings(&background) == GP100_SETTINGS_WRITER_WRITTEN);
    gp100_settings_update_crc(&live);
    expect_settings(&live);
    assert(!gp100_settings_copy_drums(&background, &live));
}

static void test_device_backup(const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fread(flash.bytes, 1u, sizeof(flash.bytes), file) == sizeof(flash.bytes));
    assert(fgetc(file) == EOF && fclose(file) == 0);
    gp100_preset_record_t original;
    assert(load_preset(0u, &original));
    assert(memcmp(&original, at(GP100_USER_PRESET_BASE), sizeof(original)) == 0);
    gp100_preset_record_t edited = original;
    assert(gp100_preset_set_name(&edited, "Cut Test"));
    gp100_preset_update_crc(&edited);
    cut_preset_save(0u, &original, &edited, 0u);
    puts("Verified saved preset from device backup and simulated its first save.");
}

int main(int argc, char **argv)
{
    assert(argc == 1 || argc == 2);
    test_presets();
    test_settings();
    test_drum_settings_checkpoint();
    if (argc == 2) {
        test_device_backup(argv[1]);
    }
    printf("Passed %zu simulated power cuts plus readback errors.\n", power_cuts);
    return 0;
}
