#ifndef GP100_PLAY_SCREEN_H
#define GP100_PLAY_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/preset_record.h"
#include "storage/patch_chain.h"
#include "storage/settings_record.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"
#include "protocol/midi/sysex/vendor_sysex_system.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PLAY_STOMP_SWITCH_COUNT 2u
#define GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH 3u
#define GP100_PLAY_UNASSIGNED_FAMILY UINT16_MAX

/* Factory encoder order: Quick 1..3 followed by PARA. */
typedef enum gp100_play_control {
    GP100_PLAY_CONTROL_MASTER_VOLUME = 0,
    GP100_PLAY_CONTROL_TEMPO,
    GP100_PLAY_CONTROL_PRESET_VOLUME,
    GP100_PLAY_CONTROL_PRESET
} gp100_play_control_t;

typedef enum gp100_play_action_result {
    GP100_PLAY_ACTION_INVALID = -1,
    GP100_PLAY_ACTION_UNCHANGED = 0,
    GP100_PLAY_ACTION_SETTINGS_CHANGED,
    GP100_PLAY_ACTION_PRESET_CHANGED,
    GP100_PLAY_ACTION_SELECT_PRESET,
    GP100_PLAY_ACTION_TAP_TEMPO_TOGGLED,
    GP100_PLAY_ACTION_TAP_TEMPO_ARMED,
    GP100_PLAY_ACTION_TAP_TEMPO_APPLIED
} gp100_play_action_result_t;

typedef enum gp100_play_mode_result {
    GP100_PLAY_MODE_INVALID = -1,
    GP100_PLAY_MODE_UNHANDLED = 0,
    GP100_PLAY_MODE_DRUM_TOGGLED,
    GP100_PLAY_MODE_CHANGED
} gp100_play_mode_result_t;

typedef enum gp100_play_event_result {
    GP100_PLAY_EVENT_INVALID = -1,
    GP100_PLAY_EVENT_UNHANDLED = 0,
    GP100_PLAY_EVENT_SETTINGS_CHANGED,
    GP100_PLAY_EVENT_PRESET_CHANGED,
    GP100_PLAY_EVENT_PRESET_SELECTED,
    GP100_PLAY_EVENT_DRUM_TOGGLED,
    GP100_PLAY_EVENT_TAP_TEMPO_TOGGLED,
    GP100_PLAY_EVENT_TAP_TEMPO_ARMED,
    GP100_PLAY_EVENT_TAP_TEMPO_APPLIED,
    GP100_PLAY_EVENT_MODE_CHANGED
} gp100_play_event_result_t;

typedef struct gp100_play_effect {
    gp100_effect_family_t family;
    uint16_t algorithm;
    bool enabled;
} gp100_play_effect_t;

typedef struct gp100_play_screen_snapshot {
    uint16_t logical_preset;
    char preset_name[GP100_PRESET_NAME_SIZE + 1u];
    gp100_footswitch_mode_t footswitch_mode;
    uint16_t master_volume;
    uint16_t tempo_bpm;
    uint16_t preset_volume;
    uint16_t stomp_family[GP100_PLAY_STOMP_SWITCH_COUNT]
                          [GP100_PLAY_STOMP_ASSIGNMENTS_PER_SWITCH];
    bool effect_enabled[GP100_EFFECT_FAMILY_COUNT];
    bool native;
    uint8_t stomp_count[GP100_PLAY_STOMP_SWITCH_COUNT];
    gp100_play_effect_t stomp_effects[GP100_PLAY_STOMP_SWITCH_COUNT]
                                    [GP100_PATCH_MAX_BLOCKS];
    bool expression_active;
    gp100_play_effect_t expression_pedal;
    bool preset_modified;
    bool drum_enabled;
} gp100_play_screen_snapshot_t;

typedef struct gp100_play_screen_theme {
    const gp100_bitmap_font_t *large_font;
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t top_rgb565;
    uint16_t middle_rgb565;
    uint16_t bottom_rgb565;
    uint16_t track_rgb565;
    uint16_t disabled_rgb565;
    uint16_t effect_rgb565[GP100_EFFECT_FAMILY_COUNT];
    uint16_t accent_rgb565;
} gp100_play_screen_theme_t;

/*
 * Build the persistent part of the live Play view. The expression VOL/EXP
 * state is preset-backed; modified and drum-enabled remain runtime state.
 */
bool gp100_play_screen_snapshot_from_records(
    gp100_play_screen_snapshot_t *snapshot,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    bool preset_modified,
    bool drum_enabled);

/* Apply one Play encoder event and update the owning record's CRC. */
gp100_play_action_result_t gp100_play_adjust(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    gp100_play_control_t control,
    bool increase,
    uint8_t detent_count);

/* Handle a short release of footswitch - (0) or + (1). */
gp100_play_action_result_t gp100_play_footswitch(
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    size_t footswitch);

/* Handle HOLD: minus toggles Tap Tempo, plus toggles PATCH/STOMP. */
gp100_play_action_result_t gp100_play_footswitch_hold(
    gp100_settings_record_t *settings,
    bool *tap_tempo_enabled,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    size_t footswitch,
    uint32_t event_tick);

/* Convert a footswitch-release timestamp to the factory 40..250 BPM range. */
gp100_play_action_result_t gp100_play_tap_tempo(
    gp100_preset_record_t *preset,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    uint32_t event_tick);

/* Apply the Play screen's buttons and paired-footswitch mode gestures. */
gp100_play_mode_result_t gp100_play_handle_mode_event(
    const gp100_event_t *event,
    bool *drum_enabled,
    gp100_ui_mode_t *next_mode);

/* Apply one packed Play-mode event to records and caller-owned live state. */
gp100_play_event_result_t gp100_play_handle_event(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    bool *drum_enabled,
    bool *tap_tempo_enabled,
    bool *footswitch_release_pending,
    uint32_t *tap_count,
    uint32_t *last_tap_tick,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

/* Format logical index 0..197 as the factory's P01..P99/F01..F99 code. */
bool gp100_play_screen_patch_code(uint16_t logical_preset,
                                       char code[4]);

/* Short portable labels for the factory's nine colored STOMP glyphs. */
const char *gp100_play_screen_family_label(
    gp100_effect_family_t family);

bool gp100_play_screen_default_theme(
    gp100_play_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_play_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_play_screen_snapshot_t *snapshot,
    const gp100_play_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
