#include "audio/effects/effect_catalog.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_u32(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint16_t read_u16(const uint8_t bytes[2])
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static int test_family_inventory(void)
{
    static const size_t expected_counts[GP100_EFFECT_FAMILY_COUNT] = {
        21u, 20u, 51u, 2u, 60u, 3u, 12u, 12u, 10u
    };
    size_t family_index;
    size_t total = 0u;
    size_t total_parameters = 0u;
    size_t built_in_names = 0u;
    size_t user_slots = 0u;
    size_t unnamed_parameters = 0u;

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        size_t count = gp100_effect_algorithm_count(family);
        size_t algorithm_index;

        if (gp100_effect_family_name(family) == NULL ||
            count != expected_counts[family_index]) {
            return 1;
        }
        total += count;
        for (algorithm_index = 0u;
             algorithm_index < count;
             ++algorithm_index) {
            uint32_t algorithm_id;
            size_t parameter_count;
            size_t parameter_index;
            bool seen_slots[GP100_PARAMETERS_PER_FAMILY] = {false};
            const char *name = gp100_effect_algorithm_name(
                family, algorithm_index);
            if (!gp100_effect_algorithm_id(family, algorithm_index,
                                                &algorithm_id) ||
                gp100_effect_algorithm_index(family, algorithm_id) !=
                    (int)algorithm_index ||
                gp100_effect_parameter_count(family, algorithm_index) ==
                    0u ||
                gp100_effect_parameter_count_for_id(family,
                                                          algorithm_id) !=
                    gp100_effect_parameter_count(family,
                                                       algorithm_index)) {
                return 2;
            }
            total_parameters += gp100_effect_parameter_count(
                family, algorithm_index);
            parameter_count = gp100_effect_parameter_count(
                family, algorithm_index);
            for (parameter_index = 0u;
                 parameter_index < parameter_count;
                 ++parameter_index) {
                const char *parameter_name = gp100_effect_parameter_name(
                    family, algorithm_index, parameter_index);
                uint8_t storage_slot;
                if (parameter_name == NULL ||
                    !gp100_effect_parameter_storage_slot(
                        family, algorithm_index, parameter_index,
                        &storage_slot) ||
                    storage_slot >= GP100_PARAMETERS_PER_FAMILY ||
                    seen_slots[storage_slot] ||
                    gp100_effect_parameter_index_for_storage_slot(
                        family, algorithm_index, storage_slot) !=
                        (int)parameter_index) {
                    return 6;
                }
                if (parameter_name[0] == '\0') {
                    ++unnamed_parameters;
                }
                seen_slots[storage_slot] = true;
            }
            if (name == NULL) {
                if (!gp100_effect_algorithm_is_user_slot(
                        family, algorithm_index)) {
                    return 4;
                }
                ++user_slots;
            } else {
                if (gp100_effect_algorithm_is_user_slot(
                        family, algorithm_index)) {
                    return 5;
                }
                ++built_in_names;
            }
        }
    }
    return total == GP100_TOTAL_EFFECT_ALGORITHMS &&
           total_parameters == GP100_TOTAL_EFFECT_PARAMETERS &&
           built_in_names == 166u && user_slots == 25u &&
           unnamed_parameters == 25u ? 0 : 3;
}

static int test_known_preset_algorithms(void)
{
    static const uint32_t ids[GP100_EFFECT_FAMILY_COUNT] = {
        UINT32_C(0x05000008), UINT32_C(0x03000000),
        UINT32_C(0x0700006e), UINT32_C(0x0000001d),
        UINT32_C(0x0a00002c), UINT32_C(0x0100003c),
        UINT32_C(0x04000000), UINT32_C(0x0b000000),
        UINT32_C(0x0c000001)
    };
    static const int indices[GP100_EFFECT_FAMILY_COUNT] = {
        8, 6, 38, 1, 19, 2, 0, 1, 1
    };
    size_t family_index;

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        if (gp100_effect_algorithm_index(family, ids[family_index]) !=
            indices[family_index]) {
            return 1;
        }
    }
    return 0;
}

