#include "effect_runtime.h"

#include "storage/patch_chain.h"

#include <string.h>

static const struct {
    gp100_effect_family_t family;
    gp100_settings_field_t setting;
} tap_sync_families[] = {
    {GP100_EFFECT_PRE_FX, GP100_SETTINGS_TAP_SYNC_PRE},
    {GP100_EFFECT_MODULATION, GP100_SETTINGS_TAP_SYNC_MODULATION},
    {GP100_EFFECT_DELAY, GP100_SETTINGS_TAP_SYNC_DELAY}
};

void gp100_effect_runtime_reset(gp100_effect_runtime_t *runtime)
{
    if (runtime != NULL) {
        memset(runtime, 0, sizeof(*runtime));
    }
}

static uint32_t enabled_action(bool enabled)
{
    return enabled ? GP100_EFFECT_APPLY_ENABLE
                   : GP100_EFFECT_APPLY_DISABLE;
}

static bool algorithm_uses_external_asset(
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    return family == GP100_EFFECT_CABINET ||
           (family == GP100_EFFECT_AMPLIFIER && algorithm_index >= 46u);
}

static bool load_native_patch(
    gp100_effect_runtime_t *runtime, const gp100_preset_record_t *preset,
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT])
{
    gp100_effect_runtime_t next = {0};
    for (size_t position = 0u; position < gp100_patch_count(preset); ++position) {
        gp100_patch_block_t block;
        if (!gp100_patch_block_at(preset, position, &block)) {
            return false;
        }
        gp100_effect_slot_state_t *slot = &next.slots[block.id];
        slot->enabled = block.enabled;
        slot->algorithm_loaded = true;
        slot->algorithm_index = block.algorithm;
        slot->family = block.family;
        slot->instance_id = block.id;
        memcpy(slot->parameters, block.parameters, sizeof(slot->parameters));
    }
    *runtime = next;
    memset(actions, 0, GP100_EFFECT_FAMILY_COUNT * sizeof(*actions));
    return true;
}

bool gp100_effect_runtime_apply_preset(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT])
{
    gp100_effect_runtime_t next;
    uint32_t next_actions[GP100_EFFECT_FAMILY_COUNT];
    size_t family_index;

    if (runtime == NULL || preset == NULL || actions == NULL) {
        return false;
    }
    if (gp100_patch_is_native(preset)) {
        return load_native_patch(runtime, preset, actions);
    }
    next = *runtime;

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        gp100_effect_slot_state_t *slot = &next.slots[family_index];
        uint32_t algorithm_id;
        bool enabled;
        int algorithm_index;
        size_t parameter_index;
        uint32_t action;

        if (!gp100_preset_effect_enabled(preset, family, &enabled) ||
            !gp100_preset_algorithm_id(preset, family, &algorithm_id)) {
            return false;
        }

        if (!enabled) {
            slot->enabled = false;
            next_actions[family_index] = GP100_EFFECT_APPLY_DISABLE |
                (slot->algorithm_loaded ? GP100_EFFECT_APPLY_INITIALIZE : 0u);
            continue;
        }

        algorithm_index = gp100_effect_algorithm_index(family,
                                                              algorithm_id);
        if (algorithm_index < 0) {
            return false;
        }

        action = GP100_EFFECT_APPLY_ENABLE |
                 GP100_EFFECT_APPLY_COPY_PARAMETERS |
                 GP100_EFFECT_APPLY_INITIALIZE;
        if (!slot->algorithm_loaded ||
            slot->algorithm_index != (uint16_t)algorithm_index) {
            action |= GP100_EFFECT_APPLY_LOAD_ALGORITHM;
        }
        if (family == GP100_EFFECT_CABINET ||
            (family == GP100_EFFECT_AMPLIFIER &&
             algorithm_index >= 46)) {
            action |= GP100_EFFECT_APPLY_REFRESH_ASSET;
        }

        for (parameter_index = 0u;
             parameter_index < GP100_PARAMETERS_PER_FAMILY;
             ++parameter_index) {
            if (!gp100_preset_parameter(preset, family,
                                              parameter_index,
                                              &slot->parameters[
                                                  parameter_index])) {
                return false;
            }
        }
        slot->algorithm_index = (uint16_t)algorithm_index;
        slot->algorithm_loaded = true;
        slot->enabled = true;
        next_actions[family_index] = action;
    }

    *runtime = next;
    memcpy(actions, next_actions, sizeof(next_actions));
    return true;
}

