#ifndef GP100_FIRMWARE_RUNTIME_H
#define GP100_FIRMWARE_RUNTIME_H

#include "ui/ui_live_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Complete portable live state for one firmware instance.  Hardware drivers,
 * flash programming, display transport, and the audio interrupt remain board
 * responsibilities; this object owns the state they consume.
 */
typedef struct gp100_firmware_runtime {
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_ui_runtime_t ui;
    gp100_looper_engine_t looper;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t effects;
    gp100_audio_output_level_t output;
    uint8_t volume_pedal_percent;
    gp100_volume_pedal_position_t volume_pedal_position;
    gp100_rhythm_settings_t rhythm;
    gp100_effect_catalog_t catalog;
    uint16_t calibration_heel;
    uint16_t calibration_toe;
    bool preset_modified;
    bool settings_dirty;
    uint32_t clock_ms;      /* millisecond clock, kept current by the application before each event */
} gp100_firmware_runtime_t;

typedef struct gp100_firmware_runtime_result {
    gp100_ui_live_result_t live;
    bool preset_became_modified;
    bool settings_changed;
    bool settings_became_dirty;
} gp100_firmware_runtime_result_t;

/*
 * Deterministic, semantically valid records for development after an erased
 * flash.  They are an integration policy, not a claim about factory-reset
 * bytes.  Both returned records carry a valid CRC.
 */
bool gp100_make_development_preset(
    gp100_preset_record_t *preset,
    const char *name);
bool gp100_make_development_settings(
    gp100_settings_record_t *settings);

/* Reconstruct the complete settings payload emitted by factory initializer
 * 0x6584, including its data-revision word at offset 0x2c. */
bool gp100_make_factory_settings(
    gp100_settings_record_t *settings);

/*
 * Initialize every live consumer transactionally from caller-loaded records.
 * The records must have valid CRCs and semantically valid screen settings.
 */
bool gp100_firmware_runtime_init(
    gp100_firmware_runtime_t *runtime,
    uint16_t logical_preset,
    const gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    int32_t *looper_storage,
    size_t looper_storage_samples,
    uint16_t calibration_heel,
    uint16_t calibration_toe);

/* Apply one event, including transactional preset recall when requested.
 * Successful recall also records the current logical preset in settings. */
bool gp100_firmware_runtime_handle_event(
    gp100_firmware_runtime_t *runtime,
    const gp100_event_t *event,
    gp100_preset_loader_t loader,
    void *loader_context,
    gp100_firmware_runtime_result_t *result);

/* Publish the mapped 0..100 volume-pedal result and PRE/POST placement. */
bool gp100_firmware_runtime_set_volume_pedal(
    gp100_firmware_runtime_t *runtime,
    float percent,
    gp100_volume_pedal_position_t position);

/* Publish a durably saved record when it belongs to the active slot. */
void gp100_firmware_runtime_mark_preset_saved(
    gp100_firmware_runtime_t *runtime,
    uint32_t flash_address,
    const gp100_preset_record_t *saved_preset);

/* Clear dirty state after the board layer has durably persisted settings. */
void gp100_firmware_runtime_mark_settings_saved(
    gp100_firmware_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif
