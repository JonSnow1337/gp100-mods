#include "preset_normalize.h"

#include <string.h>

#define PRESET_NAME_OFFSET                 0x002u
#define PRESET_CHAIN_OFFSET                0x00eu
#define PRESET_ALGORITHM_OFFSET            0x020u
#define PRESET_PARAMETER_OFFSET            0x044u
#define PRESET_PARAMETER_FAMILY_STRIDE     0x03cu
#define PRESET_ENABLE_OFFSET               0x260u
#define PRESET_TEMPO_OFFSET                0x272u
#define PRESET_LEVEL_OFFSET                0x274u
#define PRESET_ASSIGNMENT_OFFSET           0x276u
#define PRESET_EXPRESSION_TARGET_OFFSET    0x282u
#define PRESET_VOLUME_POSITION_OFFSET      0x284u
#define PRESET_SELECTOR_OFFSET             0x286u
#define PRESET_SELECTOR_CACHE_OFFSET       0x28cu
#define PRESET_PARAMETER_SELECTOR_OFFSET   0x298u
#define PRESET_LEGACY_CABINET_OFFSET       0x2beu
#define PRESET_GENRE_OFFSET                0x2c0u
#define PRESET_SCHEMA_OFFSET               0x308u

#define PRESET_ASSIGNMENT_COUNT            6u
#define PRESET_SELECTOR_COUNT              3u
#define PRESET_CONTROL_RANGE_COUNT         4u

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static int16_t read_i16_le(const uint8_t *bytes)
{
    return (int16_t)read_u16_le(bytes);
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static float read_float_le(const uint8_t *bytes)
{
    uint32_t bits = read_u32_le(bytes);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_float_le(uint8_t *bytes, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(bytes, bits);
}

static uint32_t normalize_name(gp100_preset_record_t *record)
{
    static const char default_name[] = "DefaultName";
    uint8_t *name = &record->bytes[PRESET_NAME_OFFSET];

    if (name[0] != 0u) {
        return 0u;
    }
    memset(name, 0, GP100_PRESET_NAME_SIZE);
    memcpy(name, default_name, sizeof(default_name) - 1u);
    return GP100_PRESET_REPAIRED_NAME;
}

static uint32_t normalize_chain(gp100_preset_record_t *record)
{
    bool seen[GP100_EFFECT_FAMILY_COUNT] = {false};
    size_t position;

    for (position = 0u; position < GP100_EFFECT_FAMILY_COUNT; ++position) {
        uint16_t family = read_u16_le(
            &record->bytes[PRESET_CHAIN_OFFSET + position * 2u]);
        if (family >= GP100_EFFECT_FAMILY_COUNT || seen[family]) {
            for (position = 0u;
                 position < GP100_EFFECT_FAMILY_COUNT;
                 ++position) {
                write_u16_le(&record->bytes[PRESET_CHAIN_OFFSET +
                                            position * 2u],
                             (uint16_t)position);
            }
            return GP100_PRESET_REPAIRED_CHAIN;
        }
        seen[family] = true;
    }
    return 0u;
}

static uint32_t normalize_algorithms(
    gp100_preset_record_t *record,
    const gp100_effect_catalog_t *catalog)
{
    uint32_t repairs = 0u;
    size_t family_index;

    if (catalog == NULL || catalog->algorithm_is_valid == NULL) {
        return 0u;
    }

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        uint8_t *algorithm_bytes =
            &record->bytes[PRESET_ALGORITHM_OFFSET + family_index * 4u];
        uint32_t algorithm_id = read_u32_le(algorithm_bytes);
        bool algorithm_valid = catalog->algorithm_is_valid(
            catalog->context, family, algorithm_id);

        if (!algorithm_valid && catalog->default_algorithm != NULL) {
            algorithm_id = catalog->default_algorithm(catalog->context,
                                                       family);
            write_u32_le(algorithm_bytes, algorithm_id);
            repairs |= GP100_PRESET_REPAIRED_ALGORITHM;
            algorithm_valid = true;
        }

        if (algorithm_valid && catalog->parameter_count != NULL &&
            catalog->parameter_rule != NULL) {
            size_t count = catalog->parameter_count(catalog->context, family,
                                                    algorithm_id);
            float stored_parameters[GP100_PARAMETERS_PER_FAMILY];
            size_t parameter_index;

            for (parameter_index = 0u;
                 parameter_index < GP100_PARAMETERS_PER_FAMILY;
                 ++parameter_index) {
                stored_parameters[parameter_index] = read_float_le(
                    &record->bytes[
                        PRESET_PARAMETER_OFFSET +
                        family_index * PRESET_PARAMETER_FAMILY_STRIDE +
                        parameter_index * sizeof(uint32_t)]);
            }

            for (parameter_index = 0u;
                 parameter_index < count;
                 ++parameter_index) {
                gp100_parameter_rule_t rule;
                uint8_t *value_bytes;
                float value;

                if (!catalog->parameter_rule(catalog->context, family,
                                             algorithm_id, parameter_index,
                                             stored_parameters,
                                             &rule) ||
                    rule.storage_slot >= GP100_PARAMETERS_PER_FAMILY) {
                    continue;
                }
                value_bytes = &record->bytes[
                    PRESET_PARAMETER_OFFSET +
                    family_index * PRESET_PARAMETER_FAMILY_STRIDE +
                    (size_t)rule.storage_slot * sizeof(uint32_t)];
                value = read_float_le(value_bytes);
                if (value < rule.minimum || value > rule.maximum) {
                    write_float_le(value_bytes, rule.default_value);
                    stored_parameters[rule.storage_slot] = rule.default_value;
                    repairs |= GP100_PRESET_REPAIRED_PARAMETER;
                }
            }
        }
    }
    return repairs;
}

static uint32_t normalize_flags_and_levels(
    gp100_preset_record_t *record)
{
    uint32_t repairs = 0u;
    size_t index;
    uint16_t value;

    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        uint8_t *flag = &record->bytes[PRESET_ENABLE_OFFSET + index * 2u];
        if (read_u16_le(flag) > 1u) {
            write_u16_le(flag, 0u);
            repairs |= GP100_PRESET_REPAIRED_ENABLE;
        }
    }

    value = read_u16_le(&record->bytes[PRESET_TEMPO_OFFSET]);
    if (value < 40u || value > 250u) {
        write_u16_le(&record->bytes[PRESET_TEMPO_OFFSET], 120u);
        repairs |= GP100_PRESET_REPAIRED_TEMPO;
    }

    if (read_i16_le(&record->bytes[PRESET_LEVEL_OFFSET]) > 99) {
        write_u16_le(&record->bytes[PRESET_LEVEL_OFFSET], 99u);
        repairs |= GP100_PRESET_REPAIRED_LEVEL;
    }
    return repairs;
}

