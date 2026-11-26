#include "audio/effects/effect_runtime.h"
#include "storage/preset_normalize.h"
#include "tests/fixtures/flash_layout.h"

#include <stdio.h>
#include <string.h>

static void write_u32_le(uint8_t bytes[4], uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int read_factory_preset(const char *flash_path,
                               gp100_preset_record_t *preset)
{
    FILE *file = fopen(flash_path, "rb");
    if (file == NULL) {
        return 1;
    }
    if (fseek(file, (long)GP100_REFERENCE_FACTORY_PRESET_BASE, SEEK_SET) != 0 ||
        fread(preset->bytes, 1u, sizeof(preset->bytes), file) !=
            sizeof(preset->bytes)) {
        fclose(file);
        return 2;
    }
    fclose(file);
    return 0;
}

static int test_factory_preset_recall(const char *flash_path)
{
    gp100_preset_record_t preset;
    gp100_effect_runtime_t runtime;
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    uint32_t first_actions[GP100_EFFECT_FAMILY_COUNT];
    uint32_t second_actions[GP100_EFFECT_FAMILY_COUNT];
    size_t family_index;
    int result = read_factory_preset(flash_path, &preset);

    if (result != 0) {
        return result;
    }
    (void)gp100_preset_normalize(&preset, &catalog);
    gp100_effect_runtime_reset(&runtime);
    if (!gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                 first_actions) ||
        !gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                 second_actions)) {
        return 3;
    }

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        gp100_effect_slot_state_t *slot = &runtime.slots[family_index];
        uint32_t algorithm_id;
        bool enabled;
        int algorithm_index;
        size_t parameter_index;

        if (!gp100_preset_effect_enabled(&preset, family, &enabled) ||
            !gp100_preset_algorithm_id(&preset, family,
                                             &algorithm_id)) {
            return 4;
        }
        algorithm_index = gp100_effect_algorithm_index(family,
                                                              algorithm_id);
        if (!enabled) {
            if (slot->enabled ||
                first_actions[family_index] !=
                    GP100_EFFECT_APPLY_DISABLE ||
                second_actions[family_index] !=
                    GP100_EFFECT_APPLY_DISABLE) {
                return 5;
            }
            continue;
        }

        if (!slot->enabled || !slot->algorithm_loaded ||
            slot->algorithm_index != (uint16_t)algorithm_index ||
            (first_actions[family_index] &
             (GP100_EFFECT_APPLY_ENABLE |
              GP100_EFFECT_APPLY_LOAD_ALGORITHM |
              GP100_EFFECT_APPLY_COPY_PARAMETERS |
              GP100_EFFECT_APPLY_INITIALIZE)) !=
                (GP100_EFFECT_APPLY_ENABLE |
                 GP100_EFFECT_APPLY_LOAD_ALGORITHM |
                 GP100_EFFECT_APPLY_COPY_PARAMETERS |
                 GP100_EFFECT_APPLY_INITIALIZE)) {
            return 6;
        }
        for (parameter_index = 0u;
             parameter_index < GP100_PARAMETERS_PER_FAMILY;
             ++parameter_index) {
            float expected;
            if (!gp100_preset_parameter(&preset, family,
                                              parameter_index, &expected) ||
                memcmp(&slot->parameters[parameter_index], &expected,
                       sizeof(expected)) != 0) {
                return 7;
            }
        }

        uint32_t expected = GP100_EFFECT_APPLY_ENABLE |
                            GP100_EFFECT_APPLY_COPY_PARAMETERS |
                            GP100_EFFECT_APPLY_INITIALIZE;
        if (family == GP100_EFFECT_CABINET) {
            expected |= GP100_EFFECT_APPLY_REFRESH_ASSET;
        }
        if (second_actions[family_index] != expected) {
            return 8;
        }
    }

    {
        uint32_t user_amp_id;
        uint32_t expected = GP100_EFFECT_APPLY_ENABLE |
                            GP100_EFFECT_APPLY_COPY_PARAMETERS |
                            GP100_EFFECT_APPLY_REFRESH_ASSET |
                            GP100_EFFECT_APPLY_INITIALIZE;
        if (!gp100_effect_algorithm_id(GP100_EFFECT_AMPLIFIER,
                                             46u, &user_amp_id)) {
            return 10;
        }
        write_u32_le(&preset.bytes[0x20u +
                                  GP100_EFFECT_AMPLIFIER * 4u],
                     user_amp_id);
        if (!gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                     first_actions) ||
            !gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                     second_actions) ||
            second_actions[GP100_EFFECT_AMPLIFIER] != expected) {
            return 11;
        }
        if (!gp100_preset_set_effect_enabled(
                &preset, GP100_EFFECT_AMPLIFIER, false) ||
            !gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                    second_actions) ||
            runtime.slots[GP100_EFFECT_AMPLIFIER].enabled ||
            !runtime.slots[GP100_EFFECT_AMPLIFIER].algorithm_loaded ||
            runtime.slots[GP100_EFFECT_AMPLIFIER].algorithm_index != 46u ||
            second_actions[GP100_EFFECT_AMPLIFIER] !=
                (GP100_EFFECT_APPLY_DISABLE |
                 GP100_EFFECT_APPLY_INITIALIZE)) {
            return 12;
        }
    }
    return 0;
}

