#ifndef GP100_DRUM_SCREEN_H
#define GP100_DRUM_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/preset_record.h"
#include "storage/settings_record.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DRUM_GENRE_COUNT 8u
#define GP100_DRUM_FACTORY_STYLE_COUNT 100u

/* Factory encoder order: Quick 1..3 followed by PARA. */
typedef enum gp100_drum_control {
    GP100_DRUM_CONTROL_STYLE = 0,
    GP100_DRUM_CONTROL_TEMPO,
    GP100_DRUM_CONTROL_VOLUME,
    GP100_DRUM_CONTROL_GENRE
} gp100_drum_control_t;

typedef enum gp100_drum_event_result {
    GP100_DRUM_EVENT_INVALID = -1,
    GP100_DRUM_EVENT_UNHANDLED = 0,
    GP100_DRUM_EVENT_SETTINGS_CHANGED,
    GP100_DRUM_EVENT_PLAYBACK_TOGGLED,
    GP100_DRUM_EVENT_RELEASE_GUARDED,
    GP100_DRUM_EVENT_PRESET_CHANGED,
    GP100_DRUM_EVENT_PRESET_SELECTED,
    GP100_DRUM_EVENT_MODE_CHANGED
} gp100_drum_event_result_t;

typedef struct gp100_drum_screen_snapshot {
    bool enabled;
    uint16_t genre;
    uint16_t absolute_style;
    uint16_t style_in_genre;
    uint16_t tempo_bpm;
    uint16_t volume;
} gp100_drum_screen_snapshot_t;

typedef struct gp100_drum_screen_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t panel_rgb565;
    uint16_t control_rgb565;
    uint16_t accent_rgb565;
    uint16_t track_rgb565;
} gp100_drum_screen_theme_t;

/* Build the live view from the settings journal fields used by the factory. */
bool gp100_drum_screen_snapshot_from_settings(
    gp100_drum_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    bool enabled);

/* Apply one encoder detent and refresh the settings-record CRC. */
bool gp100_drum_adjust_setting(
    gp100_settings_record_t *settings,
    gp100_drum_control_t control,
    bool increase);

/* Handle the shared PARA/DRUM-button playback action. */
bool gp100_drum_toggle_playback(bool *enabled);

/* Decode and apply one raw Drum-mode encoder or screen-navigation event. */
gp100_drum_event_result_t gp100_drum_handle_event(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    bool *enabled,
    bool *drum_release_guard,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

const char *gp100_drum_genre_name(uint16_t genre);
const char *gp100_drum_style_name(uint16_t absolute_style);

/* Return the factory four-row genre window and selected row. */
bool gp100_drum_genre_window(uint16_t genre,
                                  uint16_t *first_genre,
                                  uint16_t *selected_row);

bool gp100_drum_screen_default_theme(
    gp100_drum_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_drum_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_drum_screen_snapshot_t *snapshot,
    const gp100_drum_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
