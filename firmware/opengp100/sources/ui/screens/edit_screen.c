#include "edit_screen.h"

#include "ui/render/knob_ui.h"
#include "ui/render/mode_chip.h"

#include "ui/render/effect_art.h"
#include "ui/ui_palette.h"
#include "storage/patch_chain.h"

#include "play_screen.h"

#include <string.h>

enum {
    kChainY = 36,
    kChainHeight = 45,
    kChainCardWidth = 37,
    kChainCardStride = 40,
    kPanelY = 86,
    kPanelHeight = 42,
    kMaxActionMenuItems = 4,
    kBarY = 106,
    kBarWidth = 43,
    kBarHeight = 4
};

static const int32_t slot_left[GP100_EDIT_PARAMETER_SLOTS] = {
    1, 54, 109
};

static const int32_t slot_center[GP100_EDIT_PARAMETER_SLOTS] = {
    25, 80, 134
};

static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (destination == NULL || capacity == 0u || source == NULL) {
        return false;
    }
    length = strlen(source);
    if (length >= capacity) {
        length = capacity - 1u;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static size_t fixed_slot_count(gp100_footswitch_mode_t mode)
{
    return mode == GP100_FOOTSWITCH_MODE_STOMP ? 2u : 1u;
}

static int8_t assigned_stomp_switch(
    const gp100_preset_record_t *preset,
    gp100_effect_family_t family)
{
    size_t footswitch;
    size_t assignment;

    for (footswitch = 0u;
         footswitch < GP100_STOMP_FOOTSWITCH_COUNT;
         ++footswitch) {
        for (assignment = 0u;
             assignment < GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
             ++assignment) {
            uint16_t assigned;
            if (!gp100_preset_control_assignment(
                    preset, footswitch, assignment, &assigned)) {
                return -2;
            }
            if (assigned == (uint16_t)family) {
                return (int8_t)footswitch;
            }
        }
    }
    return -1;
}

bool gp100_edit_screen_parameter_page(
    gp100_footswitch_mode_t footswitch_mode,
    size_t parameter_index,
    size_t *page_index,
    size_t *slot_index)
{
    size_t display_index;

    if ((footswitch_mode != GP100_FOOTSWITCH_MODE_PATCH &&
         footswitch_mode != GP100_FOOTSWITCH_MODE_STOMP) ||
        parameter_index >= GP100_PARAMETERS_PER_FAMILY ||
        page_index == NULL || slot_index == NULL) {
        return false;
    }
    display_index = fixed_slot_count(footswitch_mode) + parameter_index;
    *page_index = display_index / GP100_EDIT_PARAMETER_SLOTS;
    *slot_index = display_index % GP100_EDIT_PARAMETER_SLOTS;
    return true;
}

void gp100_edit_controller_init(
    gp100_edit_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }
    *controller = (gp100_edit_controller_t){0};
}

gp100_edit_result_t gp100_edit_controller_move(
    gp100_edit_controller_t *controller,
    bool increase)
{
    size_t next;

    if (controller == NULL ||
        controller->selected_position >= GP100_EDIT_CHAIN_SIZE) {
        return GP100_EDIT_RESULT_INVALID;
    }
    next = controller->selected_position;
    if (increase && next + 1u < GP100_EDIT_CHAIN_SIZE) {
        ++next;
    } else if (!increase && next != 0u) {
        --next;
    }
    if (next == controller->selected_position && controller->page_index == 0u) {
        return GP100_EDIT_RESULT_UNCHANGED;
    }
    controller->selected_position = next;
    controller->page_index = 0u;
    return GP100_EDIT_RESULT_NAVIGATED;
}

gp100_edit_result_t gp100_edit_controller_next_page(
    gp100_edit_controller_t *controller,
    const gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode)
{
    gp100_edit_screen_snapshot_t snapshot;

    if (controller == NULL || preset == NULL ||
        controller->manage_signal_chain ||
        !gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, preset, footswitch_mode,
            controller->selected_position, controller->page_index,
            false, false) || snapshot.page_count == 0u) {
        return GP100_EDIT_RESULT_INVALID;
    }
    controller->page_index =
        (controller->page_index + 1u) % snapshot.page_count;
    return GP100_EDIT_RESULT_NAVIGATED;
}

gp100_edit_result_t gp100_edit_controller_set_chain_management(
    gp100_edit_controller_t *controller,
    bool enabled)
{
    if (controller == NULL ||
        controller->selected_position >= GP100_EDIT_CHAIN_SIZE) {
        return GP100_EDIT_RESULT_INVALID;
    }
    if (controller->manage_signal_chain == enabled) {
        return GP100_EDIT_RESULT_UNCHANGED;
    }
    if (enabled) {
        controller->parameter_position = controller->selected_position;
        controller->parameter_page = controller->page_index;
        controller->selected_position = 0u;
        controller->page_index = 0u;
    } else {
        controller->selected_position = controller->parameter_position;
        controller->page_index = controller->parameter_page;
        controller->para_hold_release_pending = false;
    }
    controller->manage_signal_chain = enabled;
    return GP100_EDIT_RESULT_NAVIGATED;
}