static int test_invalid_arguments(void)
{
    gp100_effect_runtime_t runtime;
    gp100_preset_record_t preset = {0};
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    uint16_t sync_slots[GP100_EFFECT_FAMILY_COUNT];
    gp100_settings_record_t settings = {0};

    gp100_effect_runtime_reset(&runtime);
    return !gp100_effect_runtime_apply_preset(NULL, &preset, actions) &&
                   !gp100_effect_runtime_apply_preset(&runtime, NULL,
                                                            actions) &&
                   !gp100_effect_runtime_apply_preset(&runtime, &preset,
                                                            NULL) &&
                   !gp100_effect_runtime_apply_tap_sync(
                       NULL, &settings, &runtime, sync_slots) &&
                   !gp100_effect_runtime_apply_tap_sync(
                       &preset, NULL, &runtime, sync_slots) &&
                   !gp100_effect_runtime_apply_tap_sync(
                       &preset, &settings, &runtime, NULL)
               ? 0
               : 1;
}

static int test_live_preset_sync(const char *flash_path)
{
    gp100_preset_record_t preset;
    gp100_effect_runtime_t runtime;
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    size_t changed_family = GP100_EFFECT_FAMILY_COUNT;
    size_t family;

    if (read_factory_preset(flash_path, &preset) != 0) return 1;
    (void)gp100_preset_normalize(&preset, &catalog);
    gp100_effect_runtime_reset(&runtime);
    if (!gp100_effect_runtime_apply_preset(
            &runtime, &preset, actions) ||
        !gp100_effect_runtime_sync_preset(
            &runtime, &preset, actions)) {
        return 2;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        bool enabled;
        if (actions[family] != GP100_EFFECT_APPLY_NONE ||
            !gp100_preset_effect_enabled(
                &preset, (gp100_effect_family_t)family, &enabled)) {
            return 3;
        }
        if (enabled && changed_family == GP100_EFFECT_FAMILY_COUNT) {
            changed_family = family;
        }
    }
    if (changed_family == GP100_EFFECT_FAMILY_COUNT ||
        !gp100_preset_set_effect_enabled(
            &preset, (gp100_effect_family_t)changed_family, false) ||
        !gp100_effect_runtime_sync_preset(
            &runtime, &preset, actions) ||
        actions[changed_family] != GP100_EFFECT_APPLY_DISABLE ||
        runtime.slots[changed_family].enabled) {
        return 4;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        if (family != changed_family &&
            actions[family] != GP100_EFFECT_APPLY_NONE) {
            return 5;
        }
    }
    return 0;
}

static int test_live_parameter_update(void)
{
    const gp100_effect_family_t family = GP100_EFFECT_DELAY;
    gp100_effect_runtime_t runtime;
    gp100_effect_slot_state_t *delay;
    gp100_preset_record_t preset = {0};
    gp100_settings_record_t settings = {0};
    uint16_t changed_slots = 0u;
    uint16_t sync_slots[GP100_EFFECT_FAMILY_COUNT];
    float time;

    gp100_effect_runtime_reset(&runtime);
    delay = &runtime.slots[family];
    delay->algorithm_loaded = true;
    delay->parameters[1] = 500.0f;

    if (!gp100_effect_runtime_set_parameter(
            &runtime, family, 0u, 3u, 1.0f, &changed_slots) ||
        delay->parameters[3] != 1.0f || delay->parameters[1] != 4.0f ||
        changed_slots != ((UINT16_C(1) << 3) | (UINT16_C(1) << 1)) ||
        gp100_effect_runtime_set_parameter(
            &runtime, family, 1u, 0u, 0.0f, &changed_slots) ||
        gp100_effect_runtime_set_parameter(
            &runtime, family, 0u, 3u, 2.0f, &changed_slots)) {
        return 1;
    }
    if (!gp100_preset_select_algorithm(&preset, family, 0u) ||
        !gp100_preset_set_parameter(&preset, family, 1u, 319.0f, NULL) ||
        !gp100_effect_runtime_apply_tap_sync(&preset, &settings, &runtime, sync_slots) ||
        sync_slots[family] != 0u ||
        !gp100_preset_parameter(&preset, family, 1u, &time) || time != 319.0f) {
        return 2;
    }
    if (!gp100_settings_set_i16(&settings, GP100_SETTINGS_TAP_SYNC_DELAY, 1) ||
        !gp100_effect_runtime_apply_tap_sync(&preset, &settings, NULL, sync_slots) ||
        sync_slots[family] != 0u ||
        !gp100_effect_runtime_apply_tap_sync(&preset, &settings, &runtime, sync_slots) ||
        sync_slots[family] != UINT16_C(1) << 3 || delay->enabled ||
        delay->parameters[1] != 4.0f || delay->parameters[3] != 1.0f ||
        !gp100_preset_parameter(&preset, family, 1u, &time) || time != 4.0f) {
        return 3;
    }
    return 0;
}