static uint32_t normalize_assignments_and_pedal(
    gp100_preset_record_t *record)
{
    size_t first;
    size_t second;
    size_t index;
    bool duplicate = false;
    uint32_t repairs = 0u;

    for (first = 0u; first + 1u < PRESET_ASSIGNMENT_COUNT; ++first) {
        int16_t first_value = read_i16_le(
            &record->bytes[PRESET_ASSIGNMENT_OFFSET + first * 2u]);
        if (first_value == -1) {
            continue;
        }
        for (second = first + 1u;
             second < PRESET_ASSIGNMENT_COUNT;
             ++second) {
            if (first_value == read_i16_le(
                    &record->bytes[PRESET_ASSIGNMENT_OFFSET + second * 2u])) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            break;
        }
    }
    if (duplicate) {
        for (index = 0u; index < PRESET_ASSIGNMENT_COUNT; ++index) {
            write_u16_le(&record->bytes[PRESET_ASSIGNMENT_OFFSET + index * 2u],
                         (uint16_t)index);
        }
        repairs |= GP100_PRESET_REPAIRED_ASSIGNMENT;
    }

    if (read_i16_le(&record->bytes[PRESET_EXPRESSION_TARGET_OFFSET]) > 1) {
        write_u16_le(&record->bytes[PRESET_EXPRESSION_TARGET_OFFSET], 0u);
        repairs |= GP100_PRESET_REPAIRED_BOOLEAN;
    }
    if (read_i16_le(&record->bytes[PRESET_VOLUME_POSITION_OFFSET]) > 1) {
        write_u16_le(&record->bytes[PRESET_VOLUME_POSITION_OFFSET], 0u);
        repairs |= GP100_PRESET_REPAIRED_BOOLEAN;
    }
    return repairs;
}

