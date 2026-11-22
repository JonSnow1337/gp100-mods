#include "patch_editor.h"

#include "ui/screens/play_screen.h"

#include <math.h>
#include <string.h>

#define EXPRESSION_NONE UINT8_MAX

static const char *const switch_labels[] = {"None", "-", "+", "Both", "EXP"};
static const char *const action_labels[GP100_PATCH_ACTION_COUNT] = {
    [GP100_PATCH_ACTION_INSERT] = "Insert before",
    [GP100_PATCH_ACTION_REPLACE] = "Replace",
    [GP100_PATCH_ACTION_REMOVE] = "Remove",
    [GP100_PATCH_ACTION_ASSIGN_EXPRESSION] = "Assign EXP"
};

static size_t action_count(bool construct)
{
    return construct ? GP100_PATCH_ACTION_COUNT : GP100_PATCH_ACTION_ASSIGN_EXPRESSION;
}

static size_t parameter_page_count(const gp100_patch_block_t *block)
{
    return (gp100_effect_parameter_count(block->family, block->algorithm) + 4u) / 3u;
}

static size_t last_position(const gp100_preset_record_t *preset, bool construct)
{
    const size_t count = gp100_patch_count(preset);
    if (construct && count < GP100_PATCH_MAX_BLOCKS) {
        return count;
    }
    return count == 0u ? 0u : count - 1u;
}

static size_t step(size_t value, size_t count, bool forward)
{
    if (forward) {
        return (value + 1u) % count;
    }
    return value == 0u ? count - 1u : value - 1u;
}

static gp100_edit_event_result_t adjust_block(
    gp100_patch_editor_t *editor, gp100_preset_record_t *preset,
    gp100_patch_block_t *block, size_t knob, bool forward, uint8_t amount)
{
    const size_t item = 3u * editor->page + knob;
    if (item == 0u) {
        if (block->enabled == forward) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        block->enabled = forward;
    } else if (item == 1u) {
        const size_t model = block->algorithm;
        if ((forward && model + 1u >= gp100_effect_algorithm_count(block->family)) ||
            (!forward && model == 0u)) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        editor->result = gp100_patch_replace(
            preset, editor->position, block->family, forward ? model + 1u : model - 1u);
        return editor->result == GP100_PATCH_OK ? GP100_EDIT_EVENT_PRESET_CHANGED
                                               : GP100_EDIT_EVENT_NAVIGATED;
    } else {
        gp100_parameter_rule_t rule;
        const size_t parameter = item - 2u;
        if (!gp100_effect_parameter_rule(block->family, block->algorithm, parameter,
                                         block->parameters, &rule)) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        const float increment = gp100_edit_parameter_delta(&rule, amount);
        float value = block->parameters[rule.storage_slot] +
                      (forward ? increment : -increment);
        if (value < rule.minimum) {
            value = rule.minimum;
        } else if (value > rule.maximum) {
            value = rule.maximum;
        }
        if (value == block->parameters[rule.storage_slot]) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        if (!gp100_effect_apply_parameter_change(
                block->family, block->algorithm, parameter, value,
                block->parameters, NULL)) {
            return GP100_EDIT_EVENT_INVALID;
        }
    }
    return gp100_patch_update(preset, block) ? GP100_EDIT_EVENT_PRESET_CHANGED
                                            : GP100_EDIT_EVENT_INVALID;
}

