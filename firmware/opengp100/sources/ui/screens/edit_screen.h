#ifndef GP100_EDIT_SCREEN_H
#define GP100_EDIT_SCREEN_H

#include "ui/render/display_text.h"
#include "audio/effects/effect_catalog.h"
#include "storage/preset_record.h"
#include "storage/settings_record.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"
#include "protocol/midi/sysex/vendor_sysex_system.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EDIT_CHAIN_SIZE 9u
#define GP100_EDIT_VISIBLE_EFFECTS 4u
#define GP100_EDIT_PARAMETER_SLOTS 3u

typedef enum gp100_edit_slot_kind {
    GP100_EDIT_SLOT_EMPTY = 0,
    GP100_EDIT_SLOT_EFFECT_ENABLED,
    GP100_EDIT_SLOT_ALGORITHM,
    GP100_EDIT_SLOT_STOMP_SWITCH,
    GP100_EDIT_SLOT_PARAMETER
} gp100_edit_slot_kind_t;

typedef struct gp100_edit_parameter_slot {
    gp100_edit_slot_kind_t kind;
    char label[16];
    char value[24];
    float raw_value;
    float minimum;
    float maximum;
    size_t parameter_index;
    bool present;
} gp100_edit_parameter_slot_t;

typedef struct gp100_edit_chain_card {
    gp100_effect_family_t family;
    uint16_t algorithm;
    bool present;
    bool enabled;
    bool expression_assigned;
} gp100_edit_chain_card_t;

typedef struct gp100_edit_screen_snapshot {
    uint16_t logical_preset;
    char preset_name[GP100_PRESET_NAME_SIZE + 1u];
    gp100_footswitch_mode_t footswitch_mode;
    gp100_effect_family_t chain[GP100_EDIT_CHAIN_SIZE];
    bool effect_enabled[GP100_EFFECT_FAMILY_COUNT];
    size_t selected_position;
    gp100_effect_family_t selected_family;
    uint32_t algorithm_id;
    size_t algorithm_index;
    const char *algorithm_name;
    size_t parameter_count;
    size_t page_index;
    size_t page_count;
    int8_t stomp_switch;
    gp100_edit_parameter_slot_t slots[GP100_EDIT_PARAMETER_SLOTS];
    bool manage_signal_chain;
    bool preset_modified;
    bool native_chain;
    size_t chain_capacity;
    size_t occupied_count;
    gp100_edit_chain_card_t cards[GP100_EDIT_CHAIN_SIZE];
    const char *mode_label;
    const char *selection_label;
    const char *panel_title;
    const char *panel_value;
    const char *const *menu_items;
    size_t menu_count;
    size_t menu_selected;
    const char *hint;
    bool hint_is_warning;
} gp100_edit_screen_snapshot_t;

typedef struct gp100_edit_controller {
    size_t selected_position;
    size_t page_index;
    size_t parameter_position;
    size_t parameter_page;
    bool manage_signal_chain;
    bool para_hold_release_pending;
} gp100_edit_controller_t;

typedef enum gp100_edit_result {
    GP100_EDIT_RESULT_INVALID = -1,
    GP100_EDIT_RESULT_UNCHANGED = 0,
    GP100_EDIT_RESULT_NAVIGATED,
    GP100_EDIT_RESULT_PRESET_CHANGED
} gp100_edit_result_t;

typedef enum gp100_edit_event_result {
    GP100_EDIT_EVENT_INVALID = -1,
    GP100_EDIT_EVENT_UNHANDLED = 0,
    GP100_EDIT_EVENT_NAVIGATED,
    GP100_EDIT_EVENT_PRESET_CHANGED,
    GP100_EDIT_EVENT_PRESET_SELECTED,
    GP100_EDIT_EVENT_DRUM_TOGGLED,
    GP100_EDIT_EVENT_MODE_CHANGED
} gp100_edit_event_result_t;

void gp100_edit_controller_init(
    gp100_edit_controller_t *controller);

/* PARA rotation selects one of nine effects; selection saturates at the ends. */
gp100_edit_result_t gp100_edit_controller_move(
    gp100_edit_controller_t *controller,
    bool increase);

/* Short PARA press advances and wraps the selected effect's parameter pages. */
gp100_edit_result_t gp100_edit_controller_next_page(
    gp100_edit_controller_t *controller,
    const gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode);

/* Long PARA enters chain management; a later press returns to normal editing. */
gp100_edit_result_t gp100_edit_controller_set_chain_management(
    gp100_edit_controller_t *controller,
    bool enabled);

float gp100_edit_parameter_delta(const gp100_parameter_rule_t *rule,
                                 uint8_t acceleration);

/* Apply one Quick-knob edit (control 0..2) to the current slot. */
gp100_edit_result_t gp100_edit_controller_adjust(
    gp100_edit_controller_t *controller,
    gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode,
    size_t control,
    bool increase,
    uint8_t acceleration);

/* Decode one packed Edit-mode event and apply its local state transition. */
gp100_edit_event_result_t gp100_edit_controller_handle_event(
    gp100_edit_controller_t *controller,
    gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    uint16_t *logical_preset,
    gp100_footswitch_mode_t footswitch_mode,
    bool *drum_enabled,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

typedef struct gp100_edit_screen_theme {
    const gp100_bitmap_font_t *large_font;
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t chain_selection_rgb565;
    uint16_t panel_rgb565;
    uint16_t value_rgb565;
    uint16_t track_rgb565;
    uint16_t disabled_rgb565;
    uint16_t effect_rgb565[GP100_EFFECT_FAMILY_COUNT];
} gp100_edit_screen_theme_t;

/*
 * Compose the complete Edit view from one persistent preset record and the
 * small runtime cursor state. page_index is zero-based. In PATCH mode the
 * first page starts with ON/OFF followed by parameters. STOMP mode includes
 * a footswitch assignment before the parameters.
 */
bool gp100_edit_screen_snapshot_from_preset(
    gp100_edit_screen_snapshot_t *snapshot,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    gp100_footswitch_mode_t footswitch_mode,
    size_t selected_position,
    size_t page_index,
    bool manage_signal_chain,
    bool preset_modified);

/* Select the page containing a logical effect parameter. */
bool gp100_edit_screen_parameter_page(
    gp100_footswitch_mode_t footswitch_mode,
    size_t parameter_index,
    size_t *page_index,
    size_t *slot_index);

bool gp100_edit_screen_fill_parameter(
    gp100_edit_parameter_slot_t *slot,
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    const float stored[GP100_PARAMETERS_PER_FAMILY]);

/* Name a catalog model or number its user asset slot. */
const char *gp100_edit_screen_model_name(gp100_effect_family_t family,
                                         size_t algorithm,
                                         char user_name[8]);

bool gp100_edit_screen_default_theme(
    gp100_edit_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_edit_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_edit_screen_snapshot_t *snapshot,
    const gp100_edit_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