static gp100_edit_result_t set_stomp_assignment(
    gp100_preset_record_t *preset,
    gp100_effect_family_t family,
    int desired_switch)
{
    uint16_t assignments[GP100_STOMP_FOOTSWITCH_COUNT]
                        [GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH];
    size_t footswitch;
    size_t slot;
    size_t target_slot = GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;

    if (desired_switch < -1 ||
        desired_switch >= (int)GP100_STOMP_FOOTSWITCH_COUNT) {
        return GP100_EDIT_RESULT_INVALID;
    }
    for (footswitch = 0u;
         footswitch < GP100_STOMP_FOOTSWITCH_COUNT;
         ++footswitch) {
        for (slot = 0u;
             slot < GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
             ++slot) {
            if (!gp100_preset_control_assignment(
                    preset, footswitch, slot,
                    &assignments[footswitch][slot])) {
                return GP100_EDIT_RESULT_INVALID;
            }
            if (assignments[footswitch][slot] == (uint16_t)family) {
                assignments[footswitch][slot] = UINT16_MAX;
            }
        }
    }
    if (desired_switch >= 0) {
        for (slot = 0u;
             slot < GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
             ++slot) {
            if (assignments[(size_t)desired_switch][slot] == UINT16_MAX) {
                target_slot = slot;
                break;
            }
        }
        if (target_slot == GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH) {
            return GP100_EDIT_RESULT_UNCHANGED;
        }
        assignments[(size_t)desired_switch][target_slot] = (uint16_t)family;
    }
    for (footswitch = 0u;
         footswitch < GP100_STOMP_FOOTSWITCH_COUNT;
         ++footswitch) {
        for (slot = 0u;
             slot < GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
             ++slot) {
            if (!gp100_preset_set_control_assignment(
                    preset, footswitch, slot,
                    assignments[footswitch][slot])) {
                return GP100_EDIT_RESULT_INVALID;
            }
        }
    }
    return GP100_EDIT_RESULT_PRESET_CHANGED;
}

static gp100_edit_result_t move_chain_effect(
    gp100_edit_controller_t *controller,
    gp100_preset_record_t *preset,
    bool increase)
{
    uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
    gp100_effect_family_t selected_family;
    gp100_effect_family_t neighbor_family;
    size_t next = controller->selected_position;
    size_t family;

    if (increase && next + 1u < GP100_EDIT_CHAIN_SIZE) {
        ++next;
    } else if (!increase && next != 0u) {
        --next;
    } else {
        return GP100_EDIT_RESULT_UNCHANGED;
    }
    if (!gp100_preset_effect_at_position(
            preset, controller->selected_position, &selected_family) ||
        !gp100_preset_effect_at_position(
            preset, next, &neighbor_family)) {
        return GP100_EDIT_RESULT_INVALID;
    }
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        size_t position;

        if (!gp100_preset_effect_position(
                preset, (gp100_effect_family_t)family, &position)) {
            return GP100_EDIT_RESULT_INVALID;
        }
        positions[family] = (uint16_t)position;
    }
    positions[(size_t)selected_family] = (uint16_t)next;
    positions[(size_t)neighbor_family] =
        (uint16_t)controller->selected_position;
    if (!gp100_preset_set_effect_positions(preset, positions)) {
        return GP100_EDIT_RESULT_INVALID;
    }
    controller->selected_position = next;
    return GP100_EDIT_RESULT_PRESET_CHANGED;
}

float gp100_edit_parameter_delta(const gp100_parameter_rule_t *rule,
                             uint8_t acceleration)
{
    float step = rule->minimum > 0.0f && rule->minimum < 1.0f
        ? 0.1f : 1.0f;
    unsigned step_count = (unsigned)((rule->maximum - rule->minimum) / step);
    unsigned scale_shift = step_count >= 1001u ? 6u
                         : step_count >= 81u ? 1u : 0u;
    unsigned effective = acceleration == 0u ? 1u
                       : acceleration > 8u ? 5u : acceleration;

    return step * (float)(1u + ((effective - 1u) << scale_shift));
}

gp100_edit_result_t gp100_edit_controller_adjust(
    gp100_edit_controller_t *controller,
    gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode,
    size_t control,
    bool increase,
    uint8_t acceleration)
{
    gp100_edit_screen_snapshot_t snapshot;
    const gp100_edit_parameter_slot_t *slot;
    bool enabled;
    gp100_edit_result_t result;

    if (controller == NULL || preset == NULL ||
        control >= GP100_EDIT_PARAMETER_SLOTS ||
        !gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, preset, footswitch_mode,
            controller->selected_position, controller->page_index,
            controller->manage_signal_chain, false)) {
        return GP100_EDIT_RESULT_INVALID;
    }
    if (controller->manage_signal_chain) {
        if (control == 2u) {
            result = move_chain_effect(controller, preset, increase);
        } else if (control == 1u) {
            const size_t count = gp100_effect_algorithm_count(snapshot.selected_family);
            size_t algorithm = snapshot.algorithm_index;
            do {
                if ((increase && algorithm + 1u >= count) || (!increase && algorithm == 0u)) {
                    return GP100_EDIT_RESULT_UNCHANGED;
                }
                algorithm = increase ? algorithm + 1u : algorithm - 1u;
            } while (!gp100_effect_available(snapshot.selected_family, algorithm));
            if (!gp100_preset_select_algorithm(preset, snapshot.selected_family, algorithm)) {
                return GP100_EDIT_RESULT_INVALID;
            }
            if (controller->selected_position == controller->parameter_position) {
                controller->parameter_page = 0u;
            }
            result = GP100_EDIT_RESULT_PRESET_CHANGED;
        } else if (control == 0u) {
            enabled = snapshot.effect_enabled[(size_t)snapshot.selected_family];
            if (enabled == increase) {
                return GP100_EDIT_RESULT_UNCHANGED;
            }
            result = gp100_preset_set_effect_enabled(
                         preset, snapshot.selected_family, increase)
                ? GP100_EDIT_RESULT_PRESET_CHANGED
                : GP100_EDIT_RESULT_INVALID;
        } else {
            return GP100_EDIT_RESULT_UNCHANGED;
        }
    } else {
        slot = &snapshot.slots[control];
        if (!slot->present) {
            return GP100_EDIT_RESULT_UNCHANGED;
        }
        switch (slot->kind) {
        case GP100_EDIT_SLOT_EFFECT_ENABLED:
            enabled = snapshot.effect_enabled[(size_t)snapshot.selected_family];
            if (enabled == increase) {
                return GP100_EDIT_RESULT_UNCHANGED;
            }
            result = gp100_preset_set_effect_enabled(
                         preset, snapshot.selected_family, increase)
                ? GP100_EDIT_RESULT_PRESET_CHANGED
                : GP100_EDIT_RESULT_INVALID;
            break;
        case GP100_EDIT_SLOT_STOMP_SWITCH: {
            int desired = snapshot.stomp_switch + (increase ? 1 : -1);

            if (desired < -1) {
                desired = -1;
            } else if (desired > 1) {
                desired = 1;
            }
            if (desired == snapshot.stomp_switch) {
                return GP100_EDIT_RESULT_UNCHANGED;
            }
            result = set_stomp_assignment(
                preset, snapshot.selected_family, desired);
            break;
        }
        case GP100_EDIT_SLOT_PARAMETER: {
            float value = slot->raw_value;
            float delta;

            {
                gp100_parameter_rule_t rule = {
                    0u, slot->minimum, slot->maximum, 0.0f
                };
                delta = gp100_edit_parameter_delta(&rule, acceleration);
            }
            value += increase ? delta : -delta;
            if (value < slot->minimum) {
                value = slot->minimum;
            } else if (value > slot->maximum) {
                value = slot->maximum;
            }
            if (value == slot->raw_value) {
                return GP100_EDIT_RESULT_UNCHANGED;
            }
            result = gp100_preset_set_parameter(
                         preset, snapshot.selected_family,
                         slot->parameter_index, value, NULL)
                ? GP100_EDIT_RESULT_PRESET_CHANGED
                : GP100_EDIT_RESULT_INVALID;
            break;
        }
        default:
            return GP100_EDIT_RESULT_UNCHANGED;
        }
    }
    if (result == GP100_EDIT_RESULT_PRESET_CHANGED) {
        gp100_preset_update_crc(preset);
    }
    return result;
}

