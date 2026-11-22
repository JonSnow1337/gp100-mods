#ifndef GP100_TUNER_SCREEN_H
#define GP100_TUNER_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/settings_record.h"
#include "tuner/tuner_model.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_tuner_screen_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *large_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t status_background_rgb565;
    uint16_t control_background_rgb565;
    uint16_t control_foreground_rgb565;
    uint16_t near_marker_rgb565;
    uint16_t far_marker_rgb565;
    uint16_t in_tune_rgb565;
} gp100_tuner_screen_theme_t;

typedef enum gp100_tuner_screen_control {
    GP100_TUNER_SCREEN_OUTPUT_MODE = 0,
    GP100_TUNER_SCREEN_REFERENCE_HZ
} gp100_tuner_screen_control_t;

typedef enum gp100_tuner_screen_mode_result {
    GP100_TUNER_SCREEN_MODE_INVALID = -1,
    GP100_TUNER_SCREEN_MODE_UNHANDLED = 0,
    GP100_TUNER_SCREEN_MODE_CHANGED
} gp100_tuner_screen_mode_result_t;

typedef enum gp100_tuner_screen_event_result {
    GP100_TUNER_SCREEN_EVENT_INVALID = -1,
    GP100_TUNER_SCREEN_EVENT_UNHANDLED = 0,
    GP100_TUNER_SCREEN_EVENT_SETTINGS_CHANGED,
    GP100_TUNER_SCREEN_EVENT_MODE_CHANGED
} gp100_tuner_screen_event_result_t;

/* Load the two persistent tuner controls without disturbing detector history. */
bool gp100_tuner_screen_load_settings(
    gp100_tuner_model_t *model,
    const gp100_settings_record_t *settings);

/* Apply one Quick-1/output or Quick-3/reference detent and refresh CRC. */
bool gp100_tuner_screen_adjust(
    gp100_tuner_model_t *model,
    gp100_settings_record_t *settings,
    gp100_tuner_screen_control_t control,
    bool increase);

/* Return to Play on the factory's ordinary button/chord exit gestures. */
gp100_tuner_screen_mode_result_t
gp100_tuner_screen_handle_mode_event(
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

/* Apply one packed Tuner event to settings/model state or screen mode. */
gp100_tuner_screen_event_result_t
gp100_tuner_screen_handle_event(
    gp100_tuner_model_t *model,
    gp100_settings_record_t *settings,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

/* Populate the recovered V2.1.0 tuner palette around caller-owned fonts. */
bool gp100_tuner_screen_default_theme(
    gp100_tuner_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *large_font);

/*
 * Render the complete 160x128 tuner view from portable tuner state. The
 * detector and event handlers update model; this function has no hidden UI
 * state and can therefore be redrawn after any framebuffer loss.
 */
bool gp100_tuner_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_tuner_model_t *model,
    const gp100_tuner_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
