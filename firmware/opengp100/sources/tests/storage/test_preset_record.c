#include "storage/preset_record.h"
#include "tests/fixtures/flash_layout.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLASH_SIZE (8u * 1024u * 1024u)
#define PRESET_AUTHOR_OFFSET 0x2c2u
#define PRESET_NOTES_OFFSET  0x2d6u
#define PRESET_METADATA_END  0x308u

static int read_flash(const char *path, uint8_t **flash)
{
    FILE *file = fopen(path, "rb");
    size_t size;
    if (file == NULL) {
        return 1;
    }
    *flash = malloc(FLASH_SIZE);
    if (*flash == NULL) {
        fclose(file);
        return 2;
    }
    size = fread(*flash, 1u, FLASH_SIZE, file);
    fclose(file);
    return size == FLASH_SIZE ? 0 : 3;
}

static int test_factory_record(const uint8_t *flash)
{
    static const uint32_t algorithms[GP100_EFFECT_FAMILY_COUNT] = {
        0x05000008u, 0x03000000u, 0x0700006eu,
        0x0000001du, 0x0a00002cu, 0x0100003cu,
        0x04000000u, 0x0b000000u, 0x0c000001u
    };
    gp100_preset_record_t record;
    gp100_effect_family_t family;
    char name[GP100_PRESET_NAME_SIZE + 1u];
    uint32_t algorithm;
    float parameter;
    bool enabled;
    gp100_control_target_t target;
    gp100_expression_pedal_target_t pedal_target;
    size_t index;

    memcpy(record.bytes, &flash[GP100_REFERENCE_FACTORY_PRESET_BASE],
           sizeof(record.bytes));
    if (gp100_preset_validate(&record) != GP100_PRESET_VALID ||
        gp100_preset_stored_crc(&record) != UINT32_C(0x871c035a)) {
        return 1;
    }
    gp100_preset_name(&record, name);
    if (gp100_preset_number(&record) != 0u ||
        strcmp(name, "It's GP100") != 0) {
        return 2;
    }
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        size_t position;
        if (!gp100_preset_effect_position(
                &record, (gp100_effect_family_t)index, &position) ||
            position != index ||
            !gp100_preset_effect_at_position(&record, index, &family) ||
            (size_t)family != index) {
            return 3;
        }
        if (!gp100_preset_algorithm_id(&record, family, &algorithm) ||
            algorithm != algorithms[index]) {
            return 4;
        }
    }
    if (!gp100_preset_parameter(&record, GP100_EFFECT_PRE_FX,
                                     0u, &parameter) ||
        fabsf(parameter - 50.0f) > 0.001f) {
        return 5;
    }
    if (!gp100_preset_effect_enabled(&record,
                                          GP100_EFFECT_AMPLIFIER,
                                          &enabled) || !enabled) {
        return 6;
    }
    if (gp100_preset_tempo_bpm(&record) != 120u ||
        gp100_preset_level(&record) != 50u ||
        gp100_preset_schema_version(&record) != 0u) {
        return 7;
    }
    if (!gp100_preset_control_target(&record, 0u, &target) ||
        !target.assigned || target.family != GP100_EFFECT_PRE_FX ||
        target.algorithm_id != UINT32_C(0x05000008) ||
        target.parameter_index != 0u ||
        !gp100_preset_control_target(&record, 1u, &target) ||
        target.assigned ||
        !gp100_preset_control_target(&record, 2u, &target) ||
        target.assigned ||
        gp100_preset_control_target(&record,
                                         GP100_CONTROL_TARGET_COUNT,
                                         &target)) {
        return 8;
    }
    if (!gp100_preset_expression_pedal_target(
            &record, &pedal_target) ||
        pedal_target != GP100_EXPRESSION_PEDAL_VOLUME ||
        !gp100_preset_set_expression_pedal_target(
            &record, GP100_EXPRESSION_PEDAL_EFFECT) ||
        !gp100_preset_expression_pedal_target(
            &record, &pedal_target) ||
        pedal_target != GP100_EXPRESSION_PEDAL_EFFECT ||
        gp100_preset_set_expression_pedal_target(
            &record, (gp100_expression_pedal_target_t)2)) {
        return 9;
    }
    return 0;
}

