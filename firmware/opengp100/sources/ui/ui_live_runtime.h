#ifndef GP100_UI_LIVE_RUNTIME_H
#define GP100_UI_LIVE_RUNTIME_H

#include "input/expression_event.h"
#include "app/preset_runtime.h"
#include "rhythm/rhythm_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_ui_live_result {
    uint32_t ui_updates;
    uint32_t effect_actions[GP100_EFFECT_FAMILY_COUNT];
    gp100_expression_event_status_t expression_status;
    gp100_expression_event_result_t expression;
    bool chain_changed;
    bool output_level_changed;
    bool rhythm_changed;
    bool tap_tempo_changed;
    uint16_t tap_tempo_sync_slots[GP100_EFFECT_FAMILY_COUNT];
    bool preset_loaded;
    uint32_t preset_repairs;
    bool save_record_ready;
    uint32_t save_flash_address;
    gp100_preset_record_t save_record;
} gp100_ui_live_result_t;

/*
 * Apply one front-panel event and publish record changes to portable live
 * consumers. PRESET_LOAD, flash persistence, and audio gain placement remain
 * explicit board-layer follow-up indicated by the result. SAVE_COMMIT includes
 * the validated record and destination address to persist.
 */
bool gp100_ui_live_handle_event(
    gp100_ui_runtime_t *ui,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_rhythm_settings_t *rhythm,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_ui_live_result_t *result);

/*
 * Apply one event and satisfy PRESET_LOAD through the supplied storage
 * callback. State is published only after both the event and complete recall
 * pipeline succeed. A handled load is reported by preset_loaded and the
 * returned update mask no longer contains PRESET_LOAD.
 */
bool gp100_ui_live_handle_event_and_recall(
    gp100_ui_runtime_t *ui,
    gp100_preset_record_t *preset,
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_rhythm_settings_t *rhythm,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_preset_loader_t loader,
    void *loader_context,
    const gp100_effect_catalog_t *catalog,
    gp100_ui_live_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