static gp100_edit_event_result_t event_result_from_edit(
    gp100_edit_result_t result)
{
    switch (result) {
    case GP100_EDIT_RESULT_NAVIGATED:
        return GP100_EDIT_EVENT_NAVIGATED;
    case GP100_EDIT_RESULT_PRESET_CHANGED:
        return GP100_EDIT_EVENT_PRESET_CHANGED;
    case GP100_EDIT_RESULT_UNCHANGED:
        return GP100_EDIT_EVENT_UNHANDLED;
    default:
        return GP100_EDIT_EVENT_INVALID;
    }
}

gp100_edit_event_result_t gp100_edit_controller_handle_event(
    gp100_edit_controller_t *controller,
    gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    uint16_t *logical_preset,
    gp100_footswitch_mode_t footswitch_mode,
    bool *drum_enabled,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_edit_result_t result;
    gp100_ui_mode_t target;

    if (controller == NULL || preset == NULL || settings == NULL ||
        logical_preset == NULL || drum_enabled == NULL || event == NULL ||
        next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_EDIT_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        if (decoded.control == GP100_ENCODER_PARA) {
            result = gp100_edit_controller_move(
                controller,
                decoded.action == GP100_BUTTON_PRESS);
        } else if (decoded.control < GP100_ENCODER_PARA) {
            result = gp100_edit_controller_adjust(
                controller, preset, footswitch_mode, decoded.control,
                decoded.action == GP100_BUTTON_PRESS,
                decoded.amount);
        } else {
            return GP100_EDIT_EVENT_INVALID;
        }
        return event_result_from_edit(result);
    }
    if (decoded.kind == GP100_UI_CONTROL_FOOTSWITCH_PAIR) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_TUNER;
        } else if (decoded.action == GP100_BUTTON_REPEAT) {
            *next_mode = GP100_UI_MODE_LOOPER;
        } else {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        return GP100_EDIT_EVENT_MODE_CHANGED;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_PARA) {
        if (decoded.action == GP100_BUTTON_HOLD &&
            !controller->manage_signal_chain) {
            result = gp100_edit_controller_set_chain_management(
                controller, true);
            if (result == GP100_EDIT_RESULT_NAVIGATED) {
                controller->para_hold_release_pending = true;
            }
        } else if (decoded.action == GP100_BUTTON_RELEASE) {
            if (controller->para_hold_release_pending) {
                /* The release ending a long press only clears the latch. */
                controller->para_hold_release_pending = false;
                return GP100_EDIT_EVENT_UNHANDLED;
            }
            result = controller->manage_signal_chain
                ? gp100_edit_controller_set_chain_management(
                      controller, false)
                : gp100_edit_controller_next_page(
                      controller, preset, footswitch_mode);
        } else {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        return event_result_from_edit(result);
    }
    if (decoded.control == GP100_BUTTON_DRUM) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *drum_enabled = !*drum_enabled;
            return GP100_EDIT_EVENT_DRUM_TOGGLED;
        }
        if (decoded.action == GP100_BUTTON_HOLD) {
            *next_mode = GP100_UI_MODE_DRUM;
            return GP100_EDIT_EVENT_MODE_CHANGED;
        }
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if ((decoded.control == GP100_BUTTON_FOOTSWITCH_MINUS ||
         decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS) &&
        (decoded.action == GP100_BUTTON_RELEASE ||
         decoded.action == GP100_BUTTON_REPEAT)) {
        gp100_play_action_result_t footswitch_result =
            gp100_play_footswitch(
                settings, preset, logical_preset,
                decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS ?
                    1u : 0u);

        if (footswitch_result == GP100_PLAY_ACTION_SELECT_PRESET) {
            return GP100_EDIT_EVENT_PRESET_SELECTED;
        }
        if (footswitch_result == GP100_PLAY_ACTION_PRESET_CHANGED) {
            return GP100_EDIT_EVENT_PRESET_CHANGED;
        }
        if (footswitch_result == GP100_PLAY_ACTION_UNCHANGED) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        return GP100_EDIT_EVENT_INVALID;
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    switch ((gp100_button_id_t)decoded.control) {
    case GP100_BUTTON_EXIT:
        target = GP100_UI_MODE_PLAY;
        break;
    case GP100_BUTTON_GLOBAL:
        target = GP100_UI_MODE_GLOBAL;
        break;
    case GP100_BUTTON_EDIT:
        target = GP100_UI_MODE_EDIT;
        break;
    case GP100_BUTTON_SAVE:
        target = GP100_UI_MODE_SAVE;
        break;
    default:
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    *next_mode = target;
    return GP100_EDIT_EVENT_MODE_CHANGED;
}

bool gp100_edit_screen_fill_parameter(
    gp100_edit_parameter_slot_t *slot,
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    const float stored[GP100_PARAMETERS_PER_FAMILY])
{
    gp100_parameter_rule_t rule;
    uint8_t storage_slot;
    const char *name;

    if (!gp100_effect_parameter_storage_slot(
            family, algorithm_index, parameter_index, &storage_slot) ||
        !gp100_effect_parameter_rule(
            family, algorithm_index, parameter_index, stored, &rule)) {
        return false;
    }
    name = gp100_effect_parameter_name(
        family, algorithm_index, parameter_index);
    if (name == NULL ||
        !copy_text(slot->label, sizeof(slot->label), name) ||
        gp100_effect_format_parameter_value(
            family, algorithm_index, parameter_index,
            stored[storage_slot], stored, slot->value,
            sizeof(slot->value)) < 0) {
        return false;
    }
    slot->kind = GP100_EDIT_SLOT_PARAMETER;
    slot->raw_value = stored[storage_slot];
    slot->minimum = rule.minimum;
    slot->maximum = rule.maximum;
    slot->parameter_index = parameter_index;
    slot->present = true;
    return true;
}

bool gp100_edit_screen_snapshot_from_preset(
    gp100_edit_screen_snapshot_t *snapshot,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode,
    size_t selected_position,
    size_t page_index,
    bool manage_signal_chain,
    bool preset_modified)
{
    gp100_edit_screen_snapshot_t next;
    float stored[GP100_PARAMETERS_PER_FAMILY];
    size_t family_index;
    size_t item_index;
    size_t slot_index;
    int algorithm_index;

    if (snapshot == NULL || preset == NULL ||
        logical_preset >= GP100_TOTAL_LOGICAL_PRESETS ||
        selected_position >= GP100_EDIT_CHAIN_SIZE ||
        (footswitch_mode != GP100_FOOTSWITCH_MODE_PATCH &&
         footswitch_mode != GP100_FOOTSWITCH_MODE_STOMP)) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.logical_preset = logical_preset;
    next.footswitch_mode = footswitch_mode;
    next.selected_position = selected_position;
    next.manage_signal_chain = manage_signal_chain;
    next.preset_modified = preset_modified;
    gp100_preset_name(preset, next.preset_name);

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        if (!gp100_preset_effect_at_position(
                preset, family_index, &next.chain[family_index]) ||
            !gp100_preset_effect_enabled(
                preset, (gp100_effect_family_t)family_index,
                &next.effect_enabled[family_index])) {
            return false;
        }
    }
    next.selected_family = next.chain[selected_position];
    if (!gp100_preset_algorithm_id(
            preset, next.selected_family, &next.algorithm_id)) {
        return false;
    }
    algorithm_index = gp100_effect_algorithm_index(
        next.selected_family, next.algorithm_id);
    if (algorithm_index < 0) {
        return false;
    }
    next.algorithm_index = (size_t)algorithm_index;
    next.algorithm_name = gp100_effect_algorithm_name(
        next.selected_family, next.algorithm_index);
    next.parameter_count = gp100_effect_parameter_count(
        next.selected_family, next.algorithm_index);
    if (next.algorithm_name == NULL) {
        return false;
    }
    next.selection_label = next.algorithm_name;
    for (item_index = 0u;
         item_index < GP100_PARAMETERS_PER_FAMILY;
         ++item_index) {
        if (!gp100_preset_parameter(
                preset, next.selected_family, item_index,
                &stored[item_index])) {
            return false;
        }
    }
    next.stomp_switch = assigned_stomp_switch(preset, next.selected_family);
    if (next.stomp_switch == -2) {
        return false;
    }
    next.page_count =
        (fixed_slot_count(footswitch_mode) + next.parameter_count +
         GP100_EDIT_PARAMETER_SLOTS - 1u) /
        GP100_EDIT_PARAMETER_SLOTS;
    if (next.page_count == 0u) {
        next.page_count = 1u;
    }
    if (page_index >= next.page_count) {
        return false;
    }
    next.page_index = page_index;

    if (!manage_signal_chain) {
        for (slot_index = 0u;
             slot_index < GP100_EDIT_PARAMETER_SLOTS;
             ++slot_index) {
            gp100_edit_parameter_slot_t *slot = &next.slots[slot_index];
            item_index = page_index * GP100_EDIT_PARAMETER_SLOTS +
                         slot_index;
            if (item_index == 0u) {
                slot->kind = GP100_EDIT_SLOT_EFFECT_ENABLED;
                slot->present = true;
                (void)copy_text(slot->label, sizeof(slot->label), "ON/OFF");
                (void)copy_text(
                    slot->value, sizeof(slot->value),
                    next.effect_enabled[next.selected_family] ? "ON" : "OFF");
                slot->raw_value =
                    next.effect_enabled[next.selected_family] ? 1.0f : 0.0f;
                slot->maximum = 1.0f;
            } else if (footswitch_mode == GP100_FOOTSWITCH_MODE_STOMP &&
                       item_index == 1u) {
                slot->kind = GP100_EDIT_SLOT_STOMP_SWITCH;
                slot->present = true;
                (void)copy_text(slot->label, sizeof(slot->label), "FS ASSIGN");
                if (next.stomp_switch == GP100_STOMP_FOOTSWITCH_FS1) {
                    (void)copy_text(slot->value, sizeof(slot->value), "FS1");
                } else if (next.stomp_switch ==
                           GP100_STOMP_FOOTSWITCH_FS2) {
                    (void)copy_text(slot->value, sizeof(slot->value), "FS2");
                } else {
                    (void)copy_text(slot->value, sizeof(slot->value), "OFF");
                }
            } else {
                size_t parameter_index =
                    item_index - fixed_slot_count(footswitch_mode);
                if (parameter_index < next.parameter_count &&
                    !gp100_edit_screen_fill_parameter(
                        slot, next.selected_family, next.algorithm_index,
                        parameter_index, stored)) {
                    return false;
                }
            }
        }
    }
    *snapshot = next;
    return true;
}

