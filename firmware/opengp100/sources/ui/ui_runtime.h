#ifndef GP100_UI_RUNTIME_H
#define GP100_UI_RUNTIME_H

#include "ui/screens/drum_screen.h"
#include "ui/screens/edit_screen.h"
#include "ui/screens/global_screen.h"
#include "ui/screens/looper_screen.h"
#include "ui/screens/play_screen.h"
#include "ui/screens/patch_editor.h"
#include "ui/screens/save_screen.h"
#include "ui/screens/tuner_screen.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_ui_update {
    GP100_UI_UPDATE_NONE = 0,
    GP100_UI_UPDATE_REDRAW = UINT32_C(1) << 0,
    GP100_UI_UPDATE_SETTINGS = UINT32_C(1) << 1,
    GP100_UI_UPDATE_PRESET = UINT32_C(1) << 2,
    GP100_UI_UPDATE_PRESET_LOAD = UINT32_C(1) << 3,
    GP100_UI_UPDATE_SAVE_COMMIT = UINT32_C(1) << 4,
    GP100_UI_UPDATE_MODE = UINT32_C(1) << 5,
    GP100_UI_UPDATE_LOOPER = UINT32_C(1) << 6,
    GP100_UI_UPDATE_DRUM = UINT32_C(1) << 7,
    GP100_UI_UPDATE_FACTORY_RESET = UINT32_C(1) << 8,
    GP100_UI_UPDATE_CALIBRATION_COMMIT = UINT32_C(1) << 9,
    GP100_UI_UPDATE_TAP_TEMPO = UINT32_C(1) << 10
} gp100_ui_update_t;

typedef struct gp100_ui_runtime {
    gp100_ui_mode_t mode;
    uint16_t logical_preset;
    bool drum_enabled;
    bool drum_release_guard;
    bool tap_tempo_enabled;
    bool play_footswitch_release_pending;
    bool unsaved_warning;          /* Play: PARA was turned with unsaved edits; turn again to go on */
    uint32_t unsaved_warning_ms;   /* clock_ms when the warning appeared */
    uint32_t tap_count;
    uint32_t last_tap_tick;
    bool looper_pair_release_guard;
    bool looper_clear_feedback;
    gp100_tuner_model_t tuner;
    gp100_edit_controller_t edit;
    gp100_patch_editor_t patch_editor;
    gp100_global_controller_t global;
    gp100_save_screen_state_t save;
    gp100_hardware_test_t hardware_test;
} gp100_ui_runtime_t;

bool gp100_ui_runtime_init(
    gp100_ui_runtime_t *runtime,
    gp100_ui_mode_t initial_mode,
    uint16_t logical_preset,
    bool drum_enabled,
    bool effect_enabled,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings);

/*
 * Enter a different mode and initialize only that screen's private state.
 * Requesting the active mode is the factory dispatcher's state-preserving
 * no-op.
 */
bool gp100_ui_runtime_set_mode(
    gp100_ui_runtime_t *runtime,
    gp100_ui_mode_t mode,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings);

/*
 * Dispatch one packed event through the active screen. The update mask tells
 * the application which records/services need persistence or follow-up.
 */
bool gp100_ui_runtime_handle_event(
    gp100_ui_runtime_t *runtime,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    const gp100_event_t *event,
    uint32_t *updates);

#ifdef __cplusplus
}
#endif

#endif