static gp100_edit_event_result_t construct_adjust(
    gp100_patch_editor_t *editor, gp100_preset_record_t *preset,
    gp100_patch_block_t *block, uint8_t control, bool forward)
{
    const size_t count = gp100_patch_count(preset);
    if (control == GP100_ENCODER_QUICK_3) {
        const size_t position = editor->position;
        if ((forward && position + 1u >= count) || (!forward && position == 0u)) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        const size_t target = forward ? position + 1u : position - 1u;
        if (!gp100_patch_move(preset, position, target)) {
            return GP100_EDIT_EVENT_INVALID;
        }
        editor->position = (uint8_t)target;
    } else if (control == GP100_ENCODER_QUICK_1) {
        gp100_patch_expression_t assignment;
        const bool linked = gp100_patch_expression_get(preset, &assignment, NULL) &&
                            assignment.block_id == block->id;
        const size_t choices = GP100_PATCH_FOOTSWITCH_EXP + (linked ? 1u : 0u);
        block->footswitches = (uint8_t)step(block->footswitches, choices, forward);
        if (!gp100_patch_update(preset, block)) {
            return GP100_EDIT_EVENT_INVALID;
        }
    } else if (control == GP100_ENCODER_QUICK_2) {
        editor->family = (uint8_t)block->family;
        editor->replacing = true;
        editor->picker = GP100_PATCH_PICK_GROUP;
        return GP100_EDIT_EVENT_NAVIGATED;
    } else {
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    return GP100_EDIT_EVENT_PRESET_CHANGED;
}

static gp100_edit_event_result_t confirm_action(
    gp100_patch_editor_t *editor, gp100_preset_record_t *preset,
    const gp100_patch_block_t *block)
{
    if (editor->action == GP100_PATCH_ACTION_ASSIGN_EXPRESSION) {
        gp100_patch_expression_t current;
        editor->expression = (gp100_patch_expression_t){
            .block_id = block->id, .parameter = EXPRESSION_NONE
        };
        if (gp100_patch_expression_get(preset, &current, NULL) && current.block_id == block->id) {
            gp100_parameter_rule_t rule;
            if (!gp100_effect_parameter_rule(block->family, block->algorithm,
                    current.parameter, block->parameters, &rule)) {
                return GP100_EDIT_EVENT_INVALID;
            }
            current.heel = fminf(fmaxf(current.heel, rule.minimum), rule.maximum);
            current.toe = fminf(fmaxf(current.toe, rule.minimum), rule.maximum);
            editor->expression = current;
        }
        editor->picker = GP100_PATCH_PICK_EXPRESSION;
        return GP100_EDIT_EVENT_NAVIGATED;
    }
    if (editor->action == GP100_PATCH_ACTION_REMOVE) {
        if (!gp100_patch_remove(preset, editor->position)) {
            return GP100_EDIT_EVENT_INVALID;
        }
        editor->picker = GP100_PATCH_PICK_NONE;
        editor->page = 0u;
        return GP100_EDIT_EVENT_PRESET_CHANGED;
    }
    if (editor->action == GP100_PATCH_ACTION_INSERT &&
        gp100_patch_count(preset) >= GP100_PATCH_MAX_BLOCKS) {
        editor->result = GP100_PATCH_FULL;
        return GP100_EDIT_EVENT_NAVIGATED;
    }
    editor->family = (uint8_t)block->family;
    editor->replacing = editor->action == GP100_PATCH_ACTION_REPLACE;
    editor->picker = GP100_PATCH_PICK_GROUP;
    return GP100_EDIT_EVENT_NAVIGATED;
}

static gp100_edit_event_result_t edit_expression(
    gp100_patch_editor_t *editor, gp100_preset_record_t *preset,
    const gp100_patch_block_t *block, const gp100_ui_control_event_t *event)
{
    gp100_patch_expression_t *draft = &editor->expression;

    if (event->kind == GP100_UI_CONTROL_BUTTON && event->action == GP100_BUTTON_RELEASE) {
        if (event->control == GP100_BUTTON_EXIT) {
            editor->picker = GP100_PATCH_PICK_NONE;
            return GP100_EDIT_EVENT_NAVIGATED;
        }
        if (event->control == GP100_BUTTON_PARA) {
            gp100_patch_expression_t current;
            const bool assigned = gp100_patch_expression_get(preset, &current, NULL);
            editor->picker = GP100_PATCH_PICK_NONE;
            if (draft->parameter == EXPRESSION_NONE) {
                if (!assigned || current.block_id != block->id) {
                    return GP100_EDIT_EVENT_NAVIGATED;
                }
                return gp100_patch_expression_set(preset, NULL)
                    ? GP100_EDIT_EVENT_PRESET_CHANGED : GP100_EDIT_EVENT_INVALID;
            }
            if (assigned && draft->block_id == current.block_id &&
                draft->parameter == current.parameter && draft->heel == current.heel &&
                draft->toe == current.toe) {
                return GP100_EDIT_EVENT_NAVIGATED;
            }
            return gp100_patch_expression_set(preset, draft)
                ? GP100_EDIT_EVENT_PRESET_CHANGED : GP100_EDIT_EVENT_INVALID;
        }
    }
    if (event->kind != GP100_UI_CONTROL_ENCODER || event->control >= GP100_ENCODER_PARA) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }

    const bool forward = event->action == GP100_BUTTON_PRESS;
    gp100_parameter_rule_t rule;
    if (event->control == GP100_ENCODER_QUICK_1) {
        const size_t count = gp100_effect_parameter_count(block->family, block->algorithm);
        const size_t current = draft->parameter == EXPRESSION_NONE ? 0u : draft->parameter + 1u;
        const size_t choice = step(current, count + 1u, forward);
        draft->parameter = choice == 0u ? EXPRESSION_NONE : (uint8_t)(choice - 1u);
        if (choice != 0u) {
            if (!gp100_effect_parameter_rule(block->family, block->algorithm,
                    draft->parameter, block->parameters, &rule)) {
                return GP100_EDIT_EVENT_INVALID;
            }
            draft->heel = rule.minimum;
            draft->toe = rule.maximum;
        }
    } else if (draft->parameter != EXPRESSION_NONE) {
        if (!gp100_effect_parameter_rule(block->family, block->algorithm,
                draft->parameter, block->parameters, &rule)) {
            return GP100_EDIT_EVENT_INVALID;
        }
        float *value = event->control == GP100_ENCODER_QUICK_2 ? &draft->heel : &draft->toe;
        const float delta = gp100_edit_parameter_delta(&rule, event->amount);
        *value = fminf(fmaxf(*value + (forward ? delta : -delta), rule.minimum), rule.maximum);
    }
    return GP100_EDIT_EVENT_NAVIGATED;
}

