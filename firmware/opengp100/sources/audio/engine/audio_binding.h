#ifndef GP100_AUDIO_BINDING_H
#define GP100_AUDIO_BINDING_H

#include "app/firmware_runtime.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct gp100_audio_binding_status {
    bool initialized;
    bool running;
    uint32_t processed_blocks;
    uint32_t processing_errors;
    uint32_t clipped_samples;
    uint32_t last_sequence;
    uint32_t rhythm_blocks;
    uint32_t rhythm_errors;
    uint32_t rhythm_dropped_triggers;
    uint32_t looper_blocks;
    uint32_t looper_errors;
    uint32_t looper_wraps;
    uint32_t looper_capacity_stops;
    uint32_t global_eq_blocks;
    uint32_t global_eq_errors;
    uint32_t master_output_errors;
    uint32_t volume_pedal_errors;
    uint32_t input_level_errors;
    uint32_t boundary_filter_errors;
    uint32_t usb_playback_blocks;
    uint32_t usb_playback_errors;
    uint32_t usb_capture_blocks;
    uint32_t usb_capture_errors;
} gp100_audio_binding_status_t;

/* Bind the portable engine to the SAI callback. The built-in dispatcher
 * processes the catalog algorithms across the nine families. CAB
 * records and the factory rhythm banks are loaded from bounded FlexSPI XIP
 * ranges. The runtime's Looper is inserted PRE or POST at its selected factory
 * boundary. Global EQ and the POST Looper run after the recovered effect
 * chain. USB effected capture is taken there, before rhythm is mixed and the
 * reciprocal post-chain biquad is applied. Master volume scales the final
 * monitor mix, and the optional output limiter runs immediately before the
 * DAC conversion. Input level uses the
 * recovered -20..+20 dB table, per-frame gain slew, and clamp at the input
 * boundary. The recovered complementary pre/post biquads surround that route,
 * with independent histories reset when audio is initialized. USB playback
 * joins the monitor mix. USB capture selects dry or effected audio for each
 * channel.
 * The mapped expression volume uses its
 * preset-selected PRE/POST boundary. User-AMP models are loaded from their
 * bounded FlexSPI descriptors and VTSI blobs. The transport remains stopped
 * until Start is requested. */
bool GP100_AudioBindingInitialize(
    gp100_firmware_runtime_t *runtime);

/* Publish a coherent foreground runtime snapshot to the audio interrupt. */
bool GP100_AudioBindingSync(
    const gp100_firmware_runtime_t *runtime);

/* Queue the recalled patch for preparation at zero core gain. */
bool GP100_AudioBindingRecallPreset(
    const gp100_firmware_runtime_t *runtime);

/* Load a queued patch from the foreground once the audio fade completes. */
bool GP100_AudioBindingService(void);

/* Mark a rewritten factory/user cabinet record stale. If it is active, the
 * next audio block reloads it through the bounded FlexSPI asset reader. */
bool GP100_AudioBindingInvalidateCabinet(uint16_t cabinetIndex);

/* Mark a rewritten user-AMP descriptor/model stale. If it is active, the next
 * audio block reloads the descriptor and VTSI blob from FlexSPI. */
bool GP100_AudioBindingInvalidateUserAmp(uint8_t userIndex);

/* Install replacement family processors. NULL selects a fully dry path. */
void GP100_AudioBindingSetEffectOps(
    const gp100_effect_chain_ops_t *ops);

bool GP100_AudioBindingStart(void);
void GP100_AudioBindingStop(void);
void GP100_AudioBindingGetStatus(gp100_audio_binding_status_t *status);

#endif
