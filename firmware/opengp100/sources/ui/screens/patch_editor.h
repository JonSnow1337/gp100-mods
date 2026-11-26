#ifndef GP100_PATCH_EDITOR_H
#define GP100_PATCH_EDITOR_H

#include "storage/patch_chain.h"
#include "ui/screens/edit_screen.h"
#include "ui/screens/effect_picker.h"

typedef enum gp100_patch_picker {
    GP100_PATCH_PICK_NONE,
    GP100_PATCH_PICK_EFFECT,      /* one list of all effects, group shown as a heading */
    GP100_PATCH_PICK_ACTION,
    GP100_PATCH_PICK_EXPRESSION,
    GP100_PATCH_PICK_CLEAR_CONFIRM  /* "Clear preset?" question after the chain menu entry */
} gp100_patch_picker_t;

typedef enum gp100_patch_action {
    GP100_PATCH_ACTION_INSERT,
    GP100_PATCH_ACTION_REMOVE,
    GP100_PATCH_ACTION_ASSIGN_EXPRESSION,
    GP100_PATCH_ACTION_CLEAR,
    GP100_PATCH_ACTION_COUNT
} gp100_patch_action_t;

typedef struct gp100_patch_editor {
    uint8_t position;
    uint8_t page;
    gp100_effect_group_t group;
    uint16_t choice;
    gp100_patch_picker_t picker;
    gp100_patch_result_t result;
    bool para_held;
    bool edit_held;
    bool replacing;
    gp100_patch_action_t action;
    gp100_patch_expression_t expression;

    /* Live preview. The picker works on the real preset (so the effect is audible) and keeps the
     * preset from before it opened in `backup`; EXIT puts it back, PARA keeps the chosen effect. */
    bool live_preview;            /* set by the application from the feature flag */
    bool preview_pending;         /* the highlighted effect is not applied yet (debounce) */
    bool preview_applied;         /* the preset currently differs from `backup` because of the preview */
    bool preview_muted;           /* Quick 3 in the effect list switched the preview off for this picker */
    bool undo_available;          /* `backup` holds the preset from before a removal; EXIT restores it */
    bool undo_cleared;            /* the pending undo is for "Clear preset" (wording of the hint) */
    bool restore_clean;           /* runtime: the preset is back to its state before the picker/removal */
    bool hint_alt;                /* the second of two hints that take turns is showing */
    bool modified_snapshot;       /* runtime: preset_modified before the picker/removal started */
    gp100_preset_record_t backup;
} gp100_patch_editor_t;

/* Leave an open picker or pending undo without applying anything (preview is reverted).
 * Returns true if the preset was changed by the revert. */
bool gp100_patch_editor_cancel(gp100_patch_editor_t *editor, gp100_preset_record_t *preset);

/* Swap between the two hints of the Edit / Signal Chain screens; true if the screen changed. */
bool gp100_patch_editor_hint_swap(gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
                                  const gp100_preset_record_t *preset);

/* Name of the editor state shown on the mode chip, and its accent color. */
const char *gp100_patch_editor_mode_label(const gp100_patch_editor_t *editor, gp100_ui_mode_t mode);
uint16_t gp100_patch_editor_accent(const gp100_patch_editor_t *editor, gp100_ui_mode_t mode);

gp100_edit_event_result_t gp100_patch_editor_handle(
    gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
    gp100_preset_record_t *preset, const gp100_settings_record_t *settings,
    uint16_t *logical_preset, bool *drum_enabled,
    const gp100_event_t *event, gp100_ui_mode_t *next_mode);

bool gp100_patch_editor_render(
    const gp100_patch_editor_t *editor, gp100_ui_mode_t mode,
    const gp100_preset_record_t *preset, uint16_t logical_preset,
    bool preset_modified, gp100_display_canvas_t *canvas,
    const gp100_edit_screen_theme_t *theme);

#endif
