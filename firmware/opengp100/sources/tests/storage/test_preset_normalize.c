#include "audio/effects/effect_catalog.h"
#include "storage/preset_normalize.h"
#include "tests/fixtures/flash_layout.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static float read_float(const uint8_t *bytes)
{
    uint32_t bits = read_u32(bytes);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_float(uint8_t *bytes, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(bytes, bits);
}

static bool algorithm_is_valid(void *context,
                               gp100_effect_family_t family,
                               uint32_t algorithm_id)
{
    (void)context;
    return algorithm_id == UINT32_C(0x10000000) + (uint32_t)family;
}

static uint32_t default_algorithm(void *context,
                                  gp100_effect_family_t family)
{
    (void)context;
    return UINT32_C(0x10000000) + (uint32_t)family;
}

static size_t parameter_count(void *context,
                              gp100_effect_family_t family,
                              uint32_t algorithm_id)
{
    (void)context;
    (void)family;
    (void)algorithm_id;
    return 2u;
}

static bool parameter_rule(void *context,
                           gp100_effect_family_t family,
                           uint32_t algorithm_id,
                           size_t parameter_index,
                           const float stored_parameters[
                               GP100_PARAMETERS_PER_FAMILY],
                           gp100_parameter_rule_t *rule)
{
    (void)context;
    (void)family;
    (void)algorithm_id;
    (void)stored_parameters;
    if (parameter_index >= 2u) {
        return false;
    }
    rule->storage_slot = (uint8_t)(parameter_index == 0u ? 3u : 1u);
    rule->minimum = 0.0f;
    rule->maximum = 10.0f;
    rule->default_value = parameter_index == 0u ? 4.0f : 5.0f;
    return true;
}

static gp100_effect_catalog_t test_catalog(void)
{
    gp100_effect_catalog_t catalog;
    catalog.context = NULL;
    catalog.algorithm_is_valid = algorithm_is_valid;
    catalog.default_algorithm = default_algorithm;
    catalog.parameter_count = parameter_count;
    catalog.parameter_rule = parameter_rule;
    return catalog;
}

static void make_valid_record(gp100_preset_record_t *record)
{
    size_t index;

    memset(record, 0, sizeof(*record));
    memcpy(&record->bytes[0x002u], "Readable", 9u);
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        write_u16(&record->bytes[0x00eu + index * 2u], (uint16_t)index);
        write_u32(&record->bytes[0x020u + index * 4u],
                  UINT32_C(0x10000000) + (uint32_t)index);
        write_float(&record->bytes[0x044u + index * 0x3cu + 3u * 4u], 4.0f);
        write_float(&record->bytes[0x044u + index * 0x3cu + 1u * 4u], 5.0f);
        write_u16(&record->bytes[0x260u + index * 2u], 1u);
    }
    write_u16(&record->bytes[0x272u], 120u);
    write_u16(&record->bytes[0x274u], 50u);
    for (index = 0u; index < 6u; ++index) {
        write_u16(&record->bytes[0x276u + index * 2u], (uint16_t)index);
    }
    write_u16(&record->bytes[0x282u], 0u);
    write_u16(&record->bytes[0x284u], 1u);
    for (index = 0u; index < 3u; ++index) {
        write_u16(&record->bytes[0x286u + index * 2u], (uint16_t)index);
        write_u32(&record->bytes[0x28cu + index * 4u],
                  UINT32_C(0x10000000) + (uint32_t)index);
        write_u16(&record->bytes[0x298u + index * 2u], 1u);
    }
    write_float(&record->bytes[0x29eu], 0.0f);
    write_float(&record->bytes[0x2a2u], 99.0f);
    write_float(&record->bytes[0x2a6u], 0.0f);
    write_float(&record->bytes[0x2aau], 0.0f);
    write_float(&record->bytes[0x2aeu], 0.0f);
    write_float(&record->bytes[0x2b2u], 99.0f);
    write_float(&record->bytes[0x2b6u], 99.0f);
    write_float(&record->bytes[0x2bau], 99.0f);
    write_u16(&record->bytes[0x2beu], 4u);
    write_u16(&record->bytes[0x2c0u], 60u);
    write_u16(&record->bytes[0x308u], GP100_PRESET_SCHEMA_CURRENT);
}

static int test_valid_record_is_unchanged(void)
{
    gp100_preset_record_t record;
    gp100_effect_catalog_t catalog = test_catalog();
    gp100_preset_record_t before;

    make_valid_record(&record);
    before = record;
    if (gp100_preset_normalize(&record, &catalog) != 0u ||
        memcmp(&record, &before, sizeof(record)) != 0) {
        return 1;
    }
    return 0;
}