static int test_user_cabinets_and_adapter(void)
{
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    size_t index;

    for (index = 0u; index < 20u; ++index) {
        uint32_t algorithm_id;
        if (!gp100_effect_algorithm_id(GP100_EFFECT_CABINET,
                                            40u + index, &algorithm_id) ||
            algorithm_id != UINT32_C(0x0a100000) + (uint32_t)index) {
            return 1;
        }
    }
    if (catalog.context != NULL || catalog.algorithm_is_valid == NULL ||
        catalog.default_algorithm == NULL || catalog.parameter_count == NULL ||
        catalog.parameter_rule == NULL) {
        return 2;
    }
    if (!catalog.algorithm_is_valid(NULL, GP100_EFFECT_REVERB,
                                    UINT32_C(0x0c000009)) ||
        catalog.algorithm_is_valid(NULL, GP100_EFFECT_REVERB,
                                   UINT32_C(0xdeadbeef)) ||
        catalog.default_algorithm(NULL, GP100_EFFECT_DELAY) !=
            UINT32_C(0x0b00000d) ||
        catalog.parameter_count(NULL, GP100_EFFECT_DELAY,
                                UINT32_C(0x0b000003)) != 9u ||
        strcmp(gp100_effect_algorithm_name(GP100_EFFECT_AMPLIFIER,
                                                38u),
               "Bog RedM") != 0 ||
        gp100_effect_algorithm_name(GP100_EFFECT_AMPLIFIER, 46u) !=
            NULL ||
        strcmp(gp100_effect_parameter_name(GP100_EFFECT_DELAY,
                                                8u, 3u),
               " SwpDepth") != 0 ||
        gp100_effect_parameter_name(GP100_EFFECT_AMPLIFIER,
                                         46u, 0u)[0] != '\0') {
        return 3;
    }
    {
        uint8_t slot;
        if (!gp100_effect_parameter_storage_slot(
                GP100_EFFECT_CABINET, 0u, 0u, &slot) || slot != 1u ||
            !gp100_effect_parameter_storage_slot(
                GP100_EFFECT_AMPLIFIER, 22u, 5u, &slot) || slot != 6u ||
            !gp100_effect_parameter_storage_slot(
                GP100_EFFECT_DELAY, 10u, 4u, &slot) || slot != 5u ||
            !gp100_effect_parameter_storage_slot(
                GP100_EFFECT_DELAY, 10u, 5u, &slot) || slot != 6u) {
            return 4;
        }
    }
    return 0;
}

typedef struct sync_case {
    gp100_effect_family_t family;
    uint8_t algorithm_index;
    uint8_t parameter_index;
    uint8_t selector_slot;
} sync_case_t;

