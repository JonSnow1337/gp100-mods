#include "patch_editor.h"

#include "ui/render/mode_chip.h"
#include "ui/screens/play_screen.h"
#include "ui/ui_palette.h"

#include <math.h>
#include <string.h>

#define EXPRESSION_NONE UINT8_MAX

static const char *const switch_labels[] = {"None", "-", "+", "Both", "EXP"};
static const char *const action_labels[GP100_PATCH_ACTION_COUNT] = {
    [GP100_PATCH_ACTION_INSERT] = "Insert before",
    [GP100_PATCH_ACTION_REMOVE] = "Remove",
    [GP100_PATCH_ACTION_ASSIGN_EXPRESSION] = "Assign EXP",
    [GP100_PATCH_ACTION_CLEAR] = "Clear preset"
};

static size_t parameter_page_count(const gp100_patch_block_t *block)
{
    return (gp100_effect_parameter_count(block->family, block->algorithm) +
            GP100_EDIT_PARAMETER_SLOTS) / GP100_EDIT_PARAMETER_SLOTS;
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

static bool quick_remove_enabled(const gp100_settings_record_t *settings)
{
    int16_t value;
    return settings != NULL &&
           gp100_settings_get_i16(settings, GP100_SETTINGS_QUICK_REMOVE, &value) && value == 1;
}

/* ---- effect picker: one list across all groups ---------------------------------------------- */

static bool move_picker_group(gp100_patch_editor_t *editor, bool forward)
{
    size_t group = editor->group;
    for (size_t i = 0; i < GP100_EFFECT_GROUP_COUNT; ++i) {
        group = step(group, GP100_EFFECT_GROUP_COUNT, forward);
        if (gp100_effect_picker_count((gp100_effect_group_t)group) != 0u) {
            editor->group = (gp100_effect_group_t)group;
            return true;
        }
    }
    return false;
}

/* One detent of PARA or Quick 2: next/previous effect, wrapping inside the current group
 * (Quick 1 is what changes the group). */
static bool step_effect(gp100_patch_editor_t *editor, bool forward)
{
    const size_t count = gp100_effect_picker_count(editor->group);
    if (count < 2u) {
        return false;
    }
    editor->choice = (uint16_t)step(editor->choice, count, forward);
    return true;
}

/* The effect currently highlighted, taking an equivalent model of an older preset into account. */
static bool picked_effect(const gp100_patch_editor_t *editor, gp100_picker_effect_t *effect)
{
    gp100_patch_block_t original;
    gp100_effect_group_t group;
    uint16_t choice;

    if (!gp100_effect_picker_at(editor->group, editor->choice, effect)) {
        return false;
    }
    if (editor->replacing &&
        gp100_patch_block_at(&editor->backup, editor->position, &original) &&
        gp100_effect_picker_find((gp100_picker_effect_t){original.family, original.algorithm},
                                 &group, &choice) &&
        group == editor->group && choice == editor->choice) {
        *effect = (gp100_picker_effect_t){original.family, original.algorithm};
    }
    return true;
}

static bool preview_wanted(const gp100_patch_editor_t *editor)
{
    return editor->live_preview && !editor->preview_muted;
}

static void clear_preview(gp100_patch_editor_t *editor)
{
    editor->preview_pending = false;
    editor->preview_applied = false;
}

/* Build the preset for the highlighted effect from the saved copy. */
static gp100_patch_result_t apply_choice(gp100_patch_editor_t *editor, gp100_preset_record_t *preset)
{
    gp100_picker_effect_t effect;
    gp100_patch_block_t original;
    gp100_patch_result_t result;

    *preset = editor->backup;
    if (!picked_effect(editor, &effect)) {
        return GP100_PATCH_INVALID;
    }
    if (editor->replacing) {
        if (!gp100_patch_block_at(preset, editor->position, &original)) {
            return GP100_PATCH_INVALID;
        }
        if (original.family == effect.family && original.algorithm == effect.algorithm) {
            return GP100_PATCH_OK;
        }
        result = gp100_patch_replace(preset, editor->position, effect.family, effect.algorithm);
    } else {
        result = gp100_patch_insert(preset, editor->position, effect.family, effect.algorithm);
    }
    if (result != GP100_PATCH_OK) {
        *preset = editor->backup;
    }
    return result;
}

/* Put the highlighted effect into the live preset (the debounce timer expired). */
static bool preview_sync(gp100_patch_editor_t *editor, gp100_preset_record_t *preset)
{
    const gp100_preset_record_t before = *preset;

    editor->result = apply_choice(editor, preset);
    editor->preview_pending = false;
    editor->preview_applied = memcmp(preset, &editor->backup, sizeof(*preset)) != 0;
    return memcmp(&before, preset, sizeof(before)) != 0;
}

static void open_effect_picker(gp100_patch_editor_t *editor, const gp100_preset_record_t *preset,
                               const gp100_patch_block_t *block, bool replacing)
{
    editor->backup = *preset;
    editor->undo_available = false;
    editor->preview_muted = false;
    clear_preview(editor);
    editor->choice = 0u;
    if (block != NULL) {
        (void)gp100_effect_picker_find((gp100_picker_effect_t){block->family, block->algorithm},
                                      &editor->group, &editor->choice);
    }
    if (gp100_effect_picker_count(editor->group) == 0u && !move_picker_group(editor, true)) {
        editor->picker = GP100_PATCH_PICK_NONE;
        editor->result = GP100_PATCH_INVALID;
        return;
    }
    editor->replacing = replacing;
    editor->picker = GP100_PATCH_PICK_EFFECT;
    /* Adding: hear the first effect right away; changing: the current effect is already playing. */
    editor->preview_pending = preview_wanted(editor) && !replacing;
}

bool gp100_patch_editor_cancel(gp100_patch_editor_t *editor, gp100_preset_record_t *preset)
{
    bool changed = false;

    if (editor == NULL || preset == NULL) {
        return false;
    }
    if (editor->picker == GP100_PATCH_PICK_EFFECT || editor->undo_available) {
        changed = memcmp(preset, &editor->backup, sizeof(*preset)) != 0;
        *preset = editor->backup;
        editor->restore_clean = true;
    }
    editor->picker = GP100_PATCH_PICK_NONE;
    editor->undo_available = false;
    editor->result = GP100_PATCH_OK;
    clear_preview(editor);
    return changed;
}

static gp100_edit_event_result_t confirm_picker(gp100_patch_editor_t *editor,
                                                gp100_preset_record_t *preset)
{
    const gp100_preset_record_t before = *preset;
    const gp100_patch_result_t result = apply_choice(editor, preset);
    const bool changed = memcmp(&before, preset, sizeof(before)) != 0;

    editor->result = result;
    editor->preview_pending = false;
    editor->preview_applied = false;
    if (result == GP100_PATCH_OK) {
        editor->picker = GP100_PATCH_PICK_NONE;
        editor->page = 0u;
        if (memcmp(preset, &editor->backup, sizeof(*preset)) == 0) {
            editor->restore_clean = true;
        }
    }
    return changed ? GP100_EDIT_EVENT_PRESET_CHANGED : GP100_EDIT_EVENT_NAVIGATED;
}

/* Remove the selected effect; EXIT brings it back until something else is pressed. */
static gp100_edit_event_result_t remove_selected(gp100_patch_editor_t *editor,
                                                 gp100_preset_record_t *preset)
{
    const gp100_preset_record_t before = *preset;

    if (!gp100_patch_remove(preset, editor->position)) {
        return GP100_EDIT_EVENT_INVALID;
    }
    editor->backup = before;
    editor->undo_available = true;
    editor->undo_cleared = false;
    editor->picker = GP100_PATCH_PICK_NONE;
    editor->page = 0u;
    clear_preview(editor);
    return GP100_EDIT_EVENT_PRESET_CHANGED;
}

/* "Clear preset": remove every effect (and with them the footswitch and expression assignments).
 * Name, tempo and preset volume stay. EXIT right afterwards brings everything back. */
static gp100_edit_event_result_t clear_preset(gp100_patch_editor_t *editor,
                                              gp100_preset_record_t *preset)
{
    const gp100_preset_record_t before = *preset;

    while (gp100_patch_count(preset) > 0u) {
        if (!gp100_patch_remove(preset, 0u)) {
            *preset = before;
            editor->picker = GP100_PATCH_PICK_NONE;
            return GP100_EDIT_EVENT_INVALID;
        }
    }
    editor->backup = before;
    editor->undo_available = true;
    editor->undo_cleared = true;
    editor->picker = GP100_PATCH_PICK_NONE;
    editor->position = 0u;
    editor->page = 0u;
    clear_preview(editor);
    return GP100_EDIT_EVENT_PRESET_CHANGED;
}

static gp100_edit_event_result_t adjust_block(
    gp100_patch_editor_t *editor, gp100_preset_record_t *preset,
    gp100_patch_block_t *block, size_t knob, bool forward, uint8_t amount)
{
    const size_t item = GP100_EDIT_PARAMETER_SLOTS * editor->page + knob;
    if (item == 0u) {
        if (block->enabled == forward) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        block->enabled = forward;
    } else {
        gp100_parameter_rule_t rule;
        const size_t parameter = item - 1u;
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
        if (block->algorithm == GP100_PATCH_UNKNOWN_ALGORITHM) {
            editor->result = GP100_PATCH_INVALID;
            return GP100_EDIT_EVENT_NAVIGATED;
        }
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
        return remove_selected(editor, preset);
    }
    if (editor->action == GP100_PATCH_ACTION_CLEAR) {
        editor->picker = GP100_PATCH_PICK_CLEAR_CONFIRM;
        return GP100_EDIT_EVENT_NAVIGATED;
    }
    if (gp100_patch_count(preset) >= GP100_PATCH_MAX_BLOCKS) {
        editor->result = GP100_PATCH_FULL;
        return GP100_EDIT_EVENT_NAVIGATED;
    }
    open_effect_picker(editor, preset, NULL, false);
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
    if (decoded.kind == GP100_UI_CONTROL_TICK) {
        /* The encoder stopped: load the highlighted effect now. */
        if (editor->picker == GP100_PATCH_PICK_EFFECT && editor->preview_pending) {
            return preview_sync(editor, preset) ? GP100_EDIT_EVENT_PRESET_CHANGED
                                                : GP100_EDIT_EVENT_NAVIGATED;
        }
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (gp100_ui_control_is_user_input(&decoded)) {
        editor->hint_alt = false;   /* a touch on a control brings the first hint back */
    }
    /* A pending undo only lives until the next real action. */
    if (editor->undo_available &&
        !(decoded.kind == GP100_UI_CONTROL_BUTTON &&
          (decoded.action == GP100_BUTTON_PRESS || decoded.control == GP100_BUTTON_EXIT ||
           (decoded.control == GP100_BUTTON_EDIT && editor->edit_held)))) {
        editor->undo_available = false;
    }
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
            if (editor->picker == GP100_PATCH_PICK_CLEAR_CONFIRM) {
                return GP100_EDIT_EVENT_NAVIGATED;      /* a question: only PARA (yes) or EXIT (no) */
            } else if (editor->picker == GP100_PATCH_PICK_ACTION) {
                editor->action = (gp100_patch_action_t)step(
                    editor->action, GP100_PATCH_ACTION_COUNT, forward);
            } else if (editor->picker == GP100_PATCH_PICK_EFFECT) {
                if (step_effect(editor, forward) && preview_wanted(editor)) {
                    editor->preview_pending = true;
                }
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
        if (editor->picker == GP100_PATCH_PICK_EFFECT) {
            /* The three knobs of the effect list: Quick 1 group, Quick 2 effect, Quick 3 preview. */
            if (decoded.control == GP100_ENCODER_QUICK_1) {
                if (move_picker_group(editor, forward)) {
                    editor->choice = 0u;
                    editor->preview_pending = preview_wanted(editor);
                }
            } else if (decoded.control == GP100_ENCODER_QUICK_2) {
                if (step_effect(editor, forward)) {
                    editor->preview_pending = preview_wanted(editor);
                }
            } else if (decoded.control == GP100_ENCODER_QUICK_3 && editor->live_preview &&
                       editor->preview_muted == forward) {
                editor->preview_muted = !forward;       /* turn right = on, turn left = off */
                if (editor->preview_muted) {
                    /* Back to the sound from before the picker until the preview is switched on again. */
                    const bool had_preview = editor->preview_applied;
                    *preset = editor->backup;
                    editor->restore_clean = true;
                    clear_preview(editor);
                    return had_preview ? GP100_EDIT_EVENT_PRESET_CHANGED : GP100_EDIT_EVENT_NAVIGATED;
                }
                editor->preview_pending = true;
            }
            return GP100_EDIT_EVENT_NAVIGATED;
        }
        if (!selected || editor->picker != GP100_PATCH_PICK_NONE) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        if (construct) {
            return construct_adjust(editor, preset, &block, decoded.control, forward);
        }
        return gp100_effect_available(block.family, block.algorithm)
            ? adjust_block(editor, preset, &block, decoded.control, forward, decoded.amount)
            : GP100_EDIT_EVENT_UNHANDLED;
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
    if (decoded.control == GP100_BUTTON_EDIT && decoded.action == GP100_BUTTON_HOLD &&
        construct && selected && editor->picker == GP100_PATCH_PICK_NONE &&
        quick_remove_enabled(settings)) {
        editor->edit_held = true;
        editor->result = GP100_PATCH_OK;
        return remove_selected(editor, preset);
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_PARA && editor->para_held) {
        editor->para_held = false;
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_EDIT && editor->edit_held) {
        editor->edit_held = false;
        return GP100_EDIT_EVENT_UNHANDLED;
    }
    if (editor->picker != GP100_PATCH_PICK_NONE) {
        if (decoded.control == GP100_BUTTON_EXIT) {
            const bool changed = gp100_patch_editor_cancel(editor, preset);
            return changed ? GP100_EDIT_EVENT_PRESET_CHANGED : GP100_EDIT_EVENT_NAVIGATED;
        }
        if (decoded.control != GP100_BUTTON_PARA) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        if (editor->picker == GP100_PATCH_PICK_CLEAR_CONFIRM) {
            return clear_preset(editor, preset);
        }
        if (editor->picker == GP100_PATCH_PICK_ACTION) {
            return selected ? confirm_action(editor, preset, &block)
                            : GP100_EDIT_EVENT_INVALID;
        }
        return confirm_picker(editor, preset);
    }
    switch (decoded.control) {
    case GP100_BUTTON_EDIT:
        editor->result = GP100_PATCH_OK;
        if (!selected) {
            open_effect_picker(editor, preset, NULL, false);
        } else if (construct) {
            editor->action = GP100_PATCH_ACTION_INSERT;
            editor->picker = GP100_PATCH_PICK_ACTION;
        } else {
            open_effect_picker(editor, preset, &block, true);
        }
        return GP100_EDIT_EVENT_NAVIGATED;
    case GP100_BUTTON_PARA:
        if (construct) {
            /* PARA adds an effect on the empty [+] slot; on an effect it does nothing
             * (changing an effect is done in Edit mode). */
            if (selected) {
                return GP100_EDIT_EVENT_UNHANDLED;
            }
            editor->result = GP100_PATCH_OK;
            open_effect_picker(editor, preset, NULL, false);
            return GP100_EDIT_EVENT_NAVIGATED;
        }
        if (!selected || block.algorithm == GP100_PATCH_UNKNOWN_ALGORITHM) {
            return GP100_EDIT_EVENT_UNHANDLED;
        }
        editor->page = (uint8_t)step(editor->page, parameter_page_count(&block), true);
        return GP100_EDIT_EVENT_NAVIGATED;
    case GP100_BUTTON_EXIT:
        if (editor->undo_available) {
            const gp100_preset_record_t restored = editor->backup;
            editor->undo_available = false;
            editor->restore_clean = true;
            *preset = restored;
            return GP100_EDIT_EVENT_PRESET_CHANGED;
        }
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

const char *gp100_patch_editor_mode_label(const gp100_patch_editor_t *editor, gp100_ui_mode_t mode)
{
    switch (editor->picker) {
    case GP100_PATCH_PICK_EFFECT: return editor->replacing ? "CHANGE EFFECT" : "ADD EFFECT";
    case GP100_PATCH_PICK_ACTION: return "CHAIN MENU";
    case GP100_PATCH_PICK_CLEAR_CONFIRM: return "CLEAR PRESET";
    case GP100_PATCH_PICK_EXPRESSION: return "ASSIGN EXP";
    default: return mode == GP100_UI_MODE_CONSTRUCT ? "SIGNAL CHAIN" : "EDIT";
    }
}

uint16_t gp100_patch_editor_accent(const gp100_patch_editor_t *editor, gp100_ui_mode_t mode)
{
    if (editor->picker == GP100_PATCH_PICK_EFFECT) {
        return GP100_UI_MODE_PICKER_COLOR;
    }
    if (editor->picker != GP100_PATCH_PICK_NONE || mode == GP100_UI_MODE_CONSTRUCT) {
        return GP100_UI_MODE_CHAIN_COLOR;
    }
    return GP100_UI_MODE_EDIT_COLOR;
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

/* Group name that fits a knob column (the full name stays in the guide). */
static const char *short_group_name(gp100_effect_group_t group)
{
    return group == GP100_EFFECT_GROUP_MODULATION ? "Modul." : gp100_effect_picker_group_name(group);
}

static size_t append_number(char *out, size_t capacity, size_t length, size_t value)
{
    if (value >= 10u) {
        length = append_number(out, capacity, length, value / 10u);
    }
    if (length + 1u < capacity) {
        out[length++] = (char)('0' + value % 10u);
    }
    out[length] = '\0';
    return length;
}

/* "3 / 9" (no printf in the firmware). */
static void format_position(char *out, size_t capacity, size_t index, size_t count)
{
    size_t length = append_number(out, capacity, 0u, index);
    for (const char *c = " / "; *c != '\0' && length + 1u < capacity; ++c) {
        out[length++] = *c;
    }
    out[length] = '\0';
    (void)append_number(out, capacity, length, count);
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
        } else if (item - 1u < count &&
                   !gp100_edit_screen_fill_parameter(
                       slot, block->family, block->algorithm, item - 1u,
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
    const gp100_patch_picker_t picker = editor->picker;
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
        .mode_label = gp100_patch_editor_mode_label(editor, mode),
        .accent_rgb565 = gp100_patch_editor_accent(editor, mode)
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
    char position[16];
    char clear_text[32];
    gp100_patch_block_t selected;
    const char *warning = limit_message(editor->result);
    if (picker == GP100_PATCH_PICK_ACTION) {
        const gp100_edit_chain_card_t *card = &screen.cards[screen.selected_position];
        screen.selection_label = gp100_edit_screen_model_name(
            card->family, card->algorithm, user_name);
        screen.menu_items = action_labels;
        screen.menu_count = GP100_PATCH_ACTION_COUNT;
        screen.menu_selected = editor->action;
        screen.hint = "[PARA] select  [EXIT] back";
    } else if (picker == GP100_PATCH_PICK_CLEAR_CONFIRM) {
        size_t length = 0u;
        memcpy(clear_text, "Removes all ", 13u);
        length = strlen(clear_text);
        length = append_number(clear_text, sizeof(clear_text), length, count);
        if (length + 9u < sizeof(clear_text)) {
            memcpy(clear_text + length, " effects", 9u);
        }
        screen.panel_title = "Clear preset?";
        screen.panel_value = clear_text;
        screen.hint = "[PARA] clear  [EXIT] cancel";
    } else if (picker == GP100_PATCH_PICK_EXPRESSION &&
               gp100_patch_block_at(preset, screen.selected_position, &selected)) {
        const gp100_patch_expression_t *draft = &editor->expression;
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
        screen.hint = "[PARA] apply  [EXIT] cancel";
    } else if (picker != GP100_PATCH_PICK_NONE) {
        const char *group_name = gp100_effect_picker_group_name(editor->group);
        const size_t group_count = gp100_effect_picker_count(editor->group);
        gp100_picker_effect_t effect;
        if (group_name == NULL || !gp100_effect_picker_at(editor->group, editor->choice, &effect)) {
            return false;
        }
        screen.cards[screen.selected_position] = (gp100_edit_chain_card_t){
            .family = effect.family, .algorithm = effect.algorithm, .present = true, .enabled = true
        };
        screen.selection_label = gp100_edit_screen_model_name(effect.family, effect.algorithm, user_name);
        /* The knob columns of the list: left = Quick 1 (group), middle = Quick 2 (effect), right = Quick 3 (preview). */
        set_slot(&screen.slots[0], GP100_EDIT_SLOT_ALGORITHM, "Group", short_group_name(editor->group));
        format_position(position, sizeof(position), (size_t)editor->choice + 1u, group_count);
        set_slot(&screen.slots[1], GP100_EDIT_SLOT_ALGORITHM, "Effect", position);
        set_slot(&screen.slots[2], GP100_EDIT_SLOT_ALGORITHM, "Preview",
                 preview_wanted(editor) ? "ON" : "OFF");
        screen.hint = "[PARA] keep  [EXIT] cancel";
    } else if (!gp100_patch_block_at(preset, screen.selected_position, &selected)) {
        screen.panel_title = construct ? "Empty slot" : "No effects in this preset";
        /* Effects are added in Signal Chain (EDIT in Edit only replaces an effect), so Edit points there. */
        screen.panel_value = construct ? "Press PARA to add" : "Hold PARA to add effects";
        screen.hint = construct ? "[PARA] add  [EXIT] back" : "[HOLD PARA] signal chain";
    } else {
        screen.selection_label = gp100_edit_screen_model_name(
            selected.family, selected.algorithm, user_name);
        const bool available = gp100_effect_available(selected.family, selected.algorithm);
        if (construct) {
            /* Quick 1 footswitch, Quick 2 unused, Quick 3 order. */
            set_slot(&screen.slots[0], GP100_EDIT_SLOT_STOMP_SWITCH,
                     "Footsw.", switch_labels[selected.footswitches]);
            set_slot(&screen.slots[2], GP100_EDIT_SLOT_ALGORITHM, "Order", "Move");
            screen.hint = editor->hint_alt ? "[HOLD PARA] edit  [EXIT] back"
                                           : "[PARA] select  [EDIT] menu";
        } else if (!available) {
            screen.panel_title = "Unavailable";
            screen.panel_value = "Press EDIT to replace";
        } else {
            screen.page_count = parameter_page_count(&selected);
            screen.page_index = editor->page < screen.page_count ? editor->page : 0u;
            if (!fill_page(&screen, &selected)) {
                return false;
            }
            /* Two hints take turns every few seconds (see gp100_patch_editor_hint_swap). */
            if (editor->hint_alt) {
                screen.hint = "[HOLD PARA] signal chain";
            } else {
                screen.hint = screen.page_count > 1u ? "[PARA] page  [EDIT] effect"
                                                     : "[EDIT] effect  [EXIT] play";
            }
        }
    }
    if (construct && editor->undo_available && picker == GP100_PATCH_PICK_NONE) {
        screen.hint = editor->undo_cleared ? "Cleared - [EXIT] undo" : "Removed - [EXIT] undo";
    }
    if (warning != NULL) {
        screen.hint = warning;
        screen.hint_is_warning = true;
    }
    return gp100_edit_screen_render(canvas, &screen, theme);
}

/* The Edit and Signal Chain screens have two hints that take turns every few seconds. The
 * application calls this when the controls have been idle; it returns true when the screen
 * changed (there was a second hint to show). */
bool gp100_patch_editor_hint_swap(gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
                                  const gp100_preset_record_t *preset)
{
    if (editor == NULL || preset == NULL || !gp100_patch_is_native(preset) ||
        (mode != GP100_UI_MODE_EDIT && mode != GP100_UI_MODE_CONSTRUCT) ||
        editor->picker != GP100_PATCH_PICK_NONE || editor->undo_available ||
        limit_message(editor->result) != NULL) {
        return false;
    }
    const bool construct = mode == GP100_UI_MODE_CONSTRUCT;
    const size_t position = editor->position < last_position(preset, construct)
                                ? editor->position : last_position(preset, construct);
    gp100_patch_block_t block;
    if (!gp100_patch_block_at(preset, position, &block) ||
        (!construct && !gp100_effect_available(block.family, block.algorithm))) {
        return false;   /* empty slot or unavailable effect: one hint only */
    }
    editor->hint_alt = !editor->hint_alt;
    return true;
}