static int test_live_algorithm_selection(void)
{
    gp100_effect_runtime_t runtime;
    gp100_effect_slot_state_t *delay;
    uint32_t action = 0u;
    size_t parameter_count;
    size_t parameter_index;

    gp100_effect_runtime_reset(&runtime);
    if (!gp100_effect_runtime_select_algorithm(
            &runtime, GP100_EFFECT_DELAY, 0u, &action) ||
        action != (GP100_EFFECT_APPLY_DISABLE |
                   GP100_EFFECT_APPLY_LOAD_ALGORITHM |
                   GP100_EFFECT_APPLY_COPY_PARAMETERS |
                   GP100_EFFECT_APPLY_INITIALIZE)) {
        return 1;
    }
    delay = &runtime.slots[GP100_EFFECT_DELAY];
    if (!delay->algorithm_loaded || delay->algorithm_index != 0u ||
        delay->enabled) {
        return 2;
    }
    parameter_count = gp100_effect_parameter_count(
        GP100_EFFECT_DELAY, 0u);
    for (parameter_index = 0u;
         parameter_index < parameter_count;
         ++parameter_index) {
        gp100_parameter_rule_t rule;

        if (!gp100_effect_parameter_rule(
                GP100_EFFECT_DELAY, 0u, parameter_index,
                delay->parameters, &rule) ||
            delay->parameters[rule.storage_slot] != rule.default_value) {
            return 3;
        }
    }
    if (!gp100_effect_runtime_set_enabled(
            &runtime, GP100_EFFECT_DELAY, true, &action) ||
        action != GP100_EFFECT_APPLY_ENABLE || !delay->enabled) {
        return 4;
    }
    delay->parameters[0] = 12.0f;
    if (!gp100_effect_runtime_select_algorithm(
            &runtime, GP100_EFFECT_DELAY, 0u, &action) ||
        action != (GP100_EFFECT_APPLY_ENABLE |
                   GP100_EFFECT_APPLY_COPY_PARAMETERS) ||
        delay->parameters[0] != 12.0f) {
        return 5;
    }
    if (!gp100_effect_runtime_select_algorithm(
            &runtime, GP100_EFFECT_CABINET, 0u, &action) ||
        (action & GP100_EFFECT_APPLY_REFRESH_ASSET) == 0u) {
        return 6;
    }
    if (gp100_effect_runtime_select_algorithm(
            &runtime, GP100_EFFECT_DELAY,
            gp100_effect_algorithm_count(GP100_EFFECT_DELAY),
            &action) ||
        gp100_effect_runtime_set_enabled(
            &runtime, GP100_EFFECT_MODULATION, true, &action)) {
        return 7;
    }
    return 0;
}

static int test_control_target(const char *flash_path)
{
    gp100_preset_record_t preset;
    gp100_effect_runtime_t runtime;
    gp100_effect_slot_state_t *pre_fx;
    uint16_t changed_slots = 0u;

    if (read_factory_preset(flash_path, &preset) != 0) {
        return 1;
    }
    gp100_effect_runtime_reset(&runtime);
    pre_fx = &runtime.slots[GP100_EFFECT_PRE_FX];
    pre_fx->algorithm_loaded = true;
    pre_fx->algorithm_index = 8u;
    if (gp100_effect_runtime_set_control_target(
            &runtime, &preset, 0u, 51.0f, &changed_slots) !=
            GP100_CONTROL_UPDATE_APPLIED ||
        pre_fx->parameters[0] != 51.0f ||
        changed_slots != UINT16_C(1) ||
        gp100_effect_runtime_set_control_target(
            &runtime, &preset, 1u, 0.0f, &changed_slots) !=
            GP100_CONTROL_UPDATE_UNASSIGNED) {
        return 2;
    }
    pre_fx->algorithm_index = 0u;
    if (gp100_effect_runtime_set_control_target(
            &runtime, &preset, 0u, 51.0f, &changed_slots) !=
        GP100_CONTROL_UPDATE_INACTIVE) {
        return 3;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2) {
        fprintf(stderr, "usage: %s FLASH_DUMP\n", argv[0]);
        return 2;
    }
    result = test_factory_preset_recall(argv[1]);
    if (result != 0) {
        fprintf(stderr, "factory preset recall test failed: %d\n", result);
        return 10 + result;
    }
    result = test_invalid_arguments();
    if (result != 0) {
        fprintf(stderr, "invalid argument test failed: %d\n", result);
        return 30 + result;
    }
    result = test_live_preset_sync(argv[1]);
    if (result != 0) {
        fprintf(stderr, "live preset sync test failed: %d\n", result);
        return 35 + result;
    }
    result = test_live_parameter_update();
    if (result != 0) {
        fprintf(stderr, "live parameter test failed: %d\n", result);
        return 40 + result;
    }
    result = test_live_algorithm_selection();
    if (result != 0) {
        fprintf(stderr, "live algorithm test failed: %d\n", result);
        return 50 + result;
    }
    result = test_control_target(argv[1]);
    if (result != 0) {
        fprintf(stderr, "control target test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