static int test_parameter_rules(void)
{
    static const sync_case_t sync_cases[] = {
        {GP100_EFFECT_PRE_FX, 6u, 1u, 6u},
        {GP100_EFFECT_PRE_FX, 14u, 4u, 5u},
        {GP100_EFFECT_MODULATION, 0u, 1u, 3u},
        {GP100_EFFECT_MODULATION, 1u, 1u, 3u},
        {GP100_EFFECT_MODULATION, 2u, 1u, 3u},
        {GP100_EFFECT_MODULATION, 4u, 1u, 4u},
        {GP100_EFFECT_MODULATION, 5u, 1u, 2u},
        {GP100_EFFECT_MODULATION, 6u, 0u, 1u},
        {GP100_EFFECT_MODULATION, 7u, 1u, 2u},
        {GP100_EFFECT_MODULATION, 8u, 1u, 2u},
        {GP100_EFFECT_MODULATION, 9u, 1u, 3u},
        {GP100_EFFECT_MODULATION, 10u, 1u, 3u},
        {GP100_EFFECT_MODULATION, 11u, 1u, 3u},
        {GP100_EFFECT_DELAY, 0u, 1u, 3u},
        {GP100_EFFECT_DELAY, 1u, 1u, 3u},
        {GP100_EFFECT_DELAY, 2u, 1u, 3u},
        {GP100_EFFECT_DELAY, 3u, 1u, 3u},
        {GP100_EFFECT_DELAY, 4u, 1u, 3u},
        {GP100_EFFECT_DELAY, 5u, 2u, 3u},
        {GP100_EFFECT_DELAY, 7u, 2u, 5u},
        {GP100_EFFECT_DELAY, 8u, 2u, 6u},
        {GP100_EFFECT_DELAY, 8u, 4u, 5u},
        {GP100_EFFECT_DELAY, 9u, 2u, 3u},
        {GP100_EFFECT_DELAY, 10u, 1u, 5u},
        {GP100_EFFECT_DELAY, 11u, 2u, 6u},
        {GP100_EFFECT_DELAY, 11u, 5u, 7u}
    };
    float stored[GP100_PARAMETERS_PER_FAMILY] = {0.0f};
    size_t family_index;
    size_t total = 0u;
    size_t index;

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        size_t algorithm_count = gp100_effect_algorithm_count(family);
        size_t algorithm_index;

        for (algorithm_index = 0u; algorithm_index < algorithm_count;
             ++algorithm_index) {
            size_t parameter_count = gp100_effect_parameter_count(
                family, algorithm_index);
            size_t parameter_index;
            for (parameter_index = 0u; parameter_index < parameter_count;
                 ++parameter_index) {
                gp100_parameter_rule_t rule;
                uint8_t expected_slot;
                if (!gp100_effect_parameter_rule(
                        family, algorithm_index, parameter_index, stored,
                        &rule) ||
                    !gp100_effect_parameter_storage_slot(
                        family, algorithm_index, parameter_index,
                        &expected_slot) ||
                    rule.storage_slot != expected_slot ||
                    rule.minimum > rule.default_value ||
                    rule.default_value > rule.maximum) {
                    return 1;
                }
                ++total;
            }
        }
    }
    if (total != GP100_TOTAL_EFFECT_PARAMETERS) {
        return 2;
    }

    for (index = 0u; index < sizeof(sync_cases) / sizeof(sync_cases[0]);
         ++index) {
        const sync_case_t *test = &sync_cases[index];
        gp100_parameter_rule_t rule;
        gp100_parameter_rule_t baseline;
        size_t selector_parameter = SIZE_MAX;
        size_t parameter_count;
        size_t parameter_index;
        uint16_t changed_slots;
        memset(stored, 0, sizeof(stored));
        stored[test->selector_slot] = 1.0f;
        if (!gp100_effect_parameter_rule(
                test->family, test->algorithm_index, test->parameter_index,
                stored, &rule) ||
            rule.minimum != 0.0f || rule.maximum != 10.0f ||
            rule.default_value != 4.0f) {
            return 3;
        }
        stored[test->selector_slot] = 2.0f;
        if (!gp100_effect_parameter_rule(
                test->family, test->algorithm_index, test->parameter_index,
                stored, &rule) ||
            (rule.minimum == 0.0f && rule.maximum == 10.0f &&
             rule.default_value == 4.0f)) {
            return 4;
        }

        parameter_count = gp100_effect_parameter_count(
            test->family, test->algorithm_index);
        for (parameter_index = 0u; parameter_index < parameter_count;
             ++parameter_index) {
            uint8_t slot;
            if (gp100_effect_parameter_storage_slot(
                    test->family, test->algorithm_index, parameter_index,
                    &slot) && slot == test->selector_slot) {
                selector_parameter = parameter_index;
                break;
            }
        }
        if (selector_parameter == SIZE_MAX ||
            !gp100_effect_parameter_rule(
                test->family, test->algorithm_index, test->parameter_index,
                NULL, &baseline)) {
            return 6;
        }
        memset(stored, 0, sizeof(stored));
        stored[baseline.storage_slot] = baseline.maximum;
        if (!gp100_effect_apply_parameter_change(
                test->family, test->algorithm_index, selector_parameter,
                1.0f, stored, &changed_slots) ||
            stored[baseline.storage_slot] != 4.0f ||
            (changed_slots &
             (uint16_t)(UINT16_C(1) << baseline.storage_slot)) == 0u ||
            !gp100_effect_apply_parameter_change(
                test->family, test->algorithm_index, selector_parameter,
                0.0f, stored, &changed_slots) ||
            stored[baseline.storage_slot] != baseline.default_value) {
            return 7;
        }
    }

    if (gp100_effect_tempo_sync_selector_slots(
            GP100_EFFECT_MODULATION, 0u) != UINT16_C(1) << 3 ||
        gp100_effect_tempo_sync_selector_slots(
            GP100_EFFECT_DELAY, 8u) !=
            ((UINT16_C(1) << 5) | (UINT16_C(1) << 6)) ||
        gp100_effect_tempo_sync_selector_slots(
            GP100_EFFECT_DELAY, 99u) != 0u ||
        gp100_effect_tempo_sync_selector_slots(
            (gp100_effect_family_t)99, 0u) != 0u) {
        return 8;
    }

    {
        gp100_parameter_rule_t rule;
        memset(stored, 0, sizeof(stored));
        if (!gp100_effect_parameter_rule(GP100_EFFECT_PRE_FX,
                                              0u, 0u, stored, &rule) ||
            rule.minimum != 0.0f || rule.maximum != 99.0f ||
            rule.default_value != 20.0f || rule.storage_slot != 0u ||
            !gp100_effect_parameter_rule(GP100_EFFECT_DELAY,
                                              0u, 1u, NULL, &rule) ||
            rule.minimum != 20.0f || rule.maximum != 4000.0f ||
            rule.default_value != 500.0f) {
            return 9;
        }
    }
    return 0;
}

