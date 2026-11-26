#include "storage/settings_record.h"
#include "app/firmware_runtime.h"
#include "storage/preset_record.h"
#include "tuner/tuner_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 1;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 2;
    }
    *data = malloc((size_t)length);
    if (*data == NULL || fread(*data, 1u, (size_t)length, file) !=
                             (size_t)length) {
        free(*data);
        fclose(file);
        return 3;
    }
    fclose(file);
    *size = (size_t)length;
    return 0;
}

static int test_supplied_flash(const char *path)
{
    uint8_t *flash = NULL;
    size_t flash_size = 0u;
    gp100_settings_record_t record;
    gp100_sysex_global_state_t eq;
    size_t slot;
    size_t next_slot;
    int16_t value;
    int result;

    result = load_file(path, &flash, &flash_size);
    if (result != 0) {
        return result;
    }
    if (flash_size < GP100_SETTINGS_FLASH_BASE +
                         GP100_SETTINGS_FLASH_BYTES ||
        !gp100_settings_find_latest(
            flash + GP100_SETTINGS_FLASH_BASE,
            flash_size - GP100_SETTINGS_FLASH_BASE,
            &record, &slot, &next_slot)) {
        free(flash);
        return 4;
    }
    free(flash);

    if (slot != 193u || next_slot != 194u ||
        !gp100_settings_validate(&record)) {
        return 5;
    }
    if (!gp100_settings_get_i16(
            &record, GP100_SETTINGS_CURRENT_PRESET, &value) ||
        value != 0 ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_FOOTSWITCH_MODE, &value) ||
        value != 1 ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_DRUM_TEMPO, &value) ||
        value != 120 ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_TUNER_REFERENCE_HZ, &value) ||
        value != 440 ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_TUNER_MODE, &value) ||
        value != GP100_TUNER_OUTPUT_MUTE ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_MASTER_VOLUME, &value) ||
        value != 99) {
        return 6;
    }
    if (!gp100_settings_get_global_eq(&record, &eq) ||
        eq.parameters[GP100_GLOBAL_EQ_ENABLE] !=
            UINT32_C(0x3f800000) ||
        eq.parameters[GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY] !=
            UINT32_C(0x41980000) ||
        eq.parameters[GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY] !=
            UINT32_C(0x469c4200) ||
        eq.parameters[GP100_GLOBAL_EQ_BAND_1_FREQUENCY] !=
            UINT32_C(0x42980000)) {
        return 7;
    }
    if (gp100_settings_normalize(&record, 9u) !=
            GP100_SETTINGS_REPAIR_NONE) {
        return 8;
    }
    return 0;
}

static int test_update_and_fallback(void)
{
    uint8_t region[GP100_SETTINGS_FLASH_BYTES];
    gp100_settings_record_t first;
    gp100_settings_record_t second;
    gp100_settings_record_t loaded;
    gp100_sysex_global_state_t eq = {{0u}};
    gp100_sysex_global_state_t decoded;
    size_t slot;
    size_t next_slot;
    uint32_t address;
    int16_t value;

    memset(region, 0xff, sizeof(region));
    memset(&first, 0, sizeof(first));
    if (!gp100_settings_set_i16(
            &first, GP100_SETTINGS_INPUT_LEVEL_DB, -12)) {
        return 1;
    }
    eq.parameters[14] = UINT32_C(0x3f333333);
    if (!gp100_settings_set_global_eq(&first, &eq)) {
        return 2;
    }
    gp100_settings_update_crc(&first);
    memcpy(region, first.bytes, sizeof(first.bytes));

    second = first;
    if (!gp100_settings_set_i16(
            &second, GP100_SETTINGS_INPUT_LEVEL_DB, 6)) {
        return 3;
    }
    gp100_settings_update_crc(&second);
    memcpy(region + GP100_SETTINGS_RECORD_SIZE,
           second.bytes, sizeof(second.bytes));

    if (!gp100_settings_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 1u || next_slot != 2u ||
        !gp100_settings_get_i16(
            &loaded, GP100_SETTINGS_INPUT_LEVEL_DB, &value) ||
        value != 6 ||
        !gp100_settings_get_global_eq(&loaded, &decoded) ||
        decoded.parameters[14] != UINT32_C(0x3f333333)) {
        return 4;
    }

    region[GP100_SETTINGS_RECORD_SIZE + 7u] ^= 1u;
    if (!gp100_settings_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 1u ||
        !gp100_settings_get_i16(
            &loaded, GP100_SETTINGS_INPUT_LEVEL_DB, &value) ||
        value != -12) {
        return 5;
    }
    if (!gp100_settings_slot_address(278u, &address) ||
        address != UINT32_C(0x007f8f58) ||
        gp100_settings_slot_address(279u, &address)) {
        return 6;
    }
    return 0;
}

