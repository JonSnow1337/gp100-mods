#ifndef GP100_PATCH_EDITOR_H
#define GP100_PATCH_EDITOR_H

#include "storage/patch_chain.h"
#include "ui/screens/edit_screen.h"

typedef enum gp100_patch_picker {
    GP100_PATCH_PICK_NONE,
    GP100_PATCH_PICK_GROUP,
    GP100_PATCH_PICK_EFFECT,
    GP100_PATCH_PICK_ACTION,
    GP100_PATCH_PICK_EXPRESSION
} gp100_patch_picker_t;

typedef enum gp100_patch_action {
    GP100_PATCH_ACTION_INSERT,
    GP100_PATCH_ACTION_REPLACE,
    GP100_PATCH_ACTION_REMOVE,
    GP100_PATCH_ACTION_ASSIGN_EXPRESSION,
    GP100_PATCH_ACTION_COUNT
} gp100_patch_action_t;

typedef struct gp100_patch_editor {
    uint8_t position;
    uint8_t page;
    uint8_t family;
    uint16_t algorithm;
    gp100_patch_picker_t picker;
    gp100_patch_result_t result;
    bool para_held;
    bool replacing;
    gp100_patch_action_t action;
    gp100_patch_expression_t expression;
} gp100_patch_editor_t;

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