static int expect_formatted(gp100_effect_family_t family,
                            size_t algorithm_index,
                            size_t parameter_index,
                            float value,
                            const float stored[
                                GP100_PARAMETERS_PER_FAMILY],
                            const char *expected)
{
    char output[32];
    int length = gp100_effect_format_parameter_value(
        family, algorithm_index, parameter_index, value, stored, output,
        sizeof(output));
    return length == (int)strlen(expected) && strcmp(output, expected) == 0
               ? 0
               : 1;
}

static int test_parameter_formatting(void)
{
    static const char *const divisions[] = {
        "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D",
        "1/4T", "1/8", "1/8D", "1/8T", "1/16"
    };
    float stored[GP100_PARAMETERS_PER_FAMILY] = {0.0f};
    char short_output[4];
    size_t index;

    if (expect_formatted(GP100_EFFECT_PRE_FX, 4u, 3u, 2.0f,
                         stored, "ENH") != 0 ||
        expect_formatted(GP100_EFFECT_PRE_FX, 12u, 0u, 4.0f,
                         stored, "+/-1 Oct") != 0 ||
        expect_formatted(GP100_EFFECT_DISTORTION, 13u, 3u, 1.0f,
                         stored, "Scoop") != 0 ||
        expect_formatted(GP100_EFFECT_AMPLIFIER, 40u, 3u, 3.0f,
                         stored, "1.6kHz") != 0 ||
        expect_formatted(GP100_EFFECT_MODULATION, 0u, 1u, 1.235f,
                         stored, "1.24Hz") != 0 ||
        expect_formatted(GP100_EFFECT_MODULATION, 4u, 2u, 50.0f,
                         stored, "50") != 0 ||
        expect_formatted(GP100_EFFECT_DELAY, 0u, 1u, 500.0f,
                         stored, "500ms") != 0 ||
        expect_formatted(GP100_EFFECT_REVERB, 0u, 1u, 50.0f,
                         stored, "50ms") != 0 ||
        expect_formatted(GP100_EFFECT_PRE_FX, 2u, 1u, 0.5f,
                         stored, "Off") != 0) {
        return 1;
    }

    stored[3] = 1.0f;
    for (index = 0u; index < sizeof(divisions) / sizeof(divisions[0]);
         ++index) {
        if (expect_formatted(GP100_EFFECT_DELAY, 0u, 1u,
                             (float)index, stored, divisions[index]) != 0) {
            return 2;
        }
    }

    if (gp100_effect_format_parameter_value(
            GP100_EFFECT_DELAY, 0u, 1u, 10.0f, stored, short_output,
            sizeof(short_output)) != 4 ||
        strcmp(short_output, "1/1") != 0 ||
        gp100_effect_format_parameter_value(
            GP100_EFFECT_DELAY, 0u, 1u, 10.0f, stored, NULL, 0u) != 4 ||
        gp100_effect_format_parameter_value(
            GP100_EFFECT_DELAY, 0u, 1u, 10.0f, stored, NULL, 1u) != -1 ||
        gp100_effect_format_parameter_value(
            (gp100_effect_family_t)GP100_EFFECT_FAMILY_COUNT,
            0u, 0u, 0.0f, stored, short_output, sizeof(short_output)) != -1) {
        return 3;
    }
    return 0;
}