static uint32_t normalize_selectors(
    gp100_preset_record_t *record,
    const gp100_effect_catalog_t *catalog)
{
    uint32_t repairs = 0u;
    size_t index;

    for (index = 0u; index < PRESET_SELECTOR_COUNT; ++index) {
        uint8_t *selector_bytes =
            &record->bytes[PRESET_SELECTOR_OFFSET + index * 2u];
        uint8_t *cache = &record->bytes[PRESET_SELECTOR_CACHE_OFFSET + index * 4u];
        uint8_t *parameter_selector =
            &record->bytes[PRESET_PARAMETER_SELECTOR_OFFSET + index * 2u];
        int16_t selector = read_i16_le(selector_bytes);
        size_t cache_source;
        size_t parameter_count;
        uint32_t cached_algorithm;

        if (selector < -1 || selector >= (int16_t)GP100_EFFECT_FAMILY_COUNT) {
            selector = -1;
            write_u16_le(selector_bytes, UINT16_MAX);
            repairs |= GP100_PRESET_REPAIRED_SELECTOR;
        }
        if (selector == -1) {
            /* The factory copies the last two chain entries for an empty target. */
            cache_source = PRESET_CHAIN_OFFSET +
                (GP100_EFFECT_FAMILY_COUNT - 2u) * 2u;
        } else {
            cache_source = PRESET_ALGORITHM_OFFSET + (size_t)selector * 4u;
        }
        cached_algorithm = read_u32_le(&record->bytes[cache_source]);
        if (read_u32_le(cache) != cached_algorithm) {
            write_u32_le(cache, cached_algorithm);
            repairs |= GP100_PRESET_REPAIRED_SELECTOR_CACHE;
        }

        if (selector >= 0 && (catalog == NULL || catalog->parameter_count == NULL)) {
            continue;
        }
        parameter_count = selector == -1 ? 0u : catalog->parameter_count(
            catalog->context, (gp100_effect_family_t)selector, cached_algorithm);

        if (read_i16_le(parameter_selector) > (int16_t)parameter_count) {
            write_u16_le(parameter_selector, 0u);
            repairs |= GP100_PRESET_REPAIRED_PARAMETER_SELECTOR;
        }
    }
    return repairs;
}

static uint32_t normalize_tail(gp100_preset_record_t *record,
                               const gp100_effect_catalog_t *catalog)
{
    static const size_t minimum_offsets[PRESET_CONTROL_RANGE_COUNT] = {
        0x29eu, 0x2a6u, 0x2aau, 0x2aeu
    };
    static const size_t maximum_offsets[PRESET_CONTROL_RANGE_COUNT] = {
        0x2a2u, 0x2b2u, 0x2b6u, 0x2bau
    };
    uint32_t repairs = 0u;
    size_t index;

    for (index = 0u; index < PRESET_CONTROL_RANGE_COUNT; ++index) {
        float minimum = read_float_le(&record->bytes[minimum_offsets[index]]);
        float maximum = read_float_le(&record->bytes[maximum_offsets[index]]);

        if (minimum > 99.0f) {
            write_float_le(&record->bytes[minimum_offsets[index]], 0.0f);
            repairs |= GP100_PRESET_REPAIRED_CONTROL_RANGE;
        }
        if (maximum > 99.0f) {
            write_float_le(&record->bytes[maximum_offsets[index]], 99.0f);
            repairs |= GP100_PRESET_REPAIRED_CONTROL_RANGE;
        }
    }

    if (catalog != NULL && catalog->default_algorithm != NULL) {
        uint16_t cabinet_default = (uint16_t)catalog->default_algorithm(
            catalog->context, GP100_EFFECT_CABINET);
        if (read_u16_le(&record->bytes[PRESET_LEGACY_CABINET_OFFSET]) !=
            cabinet_default) {
            write_u16_le(&record->bytes[PRESET_LEGACY_CABINET_OFFSET],
                         cabinet_default);
            repairs |= GP100_PRESET_REPAIRED_LEGACY_CABINET;
        }
    }

    if (read_u16_le(&record->bytes[PRESET_GENRE_OFFSET]) >=
        GP100_PRESET_GENRE_COUNT) {
        write_u16_le(&record->bytes[PRESET_GENRE_OFFSET], 0u);
        repairs |= GP100_PRESET_REPAIRED_GENRE;
    }
    return repairs;
}

uint32_t gp100_preset_normalize(
    gp100_preset_record_t *record,
    const gp100_effect_catalog_t *catalog)
{
    uint32_t repairs;

    if (record == NULL) {
        return 0u;
    }
    repairs = normalize_name(record);
    repairs |= normalize_chain(record);
    repairs |= normalize_algorithms(record, catalog);
    repairs |= normalize_flags_and_levels(record);
    repairs |= normalize_assignments_and_pedal(record);
    repairs |= normalize_selectors(record, catalog);
    repairs |= normalize_tail(record, catalog);

    if (read_u16_le(&record->bytes[PRESET_SCHEMA_OFFSET]) !=
        GP100_PRESET_SCHEMA_CURRENT) {
        write_u16_le(&record->bytes[PRESET_SCHEMA_OFFSET],
                     GP100_PRESET_SCHEMA_CURRENT);
        repairs |= GP100_PRESET_REPAIRED_SCHEMA;
    }
    return repairs;
}
