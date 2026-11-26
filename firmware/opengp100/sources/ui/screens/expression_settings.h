#ifndef GP100_EXPRESSION_SETTINGS_H
#define GP100_EXPRESSION_SETTINGS_H

#include "ui/render/display_text.h"
#include "audio/effects/effect_catalog.h"
#include "storage/preset_record.h"
#include "ui/ui_geometry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EXPRESSION_MENU_ITEMS 2u
#define GP100_EXPRESSION_CONTROL_SLOTS 3u

typedef enum gp100_expression_menu_item {
    GP100_EXPRESSION_VOLUME_RANGE = 0,
    GP100_EXPRESSION_CALIBRATE
} gp100_expression_menu_item_t;

typedef struct gp100_expression_control {
    char label[16];
    char value[24];
    bool present;
} gp100_expression_control_t;

typedef struct gp100_expression_settings_snapshot {
    gp100_expression_menu_item_t selected_item;
    gp100_expression_control_t controls[
        GP100_EXPRESSION_CONTROL_SLOTS];
} gp100_expression_settings_snapshot_t;

typedef struct gp100_expression_settings_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t panel_rgb565;
    uint16_t value_rgb565;
    uint16_t track_rgb565;
} gp100_expression_settings_theme_t;

typedef enum gp100_expression_calibration_step {
    GP100_EXPRESSION_CALIBRATE_HEEL = 0,
    GP100_EXPRESSION_CALIBRATE_TOE,
    GP100_EXPRESSION_CALIBRATE_SWITCH,
    GP100_EXPRESSION_CALIBRATE_COMPLETE,
    GP100_EXPRESSION_CALIBRATE_FAILED,
    GP100_EXPRESSION_CALIBRATE_SAVE_FAILED
} gp100_expression_calibration_step_t;

typedef struct gp100_expression_calibration {
    gp100_expression_calibration_step_t step;
    uint16_t heel;
    uint16_t toe;
    uint16_t trigger;
} gp100_expression_calibration_t;

const char *gp100_expression_menu_item_name(
    gp100_expression_menu_item_t item);

bool gp100_expression_settings_snapshot_from_preset(
    gp100_expression_settings_snapshot_t *snapshot,
    const gp100_preset_record_t *preset,
    gp100_expression_menu_item_t selected_item);

bool gp100_expression_adjust_volume_range(gp100_preset_record_t *preset,
                                          bool maximum, int step);

void gp100_expression_calibration_init(
    gp100_expression_calibration_t *calibration);

/*
 * raw_value is the current ADC value for heel/toe and the observed peak for
 * the toe-switch step. That step requests a save after validating the captures.
 * The application reports save success or failure before the screen redraws.
 */
bool gp100_expression_calibration_advance(
    gp100_expression_calibration_t *calibration,
    uint16_t raw_value,
    bool *committed);

bool gp100_expression_settings_default_theme(
    gp100_expression_settings_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_expression_settings_render(
    gp100_display_canvas_t *canvas,
    const gp100_expression_settings_snapshot_t *snapshot,
    const gp100_expression_settings_theme_t *theme);

bool gp100_expression_calibration_render(
    gp100_display_canvas_t *canvas,
    const gp100_expression_calibration_t *calibration,
    const gp100_expression_settings_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