bool gp100_effect_runtime_sync_preset(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT])
{
    gp100_effect_runtime_t next;
    uint32_t next_actions[GP100_EFFECT_FAMILY_COUNT] = {0};
    size_t family_index;

    if (runtime == NULL || preset == NULL || actions == NULL) {
        return false;
    }
    if (gp100_patch_is_native(preset)) {
        return load_native_patch(runtime, preset, actions);
    }
    next = *runtime;
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        gp100_effect_slot_state_t *slot = &next.slots[family_index];
        bool enabled;
        uint32_t algorithm_id;
        int algorithm_index;
        float parameters[GP100_PARAMETERS_PER_FAMILY];
        uint32_t action = GP100_EFFECT_APPLY_NONE;
        bool algorithm_changed;
        size_t parameter_index;

        if (!gp100_preset_effect_enabled(preset, family, &enabled) ||
            !gp100_preset_algorithm_id(
                preset, family, &algorithm_id)) {
            return false;
        }
        if (!enabled) {
            if (slot->enabled) action = GP100_EFFECT_APPLY_DISABLE;
            slot->enabled = false;
            next_actions[family_index] = action;
            continue;
        }
        algorithm_index = gp100_effect_algorithm_index(
            family, algorithm_id);
        if (algorithm_index < 0) return false;
        for (parameter_index = 0u;
             parameter_index < GP100_PARAMETERS_PER_FAMILY;
             ++parameter_index) {
            if (!gp100_preset_parameter(
                    preset, family, parameter_index,
                    &parameters[parameter_index])) {
                return false;
            }
        }
        algorithm_changed = !slot->algorithm_loaded ||
            slot->algorithm_index != (uint16_t)algorithm_index;
        if (!slot->enabled) action |= GP100_EFFECT_APPLY_ENABLE;
        if (algorithm_changed) {
            action |= GP100_EFFECT_APPLY_LOAD_ALGORITHM |
                      GP100_EFFECT_APPLY_INITIALIZE |
                      GP100_EFFECT_APPLY_COPY_PARAMETERS;
            if (algorithm_uses_external_asset(
                    family, (size_t)algorithm_index)) {
                action |= GP100_EFFECT_APPLY_REFRESH_ASSET;
            }
        } else if (memcmp(
                       slot->parameters, parameters,
                       sizeof(parameters)) != 0) {
            action |= GP100_EFFECT_APPLY_COPY_PARAMETERS;
        }
        memcpy(slot->parameters, parameters, sizeof(parameters));
        slot->algorithm_index = (uint16_t)algorithm_index;
        slot->algorithm_loaded = true;
        slot->enabled = true;
        next_actions[family_index] = action;
    }
    *runtime = next;
    memcpy(actions, next_actions, sizeof(next_actions));
    return true;
}