static int test_invalid_arguments(void)
{
    uint32_t algorithm_id;
    const size_t past_pre_fx = gp100_effect_algorithm_count(GP100_EFFECT_PRE_FX);
    gp100_effect_family_t invalid_family =
        (gp100_effect_family_t)GP100_EFFECT_FAMILY_COUNT;
    uint8_t storage_slot;
    gp100_parameter_rule_t rule;

    if (gp100_effect_family_name(invalid_family) != NULL ||
        gp100_effect_algorithm_count(invalid_family) != 0u ||
        gp100_effect_algorithm_id(invalid_family, 0u, &algorithm_id) ||
        gp100_effect_algorithm_id(GP100_EFFECT_PRE_FX, past_pre_fx,
                                      &algorithm_id) ||
        gp100_effect_algorithm_id(GP100_EFFECT_PRE_FX, 0u, NULL) ||
        gp100_effect_algorithm_index(invalid_family, 0u) != -1 ||
        gp100_effect_default_algorithm_id(invalid_family) != 0u ||
        gp100_effect_parameter_count(invalid_family, 0u) != 0u ||
        gp100_effect_parameter_count(GP100_EFFECT_PRE_FX, past_pre_fx) !=
            0u ||
        gp100_effect_parameter_count_for_id(GP100_EFFECT_PRE_FX,
                                                  UINT32_C(0xdeadbeef)) != 0u ||
        gp100_effect_algorithm_name(invalid_family, 0u) != NULL ||
        gp100_effect_algorithm_name(GP100_EFFECT_PRE_FX, past_pre_fx) !=
            NULL ||
        gp100_effect_algorithm_is_user_slot(invalid_family, 0u) ||
        gp100_effect_parameter_name(invalid_family, 0u, 0u) != NULL ||
        gp100_effect_parameter_name(GP100_EFFECT_PRE_FX,
                                         past_pre_fx, 0u) != NULL ||
        gp100_effect_parameter_name(GP100_EFFECT_PRE_FX,
                                         0u, 2u) != NULL ||
        gp100_effect_parameter_storage_slot(invalid_family, 0u, 0u,
                                                 &storage_slot) ||
        gp100_effect_parameter_storage_slot(GP100_EFFECT_PRE_FX,
                                                 0u, 2u, &storage_slot) ||
        gp100_effect_parameter_storage_slot(GP100_EFFECT_PRE_FX,
                                                 0u, 0u, NULL)) {
        return 1;
    }
    if (gp100_effect_parameter_index_for_storage_slot(
            invalid_family, 0u, 0u) != -1 ||
        gp100_effect_parameter_index_for_storage_slot(
            GP100_EFFECT_PRE_FX, past_pre_fx, 0u) != -1 ||
        gp100_effect_parameter_index_for_storage_slot(
            GP100_EFFECT_PRE_FX, 0u,
            GP100_PARAMETERS_PER_FAMILY) != -1 ||
        gp100_effect_parameter_index_for_storage_slot(
            GP100_EFFECT_CABINET, 0u, 0u) != -1) {
        return 2;
    }
    if (gp100_effect_parameter_rule(invalid_family, 0u, 0u, NULL,
                                         &rule) ||
        gp100_effect_parameter_rule(GP100_EFFECT_PRE_FX, past_pre_fx, 0u,
                                         NULL, &rule) ||
        gp100_effect_parameter_rule(GP100_EFFECT_PRE_FX, 0u, 2u,
                                         NULL, &rule) ||
        gp100_effect_parameter_rule(GP100_EFFECT_PRE_FX, 0u, 0u,
                                         NULL, NULL)) {
        return 3;
    }
    return 0;
}