static int test_user_and_erased_records(const uint8_t *flash)
{
    gp100_preset_record_t record;
    char name[GP100_PRESET_NAME_SIZE + 1u];
    float minimum;
    float maximum;
    char author[GP100_PRESET_AUTHOR_SIZE + 1u];
    char notes[GP100_PRESET_NOTES_SIZE + 1u];
    gp100_control_range_t range;

    memcpy(record.bytes, &flash[GP100_USER_PRESET_BASE],
           sizeof(record.bytes));
    if (gp100_preset_validate(&record) != GP100_PRESET_VALID) {
        return 1;
    }
    gp100_preset_name(&record, name);
    if (strcmp(name, "Bass Base") != 0 ||
        gp100_preset_schema_version(&record) != 0x12u) {
        return 2;
    }
    for (range = GP100_CONTROL_RANGE_VOLUME;
         (unsigned)range < GP100_CONTROL_RANGE_COUNT;
         range = (gp100_control_range_t)((unsigned)range + 1u)) {
        if (!gp100_preset_control_range(
                &record, range, &minimum, &maximum) ||
            minimum != 0.0f || maximum != 99.0f) {
            return 3;
        }
    }
    if (gp100_preset_control_range(
            &record, (gp100_control_range_t)-1,
            &minimum, &maximum) ||
        gp100_preset_control_range(
            &record, (gp100_control_range_t)GP100_CONTROL_RANGE_COUNT,
            &minimum, &maximum)) {
        return 4;
    }
    if (gp100_preset_legacy_cabinet_id(&record) != 56u ||
        gp100_preset_genre_index(&record) != 4u) {
        return 5;
    }
    gp100_preset_author(&record, author);
    gp100_preset_notes(&record, notes);
    if (author[0] != '\0' || notes[0] != '\0') {
        return 6;
    }
    memset(&record.bytes[PRESET_AUTHOR_OFFSET], 'A',
           GP100_PRESET_AUTHOR_SIZE);
    memset(&record.bytes[PRESET_NOTES_OFFSET], 'N',
           GP100_PRESET_NOTES_SIZE);
    gp100_preset_author(&record, author);
    gp100_preset_notes(&record, notes);
    if (strlen(author) != GP100_PRESET_AUTHOR_SIZE ||
        strlen(notes) != GP100_PRESET_NOTES_SIZE ||
        author[GP100_PRESET_AUTHOR_SIZE - 1u] != 'A' ||
        notes[GP100_PRESET_NOTES_SIZE - 1u] != 'N') {
        return 7;
    }

    memcpy(record.bytes,
           &flash[GP100_USER_PRESET_BASE + GP100_PRESET_RECORD_SIZE],
           sizeof(record.bytes));
    if (!gp100_preset_is_erased(&record) ||
        gp100_preset_validate(&record) != GP100_PRESET_ERASED) {
        return 8;
    }
    return 0;
}

static int test_crc_repair_and_addresses(const uint8_t *flash)
{
    gp100_preset_record_t record;
    uint32_t address;

    if (!gp100_preset_flash_address(0u, &address) ||
        address != GP100_USER_PRESET_BASE) {
        return 1;
    }
    if (!gp100_preset_flash_address(98u, &address) ||
        address != GP100_USER_PRESET_BASE + 98u * 0x30eu) {
        return 2;
    }
    if (gp100_preset_flash_address(99u, &address)) {
        return 3;
    }
    if (gp100_preset_flash_address(197u, &address)) {
        return 4;
    }
    if (gp100_preset_flash_address(198u, &address)) {
        return 5;
    }

    memcpy(record.bytes, &flash[GP100_REFERENCE_FACTORY_PRESET_BASE],
           sizeof(record.bytes));
    record.bytes[2] ^= 1u;
    if (gp100_preset_validate(&record) != GP100_PRESET_BAD_CRC) {
        return 6;
    }
    gp100_preset_update_crc(&record);
    if (gp100_preset_validate(&record) != GP100_PRESET_VALID) {
        return 7;
    }
    return 0;
}