gp100_edit_event_result_t gp100_patch_editor_handle(
    gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
    gp100_preset_record_t *preset, const gp100_settings_record_t *settings,
    uint16_t *logical_preset, bool *drum_enabled,
    const gp100_event_t *event, gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    if (!gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_EDIT_EVENT_INVALID;
    }

    const bool construct = mode == GP100_UI_MODE_CONSTRUCT;
    const size_t last = last_position(preset, construct);
    if (editor->position > last) {
        editor->position = (uint8_t)last;
        editor->page = 0u;
    }
    gp100_patch_block_t block;
    const bool selected = gp100_patch_block_at(preset, editor->position, &block);
    if (editor->picker == GP100_PATCH_PICK_EXPRESSION) {
        return selected ? edit_expression(editor, preset, &block, &decoded)
                        : GP100_EDIT_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        const bool forward = decoded.action == GP100_BUTTON_PRESS;
        editor->result = GP100_PATCH_OK;
        if (decoded.control == GP100_ENCODER_PARA) {
            if (editor->picker == GP100_PATCH_PICK_ACTION) {
                editor->action = (gp100_patch_action_t)step(
                    editor->action, action_count(construct), forward);
            } else if (editor->picker == GP100_PATCH_PICK_GROUP) {
                editor->family = (uint8_t)step(editor->family,
                                               GP100_EFFECT_FAMILY_COUNT, forward);
            } else if (editor->picker == GP100_PATCH_PICK_EFFECT) {
                editor->algorithm = (uint16_t)step(
                    editor->algorithm, gp100_effect_algorithm_count(editor->family),
                    forward);
            } else {
                if ((forward && editor->position == last) ||
                    (!forward && editor->position == 0u)) {
                    return GP100_EDIT_EVENT_NAVIGATED;
                }
                editor->position = (uint8_t)(forward ? editor->position + 1u
                                                     : editor->position - 1u);
                editor->page = 0u;
            }
            return GP100_EDIT_EVENT_NAVIGATED;
        }
        if (!selected || editor->picker != GP100_PATCH_PICK_NONE) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        return construct ? construct_adjust(editor, preset, &block,
                                           decoded.control, forward)
                         : adjust_block(editor, preset, &block,
                                        decoded.control, forward, decoded.amount);
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }

    if (decoded.control == GP100_BUTTON_PARA && decoded.action == GP100_BUTTON_HOLD &&
        editor->picker == GP100_PATCH_PICK_NONE) {
        *next_mode = construct ? GP100_UI_MODE_EDIT : GP100_UI_MODE_CONSTRUCT;
        editor->para_held = true;
        return GP100_EDIT_EVENT_MODE_CHANGED;
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_PARA && editor->para_held) {
        editor->para_held = false;
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (editor->picker != GP100_PATCH_PICK_NONE) {
        if (decoded.control == GP100_BUTTON_EXIT) {
            editor->picker = editor->picker == GP100_PATCH_PICK_EFFECT
                ? GP100_PATCH_PICK_GROUP : GP100_PATCH_PICK_NONE;
            editor->result = GP100_PATCH_OK;
        } else if (decoded.control == GP100_BUTTON_PARA) {
            if (editor->picker == GP100_PATCH_PICK_ACTION) {
                return selected ? confirm_action(editor, preset, &block)
                                : GP100_EDIT_EVENT_INVALID;
            } else if (editor->picker == GP100_PATCH_PICK_GROUP) {
                editor->algorithm = editor->replacing && selected &&
                    block.family == editor->family ? block.algorithm : 0u;
                editor->picker = GP100_PATCH_PICK_EFFECT;
            } else {
                const gp100_effect_family_t family = editor->family;
                editor->result = editor->replacing
                    ? gp100_patch_replace(preset, editor->position, family, editor->algorithm)
                    : gp100_patch_insert(preset, editor->position, family, editor->algorithm);
                if (editor->result == GP100_PATCH_OK) {
                    editor->picker = GP100_PATCH_PICK_NONE;
                    editor->page = 0u;
                    return GP100_EDIT_EVENT_PRESET_CHANGED;
                }
            }
        } else {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        return GP100_EDIT_EVENT_NAVIGATED;
    }
    switch (decoded.control) {
    case GP100_BUTTON_EDIT:
        if (!selected && !construct) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        editor->result = GP100_PATCH_OK;
        if (!selected) {
            editor->replacing = false;
            editor->picker = GP100_PATCH_PICK_GROUP;
            return GP100_EDIT_EVENT_NAVIGATED;
        }
        editor->action = GP100_PATCH_ACTION_INSERT;
        editor->picker = GP100_PATCH_PICK_ACTION;
        return GP100_EDIT_EVENT_NAVIGATED;
    case GP100_BUTTON_PARA:
        if (construct) {
            *next_mode = GP100_UI_MODE_EDIT;
            return GP100_EDIT_EVENT_MODE_CHANGED;
        }
        if (!selected) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        editor->page = (uint8_t)step(editor->page, parameter_page_count(&block), true);
        return GP100_EDIT_EVENT_NAVIGATED;
    case GP100_BUTTON_EXIT:
        *next_mode = construct ? GP100_UI_MODE_EDIT : GP100_UI_MODE_PLAY;
        return GP100_EDIT_EVENT_MODE_CHANGED;
    case GP100_BUTTON_SAVE:
        *next_mode = GP100_UI_MODE_SAVE;
        return GP100_EDIT_EVENT_MODE_CHANGED;
    case GP100_BUTTON_GLOBAL:
        *next_mode = GP100_UI_MODE_GLOBAL;
        return GP100_EDIT_EVENT_MODE_CHANGED;
    case GP100_BUTTON_DRUM:
        *drum_enabled = !*drum_enabled;
        return GP100_EDIT_EVENT_DRUM_TOGGLED;
    case GP100_BUTTON_FOOTSWITCH_MINUS:
    case GP100_BUTTON_FOOTSWITCH_PLUS:
        switch (gp100_play_footswitch(settings, preset, logical_preset, decoded.control)) {
        case GP100_PLAY_ACTION_SELECT_PRESET:
            editor->position = editor->page = 0u;
            return GP100_EDIT_EVENT_PRESET_SELECTED;
        case GP100_PLAY_ACTION_PRESET_CHANGED:
            return GP100_EDIT_EVENT_PRESET_CHANGED;
        case GP100_PLAY_ACTION_INVALID:
            return GP100_EDIT_EVENT_INVALID;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return GP100_EDIT_EVENT_UNHANDLED;
}

static const char *limit_message(gp100_patch_result_t result)
{
    switch (result) {
    case GP100_PATCH_FULL: return "All 8 slots used";
    case GP100_PATCH_CPU_LIMIT: return "DSP budget reached";
    case GP100_PATCH_MEMORY_LIMIT: return "Memory budget reached";
    case GP100_PATCH_INVALID: return "Effect unavailable";
    default: return NULL;
    }
}

static void set_slot(gp100_edit_parameter_slot_t *slot,
                     gp100_edit_slot_kind_t kind, const char *label,
                     const char *value)
{
    strncpy(slot->label, label, sizeof(slot->label) - 1u);
    strncpy(slot->value, value, sizeof(slot->value) - 1u);
    slot->kind = kind;
    slot->present = true;
}

static bool fill_page(gp100_edit_screen_snapshot_t *screen,
                      const gp100_patch_block_t *block)
{
    const size_t count = gp100_effect_parameter_count(block->family, block->algorithm);
    for (size_t knob = 0u; knob < GP100_EDIT_PARAMETER_SLOTS; ++knob) {
        const size_t item = screen->page_index * GP100_EDIT_PARAMETER_SLOTS + knob;
        gp100_edit_parameter_slot_t *slot = &screen->slots[knob];
        if (item == 0u) {
            set_slot(slot, GP100_EDIT_SLOT_EFFECT_ENABLED, "ON/OFF",
                     block->enabled ? "ON" : "OFF");
        } else if (item == 1u) {
            set_slot(slot, GP100_EDIT_SLOT_ALGORITHM, "Model", screen->selection_label);
        } else if (item - 2u < count &&
                   !gp100_edit_screen_fill_parameter(
                       slot, block->family, block->algorithm, item - 2u,
                       block->parameters)) {
            return false;
        }
    }
    return true;
}

bool gp100_patch_editor_render(
    const gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
    const gp100_preset_record_t *preset, uint16_t logical_preset,
    bool preset_modified, gp100_display_canvas_t *canvas,
    const gp100_edit_screen_theme_t *theme)
{
    if (editor == NULL || !gp100_patch_is_native(preset)) {
        return false;
    }
    const size_t count = gp100_patch_count(preset);
    const bool construct = mode == GP100_UI_MODE_CONSTRUCT;
    const size_t last = last_position(preset, construct);
    gp100_edit_screen_snapshot_t screen = {
        .logical_preset = logical_preset,
        .selected_position = editor->position < last ? editor->position : last,
        .page_count = 1u,
        .preset_modified = preset_modified,
        .native_chain = true,
        .manage_signal_chain = construct,
        .chain_capacity = GP100_PATCH_MAX_BLOCKS,
        .occupied_count = count,
        .mode_label = construct ? "SIGNAL CHAIN" : NULL
    };

    if (count > GP100_PATCH_MAX_BLOCKS) {
        return false;
    }
    gp100_preset_name(preset, screen.preset_name);
    gp100_patch_expression_t expression;
    const bool has_expression = gp100_patch_expression_get(preset, &expression, NULL);
    for (size_t pos = 0; pos < count; ++pos) {
        gp100_patch_block_t block;
        if (!gp100_patch_block_at(preset, pos, &block)) {
            return false;
        }
        screen.cards[pos] = (gp100_edit_chain_card_t){
            .family = block.family, .algorithm = block.algorithm,
            .present = true, .enabled = block.enabled,
            .expression_assigned = construct && has_expression && expression.block_id == block.id
        };
    }

    char user_name[8];
    gp100_patch_block_t selected;
    const char *warning = limit_message(editor->result);
    if (editor->picker == GP100_PATCH_PICK_ACTION) {
        const gp100_edit_chain_card_t *card = &screen.cards[screen.selected_position];
        screen.selection_label = gp100_edit_screen_model_name(
            card->family, card->algorithm, user_name);
        screen.mode_label = "PEDAL ACTIONS";
        screen.menu_items = action_labels;
        screen.menu_count = action_count(construct);
        screen.menu_selected = editor->action;
        screen.hint = "PARA select   EXIT back";
    } else if (editor->picker == GP100_PATCH_PICK_EXPRESSION &&
               gp100_patch_block_at(preset, screen.selected_position, &selected)) {
        const gp100_patch_expression_t *draft = &editor->expression;
        screen.mode_label = "ASSIGN EXP";
        screen.selection_label = gp100_edit_screen_model_name(
            selected.family, selected.algorithm, user_name);
        const char *name = draft->parameter == EXPRESSION_NONE ? "None" :
            gp100_effect_parameter_name(selected.family, selected.algorithm, draft->parameter);
        if (name == NULL) {
            return false;
        }
        set_slot(&screen.slots[0], GP100_EDIT_SLOT_ALGORITHM, "Knob", name);
        if (draft->parameter != EXPRESSION_NONE) {
            set_slot(&screen.slots[1], GP100_EDIT_SLOT_ALGORITHM, "Heel", "");
            set_slot(&screen.slots[2], GP100_EDIT_SLOT_ALGORITHM, "Toe", "");
            if (gp100_effect_format_parameter_value(selected.family, selected.algorithm,
                    draft->parameter, draft->heel, selected.parameters,
                    screen.slots[1].value, sizeof(screen.slots[1].value)) < 0 ||
                gp100_effect_format_parameter_value(selected.family, selected.algorithm,
                    draft->parameter, draft->toe, selected.parameters,
                    screen.slots[2].value, sizeof(screen.slots[2].value)) < 0) {
                return false;
            }
        }
        screen.hint = "PARA apply   EXIT cancel";
    } else if (editor->picker != GP100_PATCH_PICK_NONE) {
        const bool choose_group = editor->picker == GP100_PATCH_PICK_GROUP;
        const gp100_effect_family_t family = editor->family;
        const uint16_t algorithm = choose_group ? 0u : editor->algorithm;
        screen.cards[screen.selected_position] = (gp100_edit_chain_card_t){
            .family = family, .algorithm = algorithm, .present = true, .enabled = true
        };
        screen.selection_label = gp100_edit_screen_model_name(family, algorithm, user_name);
        screen.mode_label = editor->replacing ? "CHANGE EFFECT" : "ADD EFFECT";
        screen.panel_title = choose_group ? "Effect group" : gp100_effect_family_name(family);
        screen.panel_value = choose_group ? gp100_effect_family_name(family)
                                          : screen.selection_label;
        screen.hint = choose_group ? "Turn PARA, press to select"
                                  : (editor->replacing ? "Press PARA to change"
                                                       : "Press PARA to add");
    } else if (!gp100_patch_block_at(preset, screen.selected_position, &selected)) {
        screen.panel_title = construct ? "Empty slot" : "No pedals in this preset";
        screen.panel_value = construct ? "Press EDIT to add" : "Hold PARA for Signal Chain";
    } else {
        screen.selection_label = gp100_edit_screen_model_name(
            selected.family, selected.algorithm, user_name);
        if (construct) {
            set_slot(&screen.slots[0], GP100_EDIT_SLOT_STOMP_SWITCH,
                     "Footsw.", switch_labels[selected.footswitches]);
            set_slot(&screen.slots[1], GP100_EDIT_SLOT_ALGORITHM, "Effect", "Change");
            set_slot(&screen.slots[2], GP100_EDIT_SLOT_ALGORITHM, "Order", "Move");
            screen.hint = "PARA back   EDIT actions";
        } else {
            screen.page_count = parameter_page_count(&selected);
            screen.page_index = editor->page < screen.page_count ? editor->page : 0u;
            if (!fill_page(&screen, &selected)) {
                return false;
            }
            screen.hint = "EDIT actions   Hold PARA";
        }
    }
    if (warning != NULL) {
        screen.hint = warning;
        screen.hint_is_warning = true;
    }
    return gp100_edit_screen_render(canvas, &screen, theme);
}