static int test_legacy_migration(void)
{
    uint8_t region[GP100_SETTINGS_FLASH_BYTES];
    gp100_settings_record_t record;
    gp100_sysex_global_state_t eq;
    gp100_settings_format_t format;
    size_t slot;
    size_t next_slot;
    uint8_t *legacy;
    uint32_t crc;
    int16_t value;

    memset(region, 0xff, sizeof(region));
    legacy = region + 3u * GP100_SETTINGS_LEGACY_48_RECORD_SIZE;
    legacy[GP100_SETTINGS_INPUT_LEVEL_DB] = 6u;
    legacy[GP100_SETTINGS_INPUT_LEVEL_DB + 1u] = 0u;
    crc = gp100_crc32(legacy, 0x44u);
    legacy[0x44u] = (uint8_t)crc;
    legacy[0x45u] = (uint8_t)(crc >> 8);
    legacy[0x46u] = (uint8_t)(crc >> 16);
    legacy[0x47u] = (uint8_t)(crc >> 24);
    if (!gp100_settings_find_latest_compatible(
            region, sizeof(region), &record, &format, &slot, &next_slot) ||
        format != GP100_SETTINGS_FORMAT_LEGACY_48 || slot != 3u ||
        next_slot != 0u || !gp100_settings_validate(&record) ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_INPUT_LEVEL_DB, &value) ||
        value != 6 || !gp100_settings_get_global_eq(&record, &eq) ||
        eq.parameters[GP100_GLOBAL_EQ_ENABLE] != 0u ||
        eq.parameters[GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY] !=
            UINT32_C(0x41980000) ||
        eq.parameters[GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY] !=
            UINT32_C(0x469c4200)) {
        return 1;
    }

    memset(region, 0xff, sizeof(region));
    legacy = region + 5u * GP100_SETTINGS_LEGACY_44_RECORD_SIZE;
    legacy[GP100_SETTINGS_MASTER_VOLUME] = 77u;
    legacy[GP100_SETTINGS_MASTER_VOLUME + 1u] = 0u;
    crc = gp100_crc32(legacy, 0x40u);
    legacy[0x40u] = (uint8_t)crc;
    legacy[0x41u] = (uint8_t)(crc >> 8);
    legacy[0x42u] = (uint8_t)(crc >> 16);
    legacy[0x43u] = (uint8_t)(crc >> 24);
    if (!gp100_settings_find_latest_compatible(
            region, sizeof(region), &record, &format, &slot, &next_slot) ||
        format != GP100_SETTINGS_FORMAT_LEGACY_44 || slot != 5u ||
        next_slot != 0u || !gp100_settings_validate(&record) ||
        !gp100_settings_get_i16(
            &record, GP100_SETTINGS_MASTER_VOLUME, &value) ||
        value != 77 || record.bytes[0x40u] != 0u ||
        record.bytes[0x41u] != 0u || record.bytes[0x42u] != 0u ||
        record.bytes[0x43u] != 0u) {
        return 2;
    }
    return 0;
}

static void write_test_i16(gp100_settings_record_t *record,
                           size_t offset, int16_t value)
{
    record->bytes[offset] = (uint8_t)value;
    record->bytes[offset + 1u] = (uint8_t)((uint16_t)value >> 8);
}

static int expect_i16(const gp100_settings_record_t *record,
                      gp100_settings_field_t field,
                      int16_t expected)
{
    int16_t actual;

    return !gp100_settings_get_i16(record, field, &actual) ||
           actual != expected;
}

