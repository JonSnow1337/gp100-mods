#ifndef GP100_LOOPER_SCREEN_H
#define GP100_LOOPER_SCREEN_H

#include "ui/render/display_text.h"
#include "looper/looper_engine.h"
#include "storage/settings_record.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_looper_screen_snapshot {
    const gp100_looper_engine_t *engine;
    const char *preset_name;
    uint16_t logical_preset_index;
    bool clear_feedback;
} gp100_looper_screen_snapshot_t;

/* Factory encoder order: Quick 1..3 followed by PARA. */
typedef enum gp100_looper_screen_control {
    GP100_LOOPER_SCREEN_RECORD_LEVEL = 0,
    GP100_LOOPER_SCREEN_POSITION,
    GP100_LOOPER_SCREEN_PLAYBACK_LEVEL,
    GP100_LOOPER_SCREEN_PRESET
} gp100_looper_screen_control_t;

typedef enum gp100_looper_footswitch {
    GP100_LOOPER_FOOTSWITCH_RECORD_PLAY = 0,
    GP100_LOOPER_FOOTSWITCH_STOP_CLEAR
} gp100_looper_footswitch_t;

typedef enum gp100_looper_event_result {
    GP100_LOOPER_EVENT_INVALID = -1,
    GP100_LOOPER_EVENT_UNHANDLED = 0,
    GP100_LOOPER_EVENT_SETTINGS_CHANGED,
    GP100_LOOPER_EVENT_PRESET_SELECTED,
    GP100_LOOPER_EVENT_ENGINE_ACTION,
    GP100_LOOPER_EVENT_PAIR_RELEASE_GUARDED,
    GP100_LOOPER_EVENT_DRUM_TOGGLED,
    GP100_LOOPER_EVENT_MODE_CHANGED
} gp100_looper_event_result_t;

typedef struct gp100_looper_screen_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t panel_rgb565;
    uint16_t control_rgb565;
    uint16_t accent_rgb565;
    uint16_t record_rgb565;
    uint16_t play_rgb565;
    uint16_t track_rgb565;
} gp100_looper_screen_theme_t;

bool gp100_looper_screen_default_theme(
    gp100_looper_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

/* The factory screen uses these exact user-facing state words. */
const char *gp100_looper_screen_status(
    const gp100_looper_screen_snapshot_t *snapshot);

/*
 * Apply one Looper encoder event. Level controls use detent_count for the
 * factory acceleration behavior; PARA always browses one preset per event.
 */
bool gp100_looper_screen_adjust(
    gp100_looper_engine_t *engine,
    gp100_settings_record_t *settings,
    uint16_t *logical_preset_index,
    gp100_looper_screen_control_t control,
    bool increase,
    uint16_t detent_count);

/* Handle the two Looper footswitches; long press is meaningful for clear. */
bool gp100_looper_screen_handle_footswitch(
    gp100_looper_engine_t *engine,
    gp100_looper_footswitch_t footswitch,
    bool long_press,
    bool *changed,
    bool *clear_feedback);

/* Apply one raw Looper-mode panel event to caller-owned runtime state. */
gp100_looper_event_result_t gp100_looper_screen_handle_event(
    gp100_looper_engine_t *engine,
    gp100_settings_record_t *settings,
    uint16_t *logical_preset_index,
    bool *drum_enabled,
    const gp100_event_t *event,
    bool *pair_release_guard,
    bool *clear_feedback,
    gp100_ui_mode_t *next_mode);

bool gp100_looper_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_looper_screen_snapshot_t *snapshot,
    const gp100_looper_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