bool gp100_edit_screen_default_theme(
    gp100_edit_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    static const uint16_t effect_colors[GP100_EFFECT_FAMILY_COUNT] =
        GP100_UI_EFFECT_COLORS;
    size_t index;

    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->large_font = large_font != NULL ? large_font : regular_font;
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->chain_selection_rgb565 = GP100_UI_RGB565(255, 76, 76);
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->value_rgb565 = GP100_UI_CONTROL;
    theme->track_rgb565 = GP100_UI_TRACK;
    theme->disabled_rgb565 = GP100_UI_MUTED;
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        theme->effect_rgb565[index] =
            effect_colors[index];
    }
    return true;
}

static bool draw_text(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t x,
                      int32_t y,
                      const char *text,
                      gp100_text_alignment_t alignment,
                      uint16_t foreground,
                      uint16_t background)
{
    gp100_text_style_t style = {
        foreground, background, alignment, true
    };
    return gp100_display_draw_text(
        canvas, font, x, y, text, &style);
}

static void draw_border(gp100_display_canvas_t *canvas,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        uint16_t color)
{
    (void)gp100_display_draw_line(canvas, x, y,
                                       x + width - 1, y, color);
    (void)gp100_display_draw_line(canvas, x, y,
                                       x, y + height - 1, color);
    (void)gp100_display_draw_line(canvas, x + width - 1, y,
                                       x + width - 1, y + height - 1, color);
    (void)gp100_display_draw_line(canvas, x, y + height - 1,
                                       x + width - 1, y + height - 1, color);
}

