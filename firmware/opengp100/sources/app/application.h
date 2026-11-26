#ifndef GP100_APPLICATION_H
#define GP100_APPLICATION_H

#include "firmware_runtime.h"
#include "storage/flash_update.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum gp100_application_source {
    kGP100_ApplicationSourceFlash = 0,
    kGP100_ApplicationSourceFactoryFallback,
    kGP100_ApplicationSourceDevelopmentFallback
} gp100_application_source_t;

typedef struct gp100_application_status {
    bool initialized;
    bool controls_running;
    bool analog_running;
    bool midi_running;
    bool audio_running;
    bool tuner_running;
    bool persistence_configured;
    bool display_initialized;
    gp100_application_source_t preset_source;
    gp100_application_source_t settings_source;
    gp100_application_source_t genre_source;
    gp100_application_source_t expression_calibration_source;
    uint32_t settings_repairs;
    uint16_t expression_calibration_heel;
    uint16_t expression_calibration_toe;
    uint16_t expression_activation_threshold;
    uint32_t events_handled;
    uint32_t event_errors;
    uint32_t analog_pairs;
    uint32_t expression_events;
    uint32_t midi_bytes;
    uint32_t midi_messages;
    uint32_t midi_errors;
    uint32_t midi_dropped;
    uint32_t midi_uart_errors;
    uint32_t sysex_reads;
    uint32_t sysex_writes;
    uint32_t sysex_unhandled;
    uint32_t sysex_response_overruns;
    uint32_t factory_reset_requests;
    uint32_t audio_blocks;
    uint32_t audio_errors;
    uint32_t audio_sync_errors;
    uint32_t audio_clipped_samples;
    uint32_t rhythm_audio_blocks;
    uint32_t rhythm_audio_errors;
    uint32_t rhythm_dropped_triggers;
    uint32_t looper_audio_blocks;
    uint32_t looper_audio_errors;
    uint32_t looper_wraps;
    uint32_t looper_capacity_stops;
    uint32_t tuner_windows;
    uint32_t tuner_readings;
    uint32_t tuner_rejected_windows;
    uint32_t preset_writes;
    uint32_t cabinet_writes;
    uint32_t user_amp_writes;
    uint32_t settings_writes;
    uint32_t genre_writes;
    uint32_t calibration_writes;
    uint32_t persistence_errors;
    uint32_t preset_loads;
    uint32_t display_frames;
    uint32_t display_errors;
    uint32_t last_ui_updates;
} gp100_application_status_t;

/*
 * Compose the readable V2.1 runtime from records retained in the memory-mapped
 * NOR. An erased user slot falls back to its factory preset. Reconstructed
 * settings and a deterministic development preset cover unavailable records.
 * This function does not command the display, start audio, or write flash.
 */
bool GP100_ApplicationInitialize(void);

/* Apply the recovered active-low DRUM + footswitch-minus startup selection
 * before the first frame is rendered. */
bool GP100_ApplicationSelectStartupMode(uint32_t gpio1Snapshot);

/* Initialize the recovered panel and draw the current application view. */
bool GP100_ApplicationInitializeDisplay(void);
bool GP100_ApplicationRender(void);

/* Start/stop only the known input-only QTMR2 GPIO scanner. */
bool GP100_ApplicationStartControls(void);
void GP100_ApplicationStopControls(void);

/* Start/stop the known ADC1_IN6 expression-pedal publication path. */
bool GP100_ApplicationStartAnalog(void);
void GP100_ApplicationStopAnalog(void);

/* Start/stop DIN MIDI receive and foreground message application. */
bool GP100_ApplicationStartMidi(void);
void GP100_ApplicationStopMidi(void);

/* Retrieve one factory-shaped response produced by a vendor SysEx read.
 * DIN and USB MIDI share this interface. A too-small destination leaves
 * the response pending and reports its size. */
bool GP100_ApplicationTakeMidiResponse(
    uint8_t *destination,
    size_t capacity,
    size_t *responseSize);

/* Consume one confirmed front-panel or vendor-SysEx factory-reset request.
 * This does not erase flash or transfer control itself. */
bool GP100_ApplicationTakeFactoryResetRequest(void);

/* Drive the portable progress controller around a board-owned reset. The
 * begin helper also stops audio before the first destructive sector erase. */
bool GP100_ApplicationFactoryResetBegin(size_t totalSteps);
bool GP100_ApplicationFactoryResetUpdate(size_t completedSteps);

/* Start/stop the SAI engine explicitly. The built-in path handles all 185
 * catalog entries across nine families: 180 fixed algorithms plus five
 * flash-backed user-AMP slots using the shared SnapTone core. Codec
 * configuration remains board policy. */
bool GP100_ApplicationStartAudio(void);
void GP100_ApplicationStopAudio(void);
void GP100_ApplicationSetAudioEffectOps(
    const gp100_effect_chain_ops_t *ops);

/* Physical GPIO1_IO21 conditioning remains board-dependent, so capture is
 * explicit unless GP100_AUTOSTART_TUNER_CAPTURE is selected. Factory capture
 * runs continuously; accepted windows are reduced/redrawn only in Tuner mode. */
bool GP100_ApplicationStartTunerCapture(void);
void GP100_ApplicationStopTunerCapture(void);

/* Install a board-specific logical-offset NOR backend. Passing NULL disables
 * writes. The backend must make XIP erase/program calls safe on this MCU. */
void GP100_ApplicationSetFlashHooks(
    const gp100_flash_update_hooks_t *hooks);

/* Drain all currently queued controls. Returns true when any event was seen. */
bool GP100_ApplicationService(void);

gp100_firmware_runtime_t *GP100_ApplicationRuntime(void);
const gp100_application_status_t *GP100_ApplicationStatus(void);

#endif