static int test_complete_banks(const uint8_t *flash)
{
    gp100_preset_record_t record;
    gp100_effect_family_t family;
    bool seen[GP100_EFFECT_FAMILY_COUNT];
    bool enabled;
    size_t record_index;
    size_t chain_index;
    size_t byte_index;

    for (record_index = 0u;
         record_index < GP100_PRESETS_PER_BANK;
         ++record_index) {
        memcpy(record.bytes,
               &flash[GP100_REFERENCE_FACTORY_PRESET_BASE +
                      record_index * GP100_PRESET_RECORD_SIZE],
               sizeof(record.bytes));
        if (gp100_preset_validate(&record) != GP100_PRESET_VALID ||
            gp100_preset_number(&record) != record_index) {
            return 1;
        }
        memset(seen, 0, sizeof(seen));
        for (chain_index = 0u;
             chain_index < GP100_EFFECT_FAMILY_COUNT;
             ++chain_index) {
            size_t position;
            if (!gp100_preset_effect_position(
                    &record, (gp100_effect_family_t)chain_index,
                    &position) ||
                position >= GP100_EFFECT_FAMILY_COUNT || seen[position] ||
                !gp100_preset_effect_at_position(
                    &record, position, &family) ||
                (size_t)family != chain_index) {
                return 2;
            }
            seen[position] = true;
            if (!gp100_preset_effect_enabled(&record, family, &enabled)) {
                return 3;
            }
        }
        if (gp100_preset_tempo_bpm(&record) < 40u ||
            gp100_preset_tempo_bpm(&record) > 250u ||
            gp100_preset_level(&record) > 99u ||
            gp100_preset_schema_version(&record) != 0u) {
            return 4;
        }
        for (byte_index = PRESET_AUTHOR_OFFSET;
             byte_index < PRESET_METADATA_END;
             ++byte_index) {
            if (record.bytes[byte_index] != 0u) {
                return 5;
            }
        }
    }

    for (record_index = 0u;
         record_index < GP100_PRESETS_PER_BANK;
         ++record_index) {
        memcpy(record.bytes,
               &flash[GP100_USER_PRESET_BASE +
                      record_index * GP100_PRESET_RECORD_SIZE],
               sizeof(record.bytes));
        if (record_index == 0u) {
            if (gp100_preset_validate(&record) != GP100_PRESET_VALID) {
                return 6;
            }
            for (byte_index = PRESET_AUTHOR_OFFSET;
                 byte_index < PRESET_METADATA_END;
                 ++byte_index) {
                if (record.bytes[byte_index] != 0u) {
                    return 7;
                }
            }
        } else if (gp100_preset_validate(&record) !=
                   GP100_PRESET_ERASED) {
            return 8;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    uint8_t *flash = NULL;
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FLASH_DUMP\n", argv[0]);
        return 2;
    }
    result = read_flash(argv[1], &flash);
    if (result != 0) {
        fprintf(stderr, "could not read flash dump: %d\n", result);
        return 3;
    }
    result = test_factory_record(flash);
    if (result != 0) {
        fprintf(stderr, "factory record test failed: %d\n", result);
        free(flash);
        return 10 + result;
    }
    result = test_user_and_erased_records(flash);
    if (result != 0) {
        fprintf(stderr, "user/erased record test failed: %d\n", result);
        free(flash);
        return 20 + result;
    }
    result = test_crc_repair_and_addresses(flash);
    if (result != 0) {
        fprintf(stderr, "CRC/address test failed: %d\n", result);
        free(flash);
        return 30 + result;
    }
    result = test_complete_banks(flash);
    free(flash);
    if (result != 0) {
        fprintf(stderr, "complete-bank test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