static void format_page(char output[8], size_t page, size_t total)
{
    size_t index = 0u;

    output[index++] = (char)('0' + (page + 1u) / 10u);
    if (output[0] == '0') {
        index = 0u;
    }
    output[index++] = (char)('0' + (page + 1u) % 10u);
    output[index++] = '/';
    if (total >= 10u) {
        output[index++] = (char)('0' + total / 10u);
    }
    output[index++] = (char)('0' + total % 10u);
    output[index] = '\0';
}

static bool draw_fitted_text(gp100_display_canvas_t *canvas,
                             const gp100_bitmap_font_t *font,
                             int32_t x, int32_t y, const char *text,
                             uint16_t max_width, uint16_t foreground,
                             uint16_t background)
{
    char fitted[32];
    uint16_t width;
    uint16_t height;

    if (!copy_text(fitted, sizeof(fitted), text)) {
        return false;
    }
    size_t length = strlen(fitted);
    while (gp100_display_measure_text(font, fitted, &width, &height) &&
           width > max_width && length != 0u) {
        fitted[--length] = '\0';
    }
    return draw_text(canvas, font, x, y, fitted, GP100_TEXT_ALIGN_CENTER,
                     foreground, background);
}

const char *gp100_edit_screen_model_name(gp100_effect_family_t family,
                                         size_t algorithm,
                                         char user_name[8])
{
    if (algorithm == GP100_PATCH_UNKNOWN_ALGORITHM) {
        return "Unknown FX";
    }
    const char *name = gp100_effect_algorithm_name(family, algorithm);
    if (name != NULL) {
        return name;
    }
    if (!gp100_effect_algorithm_is_user_slot(family, algorithm)) {
        return "Unknown";
    }
    size_t first = algorithm;
    while (first != 0u && gp100_effect_algorithm_is_user_slot(family, first - 1u)) {
        --first;
    }
    const size_t slot = algorithm - first + 1u;
    memcpy(user_name, "User 00", sizeof("User 00"));
    user_name[5] = (char)('0' + slot / 10u);
    user_name[6] = (char)('0' + slot % 10u);
    return user_name;
}

static void format_page_label(char output[16], size_t page, size_t total)
{
    char number[8];

    format_page(number, page, total);
    memcpy(output, "PAGE ", 5u);
    memcpy(output + 5, number, strlen(number) + 1u);
}

static uint16_t accent_color(const gp100_edit_screen_snapshot_t *snapshot,
                             const gp100_edit_screen_theme_t *theme)
{
    return snapshot->accent_rgb565 != 0u ? snapshot->accent_rgb565
                                         : theme->selected_rgb565;
}