static int test_factory_normalization(void)
{
    gp100_settings_record_t record;
    gp100_sysex_global_state_t equalizer;
    gp100_sysex_global_state_t factory_equalizer;
    uint32_t repairs;
    uint32_t expected_repairs =
        GP100_SETTINGS_REPAIR_CURRENT_PRESET |
        GP100_SETTINGS_REPAIR_FOOTSWITCH |
        GP100_SETTINGS_REPAIR_RHYTHM |
        GP100_SETTINGS_REPAIR_TUNER |
        GP100_SETTINGS_REPAIR_LANGUAGE |
        GP100_SETTINGS_REPAIR_LOOPER |
        GP100_SETTINGS_REPAIR_LEVELS |
        GP100_SETTINGS_REPAIR_USB |
        GP100_SETTINGS_REPAIR_TAP_SYNC |
        GP100_SETTINGS_REPAIR_OUTPUT |
        GP100_SETTINGS_REPAIR_REVISION |
        GP100_SETTINGS_REPAIR_GLOBAL_EQ |
        GP100_SETTINGS_REPAIR_RESERVED_FLAGS;
    size_t offset;

    if (!gp100_make_factory_settings(&record) ||
        !gp100_settings_get_global_eq(&record, &factory_equalizer)) {
        return 1;
    }
    write_test_i16(&record, GP100_SETTINGS_CURRENT_PRESET, 198);
    write_test_i16(&record, GP100_SETTINGS_FOOTSWITCH_MODE, 2);
    write_test_i16(&record, GP100_SETTINGS_DRUM_GENRE, 8);
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE, 101);
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 11);
    write_test_i16(&record, GP100_SETTINGS_DRUM_TEMPO, 9);
    write_test_i16(&record, GP100_SETTINGS_DRUM_VOLUME, 100);
    write_test_i16(&record, GP100_SETTINGS_TUNER_REFERENCE_HZ, 448);
    write_test_i16(&record, GP100_SETTINGS_TUNER_MODE, 3);
    write_test_i16(&record, GP100_SETTINGS_LANGUAGE_INDEX, 9);
    write_test_i16(&record, GP100_SETTINGS_LOOPER_POSITION, 2);
    write_test_i16(&record, 0x16u, 2);
    write_test_i16(&record, 0x18u, 2);
    write_test_i16(&record, GP100_SETTINGS_LOOP_RECORD_VOLUME, 100);
    write_test_i16(&record, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 100);
    write_test_i16(&record, GP100_SETTINGS_MASTER_VOLUME, 100);
    write_test_i16(&record, GP100_SETTINGS_INPUT_LEVEL_DB, 21);
    write_test_i16(&record, GP100_SETTINGS_USB_MONITOR_LEVEL_DB, 21);
    write_test_i16(&record, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 2);
    write_test_i16(&record, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 2);
    write_test_i16(&record, GP100_SETTINGS_USB_RECORD_LEVEL_DB, 21);
    write_test_i16(&record, GP100_SETTINGS_DATA_REVISION, 0);
    for (offset = GP100_SETTINGS_TAP_SYNC_PRE;
         offset <= GP100_SETTINGS_TAP_SYNC_REVERB; offset += 2u) {
        write_test_i16(&record, offset, -1);
    }
    write_test_i16(&record, GP100_SETTINGS_NO_CAB_LEFT, -1);
    write_test_i16(&record, GP100_SETTINGS_NO_CAB_RIGHT, -1);
    equalizer = factory_equalizer;
    equalizer.parameters[0] = UINT32_C(0x40000000); /* 2.0 */
    equalizer.parameters[1] = UINT32_C(0x41900000); /* 18.0 */
    equalizer.parameters[2] = UINT32_C(0x469c4400); /* 20002.0 */
    equalizer.parameters[3] = UINT32_C(0x41980000); /* 19.0 */
    equalizer.parameters[4] = UINT32_C(0x3d4ccccd); /* 0.05 */
    equalizer.parameters[5] = UINT32_C(0x41a80000); /* 21.0 */
    if (!gp100_settings_set_global_eq(&record, &equalizer)) {
        return 2;
    }

    repairs = gp100_settings_normalize(&record, 9u);
    if (repairs != expected_repairs || !gp100_settings_validate(&record)) {
        return 3;
    }
    if (expect_i16(&record, GP100_SETTINGS_FOOTSWITCH_MODE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_GENRE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_STYLE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_VOLUME, 99) ||
        expect_i16(&record, GP100_SETTINGS_TUNER_REFERENCE_HZ, 440) ||
        expect_i16(&record, GP100_SETTINGS_TUNER_MODE, 0) ||
        expect_i16(&record, GP100_SETTINGS_LANGUAGE_INDEX, 0) ||
        expect_i16(&record, GP100_SETTINGS_LOOPER_POSITION, 0) ||
        expect_i16(&record, GP100_SETTINGS_LOOP_RECORD_VOLUME, 99) ||
        expect_i16(&record, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 99) ||
        expect_i16(&record, GP100_SETTINGS_MASTER_VOLUME, 99) ||
        expect_i16(&record, GP100_SETTINGS_INPUT_LEVEL_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_USB_MONITOR_LEVEL_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 1) ||
        expect_i16(&record, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 1) ||
        expect_i16(&record, GP100_SETTINGS_USB_RECORD_LEVEL_DB, 20) ||
        expect_i16(&record, GP100_SETTINGS_DATA_REVISION, 18)) {
        return 4;
    }
    if (record.bytes[0x00u] != 0u || record.bytes[0x01u] != 0u ||
        record.bytes[0x16u] != 0u || record.bytes[0x18u] != 0u ||
        !gp100_settings_get_global_eq(&record, &equalizer) ||
        memcmp(&equalizer, &factory_equalizer, sizeof(equalizer)) != 0) {
        return 5;
    }
    for (offset = GP100_SETTINGS_TAP_SYNC_PRE;
         offset <= GP100_SETTINGS_NO_CAB_RIGHT; offset += 2u) {
        if (record.bytes[offset] != 0u || record.bytes[offset + 1u] != 0u) {
            return 6;
        }
    }
    if (gp100_settings_normalize(&record, 9u) !=
            GP100_SETTINGS_REPAIR_NONE ||
        !gp100_settings_validate(&record) ||
        gp100_settings_normalize(&record, 0u) !=
            GP100_SETTINGS_REPAIR_NONE) {
        return 7;
    }
    return 0;
}