static int test_common_repairs(void)
{
    gp100_preset_record_t record;
    uint32_t repairs;
    size_t index;

    make_valid_record(&record);
    record.bytes[0x002u] = 0u;
    write_u16(&record.bytes[0x00eu + 2u * 2u], 1u);
    write_u16(&record.bytes[0x260u], 2u);
    write_u16(&record.bytes[0x272u], 39u);
    write_u16(&record.bytes[0x274u], 120u);
    write_u16(&record.bytes[0x276u + 4u * 2u], 0u);
    write_u16(&record.bytes[0x282u], 2u);
    write_u16(&record.bytes[0x286u], 10u);
    write_u32(&record.bytes[0x28cu], UINT32_C(0xdeadbeef));
    write_float(&record.bytes[0x29eu], 100.0f);
    write_float(&record.bytes[0x2b2u], 101.0f);
    write_u16(&record.bytes[0x2c0u], 61u);
    write_u16(&record.bytes[0x308u], 0u);

    repairs = gp100_preset_normalize(&record, NULL);
    if ((repairs & GP100_PRESET_REPAIRED_NAME) == 0u ||
        strcmp((const char *)&record.bytes[0x002u], "DefaultName") != 0) {
        return 1;
    }
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        if (read_u16(&record.bytes[0x00eu + index * 2u]) != index) {
            return 2;
        }
    }
    if (read_u16(&record.bytes[0x260u]) != 0u ||
        read_u16(&record.bytes[0x272u]) != 120u ||
        read_u16(&record.bytes[0x274u]) != 99u) {
        return 3;
    }
    for (index = 0u; index < 6u; ++index) {
        if (read_u16(&record.bytes[0x276u + index * 2u]) != index) {
            return 4;
        }
    }
    if (read_u16(&record.bytes[0x282u]) != 0u ||
        read_u16(&record.bytes[0x286u]) != UINT16_MAX ||
        read_u32(&record.bytes[0x28cu]) != UINT32_C(0x00080007) ||
        read_u16(&record.bytes[0x298u]) != 0u) {
        return 5;
    }
    if ((repairs & GP100_PRESET_REPAIRED_GENRE) == 0u ||
        fabsf(read_float(&record.bytes[0x29eu])) > 0.001f ||
        fabsf(read_float(&record.bytes[0x2b2u]) - 99.0f) > 0.001f ||
        read_u16(&record.bytes[0x2c0u]) != 0u ||
        read_u16(&record.bytes[0x308u]) != GP100_PRESET_SCHEMA_CURRENT) {
        return 6;
    }
    return 0;
}

static int test_catalog_repairs(void)
{
    gp100_preset_record_t record;
    gp100_effect_catalog_t catalog = test_catalog();
    uint32_t repairs;

    make_valid_record(&record);
    write_u32(&record.bytes[0x020u], UINT32_C(0xfeedface));
    write_float(&record.bytes[0x044u + 3u * 4u], 11.0f);
    write_float(&record.bytes[0x044u + 1u * 4u], -1.0f);
    write_u16(&record.bytes[0x298u], 3u);
    write_u16(&record.bytes[0x2beu], 56u);

    repairs = gp100_preset_normalize(&record, &catalog);
    if ((repairs & GP100_PRESET_REPAIRED_ALGORITHM) == 0u ||
        read_u32(&record.bytes[0x020u]) != UINT32_C(0x10000000)) {
        return 1;
    }
    if ((repairs & GP100_PRESET_REPAIRED_PARAMETER) == 0u ||
        fabsf(read_float(&record.bytes[0x044u + 3u * 4u]) - 4.0f) > 0.001f ||
        fabsf(read_float(&record.bytes[0x044u + 1u * 4u]) - 5.0f) > 0.001f) {
        return 2;
    }
    if ((repairs & GP100_PRESET_REPAIRED_PARAMETER_SELECTOR) == 0u ||
        read_u16(&record.bytes[0x298u]) != 0u) {
        return 3;
    }
    if ((repairs & GP100_PRESET_REPAIRED_LEGACY_CABINET) == 0u ||
        read_u16(&record.bytes[0x2beu]) != 4u) {
        return 4;
    }
    return 0;
}

static int test_factory_dump(const char *path)
{
    gp100_preset_record_t record;
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    FILE *file = fopen(path, "rb");
    size_t index;

    if (file == NULL) {
        return 1;
    }
    for (index = 0u; index < GP100_PRESETS_PER_BANK; ++index) {
        long offset = (long)(GP100_REFERENCE_FACTORY_PRESET_BASE +
                             index * GP100_PRESET_RECORD_SIZE);
        gp100_preset_record_t before;
        uint32_t repairs;
        uint32_t expected = GP100_PRESET_REPAIRED_SCHEMA;

        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(record.bytes, 1u, sizeof(record.bytes), file) !=
                sizeof(record.bytes)) {
            fclose(file);
            return 2;
        }
        before = record;
        repairs = gp100_preset_normalize(&record, &catalog);
        if (index == 41u) {
            expected |= GP100_PRESET_REPAIRED_SELECTOR_CACHE;
        }
        if (repairs != expected) {
            size_t byte_index;
            fprintf(stderr,
                    "factory preset %zu repair mask: 0x%08lx\n",
                    index, (unsigned long)repairs);
            for (byte_index = 0u; byte_index < sizeof(record.bytes);
                 ++byte_index) {
                if (record.bytes[byte_index] != before.bytes[byte_index]) {
                    fprintf(stderr, "  changed byte 0x%03zx: %02x -> %02x\n",
                            byte_index, before.bytes[byte_index],
                            record.bytes[byte_index]);
                }
            }
            fclose(file);
            return 3;
        }
        if (gp100_preset_normalize(&record, &catalog) != 0u) {
            fclose(file);
            return 4;
        }
    }
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    int result = test_valid_record_is_unchanged();
    if (result != 0) {
        fprintf(stderr, "valid record test failed: %d\n", result);
        return 10 + result;
    }
    result = test_common_repairs();
    if (result != 0) {
        fprintf(stderr, "common repair test failed: %d\n", result);
        return 20 + result;
    }
    result = test_catalog_repairs();
    if (result != 0) {
        fprintf(stderr, "catalog repair test failed: %d\n", result);
        return 30 + result;
    }
    if (argc == 2) {
        result = test_factory_dump(argv[1]);
        if (result != 0) {
            fprintf(stderr, "factory dump test failed: %d\n", result);
            return 40 + result;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [FLASH_DUMP]\n", argv[0]);
        return 2;
    }
    return 0;
}