static size_t factory_algorithm_count(gp100_effect_family_t family)
{
    if (family == GP100_EFFECT_PRE_FX) {
        return GP100_PRE_FX_LIMITTER;
    }
    if (family == GP100_EFFECT_DISTORTION) {
        return GP100_DISTORTION_MUFF;
    }
    return gp100_effect_algorithm_count(family);
}

static int test_sdram_tables(const char *path)
{
    static const uint32_t table_addresses[GP100_EFFECT_FAMILY_COUNT] = {
        UINT32_C(0x80133194), UINT32_C(0x801333a0),
        UINT32_C(0x80133754), UINT32_C(0x80133834),
        UINT32_C(0x80133a1c), UINT32_C(0x80133b44),
        UINT32_C(0x80133bf0), UINT32_C(0x80133cf4),
        UINT32_C(0x80133db4)
    };
    static const uint32_t parameter_count_addresses[
        GP100_EFFECT_FAMILY_COUNT] = {
        UINT32_C(0x80133214), UINT32_C(0x80133378),
        UINT32_C(0x801336ec), UINT32_C(0x80133828),
        UINT32_C(0x801339a4), UINT32_C(0x80133b30),
        UINT32_C(0x80133bd8), UINT32_C(0x80133cdc),
        UINT32_C(0x80133da0)
    };
    static const uint32_t parameter_map_addresses[
        GP100_EFFECT_FAMILY_COUNT] = {
        UINT32_C(0x801331d4), UINT32_C(0x8013332c),
        UINT32_C(0x80133620), UINT32_C(0x8013382c),
        UINT32_C(0x801338b4), UINT32_C(0x80133b38),
        UINT32_C(0x80133ba8), UINT32_C(0x80133cac),
        UINT32_C(0x80133d78)
    };
    FILE *file = fopen(path, "rb");
    size_t family_index;

    if (file == NULL) {
        return 1;
    }
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        size_t count = factory_algorithm_count(family);
        size_t algorithm_index;
        long offset = (long)(table_addresses[family_index] -
                             UINT32_C(0x80000000));

        if (fseek(file, offset, SEEK_SET) != 0) {
            fclose(file);
            return 2;
        }
        for (algorithm_index = 0u;
             algorithm_index < count;
             ++algorithm_index) {
            uint8_t bytes[4];
            uint32_t expected;
            uint32_t actual;

            if (fread(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) {
                fclose(file);
                return 3;
            }
            expected = read_u32(bytes);
            if (!gp100_effect_algorithm_id(family, algorithm_index,
                                                &actual) ||
                actual != expected) {
                fclose(file);
                return 4;
            }
        }
        offset = (long)(parameter_count_addresses[family_index] -
                        UINT32_C(0x80000000));
        if (fseek(file, offset, SEEK_SET) != 0) {
            fclose(file);
            return 5;
        }
        for (algorithm_index = 0u;
             algorithm_index < count;
             ++algorithm_index) {
            uint8_t bytes[2];
            if (fread(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) {
                fclose(file);
                return 6;
            }
            if (gp100_effect_parameter_count(family, algorithm_index) !=
                read_u16(bytes)) {
                fclose(file);
                return 7;
            }
        }
        for (algorithm_index = 0u;
             algorithm_index < count;
             ++algorithm_index) {
            uint8_t pointer_bytes[4];
            uint32_t map_address;
            size_t parameter_count = gp100_effect_parameter_count(
                family, algorithm_index);
            size_t parameter_index;

            offset = (long)(parameter_map_addresses[family_index] -
                            UINT32_C(0x80000000) + algorithm_index * 4u);
            if (fseek(file, offset, SEEK_SET) != 0 ||
                fread(pointer_bytes, 1u, sizeof(pointer_bytes), file) !=
                    sizeof(pointer_bytes)) {
                fclose(file);
                return 8;
            }
            map_address = read_u32(pointer_bytes);
            for (parameter_index = 0u;
                 parameter_index < parameter_count;
                 ++parameter_index) {
                uint8_t slot_bytes[2];
                uint8_t actual_slot;

                offset = (long)(map_address - UINT32_C(0x80000000) +
                                parameter_index * 2u);
                if (fseek(file, offset, SEEK_SET) != 0 ||
                    fread(slot_bytes, 1u, sizeof(slot_bytes), file) !=
                        sizeof(slot_bytes) ||
                    !gp100_effect_parameter_storage_slot(
                        family, algorithm_index, parameter_index,
                        &actual_slot) ||
                    actual_slot != read_u16(slot_bytes)) {
                    fclose(file);
                    return 9;
                }
            }
        }
    }

    {
        enum { NAME_REGION_SIZE = 0x10000 };
        uint8_t *name_region = malloc(NAME_REGION_SIZE);
        if (name_region == NULL ||
            fseek(file, 0x00240000L, SEEK_SET) != 0 ||
            fread(name_region, 1u, NAME_REGION_SIZE, file) != NAME_REGION_SIZE) {
            free(name_region);
            fclose(file);
            return 10;
        }
        for (family_index = 0u;
             family_index < GP100_EFFECT_FAMILY_COUNT;
             ++family_index) {
            gp100_effect_family_t family =
                (gp100_effect_family_t)family_index;
            size_t count = factory_algorithm_count(family);
            size_t algorithm_index;

            for (algorithm_index = 0u;
                 algorithm_index < count;
                 ++algorithm_index) {
                const char *name = gp100_effect_algorithm_name(
                    family, algorithm_index);
                size_t name_size;
                size_t offset;
                bool found = false;

                if (name == NULL) {
                    continue;
                }
                name_size = strlen(name) + 1u;
                for (offset = 0u;
                     offset + name_size <= NAME_REGION_SIZE;
                     ++offset) {
                    if (memcmp(&name_region[offset], name, name_size) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    free(name_region);
                    fclose(file);
                    return 11;
                }

                {
                    size_t parameter_count =
                        gp100_effect_parameter_count(family,
                                                          algorithm_index);
                    size_t parameter_index;
                    for (parameter_index = 0u;
                         parameter_index < parameter_count;
                         ++parameter_index) {
                        const char *parameter_name =
                            gp100_effect_parameter_name(
                                family, algorithm_index, parameter_index);
                        bool parameter_found = false;

                        if (parameter_name[0] == '\0') {
                            continue;
                        }
                        name_size = strlen(parameter_name) + 1u;
                        for (offset = 0u;
                             offset + name_size <= NAME_REGION_SIZE;
                             ++offset) {
                            if (memcmp(&name_region[offset], parameter_name,
                                       name_size) == 0) {
                                parameter_found = true;
                                break;
                            }
                        }
                        if (!parameter_found) {
                            free(name_region);
                            fclose(file);
                            return 12;
                        }
                    }
                }
            }
        }
        free(name_region);
    }
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    int result = test_family_inventory();
    if (result != 0) {
        fprintf(stderr, "family inventory test failed: %d\n", result);
        return 10 + result;
    }
    result = test_known_preset_algorithms();
    if (result != 0) {
        fprintf(stderr, "known algorithm test failed: %d\n", result);
        return 20 + result;
    }
    result = test_user_cabinets_and_adapter();
    if (result != 0) {
        fprintf(stderr, "cabinet/adapter test failed: %d\n", result);
        return 30 + result;
    }
    result = test_invalid_arguments();
    if (result != 0) {
        fprintf(stderr, "invalid argument test failed: %d\n", result);
        return 40 + result;
    }
    result = test_parameter_rules();
    if (result != 0) {
        fprintf(stderr, "parameter rule test failed: %d\n", result);
        return 45 + result;
    }
    result = test_parameter_formatting();
    if (result != 0) {
        fprintf(stderr, "parameter formatting test failed: %d\n", result);
        return 48 + result;
    }
    if (argc == 2) {
        result = test_sdram_tables(argv[1]);
        if (result != 0) {
            fprintf(stderr, "SDRAM table test failed: %d\n", result);
            return 50 + result;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [SDRAM_IMAGE]\n", argv[0]);
        return 2;
    }
    return 0;
}