static bool render_header(gp100_display_canvas_t *canvas,
                          const gp100_edit_screen_snapshot_t *snapshot,
                          const gp100_edit_screen_theme_t *theme)
{
    char code[4];
    char page[16];

    if (!gp100_play_screen_patch_code(snapshot->logical_preset, code)) {
        return false;
    }
    if (snapshot->native_chain) {
        /* Mode name on a colored chip, page counter underneath (only for multi-page effects). */
        format_page_label(page, snapshot->page_index, snapshot->page_count);
        if (snapshot->mode_label != NULL &&
            !gp100_mode_chip_draw(canvas, theme->small_font, snapshot->mode_label,
                                  accent_color(snapshot, theme), theme->background_rgb565,
                                  158, 1)) {
            return false;
        }
        if (snapshot->page_count > 1u &&
            !draw_text(canvas, theme->small_font, 157, 15, page,
                       GP100_TEXT_ALIGN_RIGHT,
                       theme->foreground_rgb565, theme->background_rgb565)) {
            return false;
        }
    } else {
        format_page(page, snapshot->page_index, snapshot->page_count);
        if (!draw_text(canvas, theme->small_font, 157, 4, page,
                       GP100_TEXT_ALIGN_RIGHT,
                       theme->foreground_rgb565, theme->background_rgb565) ||
            (snapshot->mode_label != NULL &&
             !draw_text(canvas, theme->small_font, 157, 19, snapshot->mode_label,
                        GP100_TEXT_ALIGN_RIGHT, theme->selected_rgb565,
                        theme->background_rgb565))) {
            return false;
        }
    }
    return draw_text(canvas, theme->large_font, 2, 1, code,
                     GP100_TEXT_ALIGN_LEFT,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 2, 19,
                     snapshot->preset_name, GP100_TEXT_ALIGN_LEFT,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           (!snapshot->preset_modified ||
            draw_text(canvas, theme->regular_font, 50, 1, "*",
                      GP100_TEXT_ALIGN_LEFT,
                      theme->foreground_rgb565,
                      theme->background_rgb565)) &&
           (snapshot->selection_label == NULL ||
            draw_fitted_text(canvas, theme->small_font, 80, 28,
                             snapshot->selection_label, 136u,
                             theme->foreground_rgb565, theme->background_rgb565));
}

static bool render_native_card(gp100_display_canvas_t *canvas,
                               const gp100_edit_screen_snapshot_t *snapshot,
                               const gp100_edit_screen_theme_t *theme,
                               size_t position, int32_t x)
{
    const gp100_edit_chain_card_t *card = &snapshot->cards[position];
    const bool insertion = snapshot->manage_signal_chain &&
                           position == snapshot->occupied_count;
    const bool selected = position == snapshot->selected_position &&
                          (card->present || insertion);
    const bool available = card->present && gp100_effect_available(card->family, card->algorithm);
    const bool enabled = available && card->enabled;
    const int32_t middle = x + kChainCardWidth / 2;
    const uint16_t accent = available ? theme->effect_rgb565[card->family]
                                      : theme->track_rgb565;
    /* Every unselected box looks the same; only the selected one (red) stands out, and a
     * bypassed effect gets a darker border (and dimmed art). The family colour stays on the icon. */
    uint16_t border = enabled ? theme->disabled_rgb565 : theme->track_rgb565;
    if (selected) {
        border = theme->chain_selection_rgb565;
        draw_border(canvas, x + 1, kChainY + 1,
                    kChainCardWidth - 2, kChainHeight - 2, border);
    }
    draw_border(canvas, x, kChainY, kChainCardWidth, kChainHeight, border);
    if (card->present) {
        char user_name[8];
        char label[6];
        const char *name = gp100_edit_screen_model_name(
            card->family, card->algorithm, user_name);
        const size_t length = strlen(name);
        (void)copy_text(label, sizeof(label), name);
        if (length > 5u &&
            ((name[length - 1u] >= '0' && name[length - 1u] <= '9') ||
             (name[length - 2u] >= '0' && name[length - 2u] <= '9'))) {
            label[3] = name[length - 2u];
            label[4] = name[length - 1u];
        }
        gp100_effect_art_draw(canvas, middle - 13, kChainY + 4,
                               GP100_EFFECT_ART_WIDTH, GP100_EFFECT_ART_HEIGHT, card->family,
                               card->algorithm, accent, theme->background_rgb565,
                               enabled);
        if (card->expression_assigned) {
            const int32_t badge_x = x + kChainCardWidth - 22;
            (void)gp100_display_fill_rect(canvas, badge_x, kChainY + 2, 20u, 8u,
                                         theme->background_rgb565);
            if (!draw_text(canvas, theme->small_font, badge_x + 1, kChainY + 2, "EXP",
                           GP100_TEXT_ALIGN_LEFT, theme->selected_rgb565,
                           theme->background_rgb565)) {
                return false;
            }
        }
        if (!available && !draw_text(canvas, theme->small_font, x + 4, kChainY + 2, "!",
                                     GP100_TEXT_ALIGN_LEFT, theme->chain_selection_rgb565,
                                     theme->background_rgb565)) {
            return false;
        }
        return draw_fitted_text(canvas, theme->small_font, middle, kChainY + 33,
                                label, kChainCardWidth - 6,
                                enabled ? theme->foreground_rgb565 : theme->disabled_rgb565,
                                theme->background_rgb565);
    }
    if (insertion) {
        const uint16_t color = selected ? theme->selected_rgb565
                                        : theme->disabled_rgb565;
        (void)gp100_display_fill_rect(canvas, middle - 6, kChainY + 16, 13u, 2u, color);
        (void)gp100_display_fill_rect(canvas, middle, kChainY + 10, 2u, 14u, color);
        return draw_text(canvas, theme->small_font, middle, kChainY + 33, "ADD",
                         GP100_TEXT_ALIGN_CENTER, color, theme->background_rgb565);
    }
    (void)gp100_display_fill_rect(canvas, middle - 3, kChainY + 20, 7u, 1u,
                                  theme->track_rgb565);
    return true;
}

static bool render_chain(gp100_display_canvas_t *canvas,
                         const gp100_edit_screen_snapshot_t *snapshot,
                         const gp100_edit_screen_theme_t *theme)
{
    size_t window = snapshot->selected_position / GP100_EDIT_VISIBLE_EFFECTS;
    size_t first = window * GP100_EDIT_VISIBLE_EFFECTS;
    size_t visible;
    const size_t capacity = snapshot->native_chain ? snapshot->chain_capacity
                                                    : GP100_EDIT_CHAIN_SIZE;

    for (visible = 0u; visible < GP100_EDIT_VISIBLE_EFFECTS; ++visible) {
        size_t position = first + visible;
        gp100_effect_family_t family;
        uint16_t color;
        int32_t x;

        if (position >= capacity) {
            break;
        }
        x = 1 + (int32_t)visible * kChainCardStride;
        if (snapshot->native_chain) {
            if (!render_native_card(canvas, snapshot, theme, position, x)) {
                return false;
            }
            continue;
        }
        family = snapshot->chain[position];
        color = snapshot->effect_enabled[family]
                    ? theme->effect_rgb565[family]
                    : theme->disabled_rgb565;
        draw_border(canvas, x, kChainY, kChainCardWidth, kChainHeight,
                    position == snapshot->selected_position
                        ? theme->chain_selection_rgb565 : color);
        if (position == snapshot->selected_position) {
            draw_border(canvas, x + 1, kChainY + 1,
                        kChainCardWidth - 2, kChainHeight - 2,
                        theme->chain_selection_rgb565);
        }
        (void)gp100_display_fill_rect(
            canvas, x + 5, kChainY + 5, kChainCardWidth - 10,
            18u, color);
        if (!draw_text(canvas, theme->small_font,
                       x + kChainCardWidth / 2, kChainY + 28,
                       gp100_play_screen_family_label(family),
                       GP100_TEXT_ALIGN_CENTER, color,
                       theme->background_rgb565)) {
            return false;
        }
    }
    if (window != 0u) {
        (void)gp100_display_fill_triangle(
            canvas, 1, 28, 8, 24, 8, 32, theme->foreground_rgb565);
    }
    const size_t reachable = snapshot->native_chain
        ? snapshot->occupied_count + (snapshot->manage_signal_chain ? 1u : 0u)
        : capacity;
    if (first + GP100_EDIT_VISIBLE_EFFECTS < capacity &&
        first + GP100_EDIT_VISIBLE_EFFECTS < reachable) {
        (void)gp100_display_fill_triangle(
            canvas, 159, 28, 152, 24, 152, 32, theme->foreground_rgb565);
    }
    return true;
}

static void render_bar(gp100_display_canvas_t *canvas,
                       size_t slot_index,
                       const gp100_edit_parameter_slot_t *slot,
                       const gp100_edit_screen_theme_t *theme)
{
    float ratio;
    uint16_t filled;

    if (slot->maximum <= slot->minimum) {
        return;
    }
    ratio = (slot->raw_value - slot->minimum) /
            (slot->maximum - slot->minimum);
    if (ratio < 0.0f) {
        ratio = 0.0f;
    } else if (ratio > 1.0f) {
        ratio = 1.0f;
    }
    filled = (uint16_t)(ratio * (float)kBarWidth + 0.5f);
    (void)gp100_display_fill_rect(
        canvas, slot_left[slot_index], kBarY, kBarWidth, kBarHeight,
        theme->track_rgb565);
    if (filled != 0u) {
        (void)gp100_display_fill_rect(
            canvas, slot_left[slot_index], kBarY, filled, kBarHeight,
            theme->selected_rgb565);
    }
}

static bool render_parameter_panel(
    gp100_display_canvas_t *canvas,
    const gp100_edit_screen_snapshot_t *snapshot,
    const gp100_edit_screen_theme_t *theme)
{
    size_t index;

    (void)gp100_display_fill_rect(
        canvas, 0, kPanelY, GP100_UI_LOGICAL_WIDTH, kPanelHeight,
        theme->panel_rgb565);
    if (snapshot->native_chain) {
        (void)gp100_display_fill_rect(
            canvas, 0, kPanelY, GP100_UI_LOGICAL_WIDTH, 2u,
            accent_color(snapshot, theme));
    }
    if (snapshot->menu_items != NULL) {
        for (size_t item = 0; item < snapshot->menu_count; ++item) {
            const bool compact = snapshot->menu_count > 3u;      /* four entries: 8 px rows */
            const int32_t spacing = compact ? 8 : 10;
            const int32_t y = (compact ? 88 : 89) + (int32_t)item * spacing;
            const bool selected = item == snapshot->menu_selected;
            const uint16_t color = selected ? theme->selected_rgb565
                                            : theme->foreground_rgb565;
            if (selected) {
                (void)gp100_display_fill_rect(
                    canvas, 3, compact ? y : y - 1, 154u, compact ? 8u : 9u, theme->value_rgb565);
                if (!draw_text(canvas, theme->small_font, 6, y, ">",
                               GP100_TEXT_ALIGN_LEFT, color, theme->value_rgb565)) {
                    return false;
                }
            }
            if (!draw_text(canvas, theme->small_font, 18, y,
                           snapshot->menu_items[item], GP100_TEXT_ALIGN_LEFT,
                           color, theme->panel_rgb565)) {
                return false;
            }
        }
        return true;
    }
    if (snapshot->panel_title != NULL) {
        return draw_fitted_text(canvas, theme->small_font, 80, 90,
                                snapshot->panel_title, 154u,
                                theme->foreground_rgb565, theme->panel_rgb565) &&
               draw_fitted_text(canvas, theme->regular_font, 80, 103,
                                snapshot->panel_value, 154u,
                                theme->selected_rgb565, theme->panel_rgb565);
    }
    if (snapshot->manage_signal_chain && !snapshot->native_chain) {
        const char *enabled = snapshot->effect_enabled[
                                  snapshot->selected_family]
                                  ? "On" : "Off";
        return draw_text(canvas, theme->regular_font, 3, 89,
                         "Manage Signal Chain",
                         GP100_TEXT_ALIGN_LEFT,
                         theme->foreground_rgb565, theme->panel_rgb565) &&
               draw_text(canvas, theme->regular_font, 25, 110, enabled,
                         GP100_TEXT_ALIGN_CENTER,
                         theme->selected_rgb565, theme->value_rgb565) &&
               draw_text(canvas, theme->regular_font, 80, 110, "Change",
                         GP100_TEXT_ALIGN_CENTER,
                         theme->selected_rgb565, theme->value_rgb565) &&
               draw_text(canvas, theme->regular_font, 134, 110, "Move",
                         GP100_TEXT_ALIGN_CENTER,
                         theme->foreground_rgb565, theme->value_rgb565);
    }
    if (snapshot->native_chain) {
        /* Knob columns: left = Quick 1, middle = Quick 2, right = Quick 3. */
        for (index = 0u; index < GP100_EDIT_PARAMETER_SLOTS; ++index) {
            const gp100_edit_parameter_slot_t *slot = &snapshot->slots[index];
            const bool parameter = slot->kind == GP100_EDIT_SLOT_PARAMETER;
            const gp100_knob_style_t style = {
                theme->small_font, theme->value_rgb565, theme->foreground_rgb565,
                parameter ? theme->foreground_rgb565 : accent_color(snapshot, theme),
                accent_color(snapshot, theme), theme->track_rgb565
            };
            float ratio = -1.0f;

            if (!slot->present) {
                continue;
            }
            if (parameter && slot->maximum > slot->minimum) {
                ratio = (slot->raw_value - slot->minimum) / (slot->maximum - slot->minimum);
                ratio = ratio < 0.0f ? 0.0f : ratio;
            }
            if (!gp100_knob_box_draw(canvas, &style, index, slot->label, slot->value, ratio)) {
                return false;
            }
        }
        return true;
    }
    for (index = 0u; index < GP100_EDIT_PARAMETER_SLOTS; ++index) {
        const gp100_edit_parameter_slot_t *slot = &snapshot->slots[index];
        uint16_t value_color;

        if (!slot->present) {
            continue;
        }
        value_color = slot->kind == GP100_EDIT_SLOT_PARAMETER
                          ? theme->foreground_rgb565
                          : theme->selected_rgb565;
        if (!draw_fitted_text(canvas, theme->small_font, slot_center[index], 90,
                             slot->label, 48u,
                             theme->foreground_rgb565, theme->panel_rgb565) ||
            !draw_fitted_text(canvas, theme->regular_font, slot_center[index], 111,
                             slot->value, 48u, value_color, theme->value_rgb565)) {
            return false;
        }
        if (slot->kind == GP100_EDIT_SLOT_PARAMETER) {
            render_bar(canvas, index, slot, theme);
        }
    }
    return true;
}

static bool render_hint(gp100_display_canvas_t *canvas,
                        const gp100_edit_screen_snapshot_t *snapshot,
                        const gp100_edit_screen_theme_t *theme)
{
    if (snapshot->hint == NULL) {
        return true;
    }
    if (snapshot->native_chain && !snapshot->hint_is_warning) {
        /* "[PARA] keep [EXIT] cancel": bracketed words are button chips. */
        return gp100_hint_draw(canvas, theme->small_font, 3, 120, 154u, snapshot->hint,
                               theme->foreground_rgb565, theme->background_rgb565,
                               theme->disabled_rgb565, theme->panel_rgb565);
    }
    return draw_fitted_text(canvas, theme->small_font, 80, 121, snapshot->hint,
                            156u, snapshot->hint_is_warning
                                ? theme->chain_selection_rgb565 : theme->disabled_rgb565,
                            theme->panel_rgb565);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_edit_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_edit_screen_snapshot_t *snapshot,
    const gp100_edit_screen_theme_t *theme)
{
    if (canvas == NULL || snapshot == NULL || theme == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        snapshot->selected_position >= GP100_EDIT_CHAIN_SIZE ||
        (snapshot->native_chain &&
         (snapshot->chain_capacity > GP100_EDIT_CHAIN_SIZE ||
          snapshot->selected_position >= snapshot->chain_capacity)) ||
        snapshot->selected_family >= GP100_EFFECT_FAMILY_COUNT ||
        snapshot->page_count == 0u ||
        snapshot->page_index >= snapshot->page_count ||
        (snapshot->menu_items != NULL &&
         (snapshot->menu_count > kMaxActionMenuItems ||
          snapshot->menu_selected >= snapshot->menu_count)) ||
        theme->regular_font == NULL || theme->large_font == NULL ||
        theme->small_font == NULL) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    return render_header(canvas, snapshot, theme) &&
           render_chain(canvas, snapshot, theme) &&
           render_parameter_panel(canvas, snapshot, theme) &&
           render_hint(canvas, snapshot, theme);
}