bool gp100_effect_runtime_set_enabled(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    bool enabled,
    uint32_t *action)
{
    gp100_effect_slot_state_t *slot;

    if (runtime == NULL || action == NULL ||
        (size_t)family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    slot = &runtime->slots[(size_t)family];
    if (!slot->algorithm_loaded) {
        return false;
    }
    slot->enabled = enabled;
    *action = enabled_action(enabled);
    return true;
}

bool gp100_effect_runtime_select_algorithm(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint32_t *action)
{
    gp100_effect_slot_state_t next;
    size_t parameter_count;
    size_t parameter_index;
    uint32_t next_action;

    if (runtime == NULL || action == NULL ||
        (size_t)family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    if (algorithm_index >= gp100_effect_algorithm_count(family)) {
        return false;
    }
    parameter_count = gp100_effect_parameter_count(family,
                                                         algorithm_index);

    next = runtime->slots[(size_t)family];
    next_action = enabled_action(next.enabled) |
                  GP100_EFFECT_APPLY_COPY_PARAMETERS;
    if (!next.algorithm_loaded || next.algorithm_index != algorithm_index) {
        memset(next.parameters, 0, sizeof(next.parameters));
        for (parameter_index = 0u;
             parameter_index < parameter_count;
             ++parameter_index) {
            gp100_parameter_rule_t rule;

            if (!gp100_effect_parameter_rule(
                    family, algorithm_index, parameter_index,
                    next.parameters, &rule)) {
                return false;
            }
            next.parameters[rule.storage_slot] = rule.default_value;
        }
        next.algorithm_index = (uint16_t)algorithm_index;
        next.algorithm_loaded = true;
        next_action |= GP100_EFFECT_APPLY_LOAD_ALGORITHM |
                       GP100_EFFECT_APPLY_INITIALIZE;
        if (algorithm_uses_external_asset(family, algorithm_index)) {
            next_action |= GP100_EFFECT_APPLY_REFRESH_ASSET;
        }
    }

    runtime->slots[(size_t)family] = next;
    *action = next_action;
    return true;
}

bool gp100_effect_runtime_set_parameter(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    uint16_t *changed_slots)
{
    gp100_effect_slot_state_t *slot;

    if (runtime == NULL || (size_t)family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    slot = &runtime->slots[(size_t)family];
    if (!slot->algorithm_loaded || slot->algorithm_index != algorithm_index) {
        return false;
    }
    return gp100_effect_apply_parameter_change(
        family, algorithm_index, parameter_index, value, slot->parameters,
        changed_slots);
}

bool gp100_effect_runtime_apply_tap_sync(
    gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    gp100_effect_runtime_t *runtime,
    uint16_t selectors[GP100_EFFECT_FAMILY_COUNT])
{
    gp100_preset_record_t next_preset;
    uint16_t selected[GP100_EFFECT_FAMILY_COUNT] = {0};
    uint16_t changed[GP100_EFFECT_FAMILY_COUNT] = {0};
    size_t index;

    if (preset == NULL || settings == NULL || selectors == NULL) {
        return false;
    }
    if (gp100_patch_is_native(preset)) {
        next_preset = *preset;
        for (size_t position = 0u; position < gp100_patch_count(preset); ++position) {
            gp100_patch_block_t block;
            if (!gp100_patch_block_at(preset, position, &block)) {
                return false;
            }
            for (size_t i = 0u;
                 i < sizeof(tap_sync_families) / sizeof(tap_sync_families[0]); ++i) {
                if (tap_sync_families[i].family != block.family) {
                    continue;
                }
                int16_t sync;
                if (!gp100_settings_get_i16(settings, tap_sync_families[i].setting, &sync)) {
                    return false;
                }
                if (sync == 0) {
                    break;
                }
                const uint16_t mask = gp100_effect_tempo_sync_selector_slots(
                    block.family, block.algorithm);
                for (uint8_t storage = 0; storage < GP100_PARAMETERS_PER_FAMILY; ++storage) {
                    if ((mask & (1u << storage)) != 0u) {
                        const int parameter = gp100_effect_parameter_index_for_storage_slot(
                            block.family, block.algorithm, storage);
                        if (parameter < 0 || !gp100_effect_apply_parameter_change(
                                block.family, block.algorithm, (size_t)parameter, 1.0f,
                                block.parameters, NULL)) {
                            return false;
                        }
                    }
                }
                if (mask != 0u && !gp100_patch_update(&next_preset, &block)) {
                    return false;
                }
                selected[block.family] |= mask;
                break;
            }
        }
        if (runtime != NULL) {
            uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
            if (!load_native_patch(runtime, &next_preset, actions)) {
                return false;
            }
        }
        *preset = next_preset;
        memcpy(selectors, selected, sizeof(selected));
        return true;
    }
    next_preset = *preset;
    for (index = 0u; index < sizeof(tap_sync_families) / sizeof(tap_sync_families[0]); ++index) {
        gp100_effect_family_t family = tap_sync_families[index].family;
        int16_t sync_open;
        bool enabled;
        uint32_t id;
        int algorithm;
        size_t storage;

        if (!gp100_settings_get_i16(settings, tap_sync_families[index].setting,
                                         &sync_open) ||
            !gp100_preset_effect_enabled(preset, family, &enabled)) {
            return false;
        }
        if (sync_open == 0 || (runtime == NULL && !enabled)) {
            continue;
        }
        if (!gp100_preset_algorithm_id(preset, family, &id)) {
            return false;
        }
        algorithm = gp100_effect_algorithm_index(family, id);
        if (algorithm < 0) {
            return false;
        }
        selected[family] = gp100_effect_tempo_sync_selector_slots(
            family, (size_t)algorithm);
        for (storage = 0u; selected[family] != 0u &&
             storage < GP100_PARAMETERS_PER_FAMILY; ++storage) {
            int parameter;
            uint16_t applied;

            if ((selected[family] & (uint16_t)(UINT16_C(1) << storage)) == 0u) {
                continue;
            }
            parameter = gp100_effect_parameter_index_for_storage_slot(
                family, (size_t)algorithm, (uint8_t)storage);
            if (parameter < 0 || !gp100_preset_set_parameter(
                    &next_preset, family, (size_t)parameter, 1.0f, &applied)) {
                return false;
            }
            changed[family] |= applied;
        }
        if (runtime != NULL && runtime->slots[family].algorithm_loaded &&
            runtime->slots[family].algorithm_index == (uint16_t)algorithm) {
            changed[family] |= selected[family];
        } else {
            changed[family] = 0u;
        }
    }
    if (runtime != NULL) {
        for (index = 0u; index < sizeof(tap_sync_families) / sizeof(tap_sync_families[0]); ++index) {
            gp100_effect_family_t family = tap_sync_families[index].family;
            size_t slot;

            for (slot = 0u; changed[family] != 0u &&
                 slot < GP100_PARAMETERS_PER_FAMILY; ++slot) {
                if ((changed[family] & (uint16_t)(UINT16_C(1) << slot)) != 0u) {
                    (void)gp100_preset_parameter(
                        &next_preset, family, slot, &runtime->slots[family].parameters[slot]);
                }
            }
        }
    }
    *preset = next_preset;
    memcpy(selectors, selected, sizeof(selected));
    return true;
}

bool gp100_effect_runtime_tap_sync_algorithm_changed(
    const gp100_preset_record_t *before,
    const gp100_preset_record_t *after)
{
    size_t index;

    for (index = 0u; index < sizeof(tap_sync_families) / sizeof(tap_sync_families[0]); ++index) {
        gp100_effect_family_t family = tap_sync_families[index].family;
        uint32_t previous;
        uint32_t next;

        if (gp100_preset_algorithm_id(before, family, &previous) &&
            gp100_preset_algorithm_id(after, family, &next) &&
            previous != next) {
            return true;
        }
    }
    return false;
}

gp100_control_update_result_t
gp100_effect_runtime_set_control_target(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    size_t target_index,
    float value,
    uint16_t *changed_slots)
{
    gp100_control_target_t target;
    gp100_effect_slot_state_t *slot;
    int algorithm_index;

    if (runtime == NULL ||
        !gp100_preset_control_target(preset, target_index, &target)) {
        return GP100_CONTROL_UPDATE_INVALID;
    }
    if (!target.assigned) {
        return GP100_CONTROL_UPDATE_UNASSIGNED;
    }
    algorithm_index = gp100_effect_algorithm_index(target.family,
                                                         target.algorithm_id);
    if (algorithm_index < 0) {
        return GP100_CONTROL_UPDATE_INVALID;
    }
    slot = &runtime->slots[(size_t)target.family];
    if (!slot->algorithm_loaded ||
        slot->algorithm_index != (uint16_t)algorithm_index) {
        return GP100_CONTROL_UPDATE_INACTIVE;
    }
    if (!gp100_effect_runtime_set_parameter(
            runtime, target.family, (size_t)algorithm_index,
            target.parameter_index, value, changed_slots)) {
        return GP100_CONTROL_UPDATE_INVALID;
    }
    return GP100_CONTROL_UPDATE_APPLIED;
}
