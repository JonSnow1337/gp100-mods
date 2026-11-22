#ifndef GP100_GLOBAL_SCREEN_H
#define GP100_GLOBAL_SCREEN_H

#include "ui/render/display_text.h"
#include "expression_settings.h"
#include "factory_reset_screen.h"
#include "language_screen.h"
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

#define GP100_GLOBAL_MENU_ITEMS 9u
#define GP100_GLOBAL_VISIBLE_ITEMS 3u
#define GP100_GLOBAL_CONTROL_SLOTS 3u

typedef enum gp100_global_menu_item {
    GP100_GLOBAL_IO = 0,
    GP100_GLOBAL_TAP_TEMPO_MODE,
    GP100_GLOBAL_EXPRESSION_SETTING,
    GP100_GLOBAL_USB_AUDIO,
    GP100_GLOBAL_FOOTSWITCH_MODE,
    GP100_GLOBAL_EQ,
    GP100_GLOBAL_LANGUAGE,
    GP100_GLOBAL_FACTORY_RESET,
    GP100_GLOBAL_ABOUT
} gp100_global_menu_item_t;

typedef enum gp100_global_view {
    GP100_GLOBAL_VIEW_MENU = 0,
    GP100_GLOBAL_VIEW_EXPRESSION,
    GP100_GLOBAL_VIEW_LANGUAGE,
    GP100_GLOBAL_VIEW_FACTORY_RESET,
    GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION
} gp100_global_view_t;

typedef enum gp100_global_navigation_result {
    GP100_GLOBAL_NAVIGATION_INVALID = -1,
    GP100_GLOBAL_NAVIGATION_UNCHANGED = 0,
    GP100_GLOBAL_NAVIGATION_UPDATED = 1,
    GP100_GLOBAL_NAVIGATION_EXIT_TO_PLAY = 2
} gp100_global_navigation_result_t;

typedef enum gp100_global_event_result {
    GP100_GLOBAL_EVENT_INVALID = -1,
    GP100_GLOBAL_EVENT_UNHANDLED = 0,
    GP100_GLOBAL_EVENT_NAVIGATED,
    GP100_GLOBAL_EVENT_SETTINGS_CHANGED,
    GP100_GLOBAL_EVENT_PRESET_CHANGED,
    GP100_GLOBAL_EVENT_DRUM_TOGGLE_REQUESTED,
    GP100_GLOBAL_EVENT_FACTORY_RESET_REQUESTED,
    GP100_GLOBAL_EVENT_CALIBRATION_COMMIT,
    GP100_GLOBAL_EVENT_MODE_CHANGED
} gp100_global_event_result_t;

typedef struct gp100_global_controller {
    gp100_global_view_t view;
    gp100_global_menu_item_t menu_item;
    gp100_expression_menu_item_t expression_item;
    size_t page_index;
    gp100_language_screen_state_t language;
    gp100_factory_reset_state_t factory_reset;
    gp100_expression_calibration_t expression_calibration;
    uint16_t calibration_raw;
    uint16_t calibration_peak;
} gp100_global_controller_t;

typedef struct gp100_global_control {
    char label[20];
    char value[20];
    float raw_value;
    float minimum;
    float maximum;
    bool present;
} gp100_global_control_t;

typedef struct gp100_global_screen_snapshot {
    gp100_global_menu_item_t selected_item;
    size_t page_index;
    size_t page_count;
    size_t first_visible_item;
    gp100_global_control_t controls[GP100_GLOBAL_CONTROL_SLOTS];
    char detail[32];
} gp100_global_screen_snapshot_t;

typedef struct gp100_global_screen_theme {
    const gp100_bitmap_font_t *large_font;
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t panel_rgb565;
    uint16_t value_rgb565;
    uint16_t track_rgb565;
} gp100_global_screen_theme_t;

const char *gp100_global_menu_item_name(
    gp100_global_menu_item_t item);

/* Initialize the composed GLOBAL/EXP/language/reset controller at I/O. */
void gp100_global_controller_init(
    gp100_global_controller_t *controller);

/*
 * Factory page identifier: menu 1..9, EXP items 10..13, Language 14,
 * Factory Reset 15, and EXP calibration 16.
 */
unsigned gp100_global_controller_factory_page(
    const gp100_global_controller_t *controller);

/* Apply one positive/negative PARA-encoder step with factory clamping. */
gp100_global_navigation_result_t gp100_global_controller_move(
    gp100_global_controller_t *controller,
    bool forward);

/* Apply a PARA-button release on the current factory page. */
gp100_global_navigation_result_t gp100_global_controller_activate(
    gp100_global_controller_t *controller);

/* Return to the owning page, or request Play when already at the top level. */
gp100_global_navigation_result_t gp100_global_controller_exit(
    gp100_global_controller_t *controller);

/* Build the existing top-level renderer snapshot from controller state. */
bool gp100_global_controller_snapshot(
    const gp100_global_controller_t *controller,
    gp100_global_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    const char *firmware_version);

/*
 * Apply one Quick-knob step to the settings-backed I/O, Tap, USB, or
 * Footswitch panel. control is Quick knob 1..3 encoded as 0..2. The record
 * CRC is refreshed after a successful edit.
 */
bool gp100_global_adjust_panel_setting(
    gp100_settings_record_t *settings,
    gp100_global_menu_item_t item,
    size_t page_index,
    size_t control,
    bool increase);

/* Apply one accelerated Quick-knob step to one of the five Global-EQ pages. */
bool gp100_global_adjust_equalizer(
    gp100_settings_record_t *settings,
    size_t page_index,
    size_t control,
    bool increase,
    uint8_t acceleration);

/* Apply top-level GLOBAL navigation and settings edits from one packed event. */
gp100_global_event_result_t gp100_global_controller_handle_event(
    gp100_global_controller_t *controller,
    gp100_settings_record_t *settings,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

/* Apply Quick-knob edits while inside the nested EXP Settings pages. */
gp100_global_event_result_t
gp100_global_controller_handle_expression_event(
    const gp100_global_controller_t *controller,
    gp100_preset_record_t *preset,
    const gp100_event_t *event);

/*
 * Compose the top-level V2.1 GLOBAL view from its journal record. page_index
 * is zero-based: USB Audio has two pages and Global EQ has five.
 */
bool gp100_global_screen_snapshot_from_settings(
    gp100_global_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    gp100_global_menu_item_t selected_item,
    size_t page_index,
    const char *firmware_version);

bool gp100_global_screen_default_theme(
    gp100_global_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_global_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_global_screen_snapshot_t *snapshot,
    const gp100_global_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