static int test_normalization_boundaries(void)
{
    gp100_settings_record_t record;

    if (!gp100_make_factory_settings(&record)) {
        return 1;
    }
    if (expect_i16(&record, GP100_SETTINGS_LIMITER_ENABLED, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_THRESHOLD_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_CEILING_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC, 15)) {
        return 5;
    }
    write_test_i16(&record, GP100_SETTINGS_LIMITER_ENABLED, 2);
    write_test_i16(&record, GP100_SETTINGS_LIMITER_THRESHOLD_DB, 13);
    write_test_i16(&record, GP100_SETTINGS_LIMITER_CEILING_DB, 1);
    write_test_i16(&record, GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC, -1);
    if (gp100_settings_normalize(&record, 9u) != GP100_SETTINGS_REPAIR_LIMITER ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_ENABLED, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_THRESHOLD_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_CEILING_DB, 0) ||
        expect_i16(&record, GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC, 15)) {
        return 6;
    }
    write_test_i16(&record, GP100_SETTINGS_DRUM_GENRE, 5);
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 22);
    write_test_i16(&record, GP100_SETTINGS_DRUM_TEMPO, 250);
    write_test_i16(&record, GP100_SETTINGS_TUNER_REFERENCE_HZ, 432);
    write_test_i16(&record, GP100_SETTINGS_INPUT_LEVEL_DB, -20);
    if (gp100_settings_normalize(&record, 9u) !=
            GP100_SETTINGS_REPAIR_NONE) {
        return 2;
    }
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 23);
    write_test_i16(&record, GP100_SETTINGS_DRUM_TEMPO, 10);
    if (gp100_settings_normalize(&record, 9u) !=
            GP100_SETTINGS_REPAIR_RHYTHM ||
        expect_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_TEMPO, 10)) {
        return 3;
    }
    write_test_i16(&record, GP100_SETTINGS_DRUM_GENRE, 7);
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE, 100);
    write_test_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 10);
    if (gp100_settings_normalize(&record, 9u) != GP100_SETTINGS_REPAIR_RHYTHM ||
        expect_i16(&record, GP100_SETTINGS_DRUM_GENRE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_STYLE, 0) ||
        expect_i16(&record, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0) ||
        !gp100_settings_validate(&record)) {
        return 4;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s valeton_spi_flash.bin\n", argv[0]);
        return 2;
    }
    result = test_supplied_flash(argv[1]);
    if (result != 0) {
        fprintf(stderr, "supplied settings journal test failed: %d\n", result);
        return 10 + result;
    }
    result = test_update_and_fallback();
    if (result != 0) {
        fprintf(stderr, "settings update/fallback test failed: %d\n", result);
        return 30 + result;
    }
    result = test_legacy_migration();
    if (result != 0) {
        fprintf(stderr, "settings legacy-migration test failed: %d\n", result);
        return 70 + result;
    }
    result = test_factory_normalization();
    if (result != 0) {
        fprintf(stderr, "settings normalization test failed: %d\n", result);
        return 90 + result;
    }
    result = test_normalization_boundaries();
    if (result != 0) {
        fprintf(stderr, "settings normalization-boundary test failed: %d\n",
                result);
        return 110 + result;
    }
    return 0;
}
