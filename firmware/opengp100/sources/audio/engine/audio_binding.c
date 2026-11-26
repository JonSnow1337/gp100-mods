#include "audio_binding.h"

#include "audio/transport/audio_transport.h"
#include "board/board.h"
#include "board/peripherals.h"
#include "rhythm/rhythm_clock.h"
#include "protocol/usb/usb_device_adapter.h"

#include "audio/dsp/audio_biquad.h"
#include "audio_engine.h"
#include "audio/effects/dynamics/output_limiter.h"
#include "audio/dsp/audio_pcm.h"
#include "audio/effects/effect_processor.h"
#include "audio/effects/tone/global_equalizer.h"
#include "rhythm/rhythm_assets.h"
#include "storage/patch_chain.h"
#include "protocol/usb/usb_audio_capture.h"

#include "fsl_common.h"

#include <string.h>

#define GP100_FLEXSPI_MAPPED_BASE UINT32_C(0x60000000)
#define GP100_PCM24_SCALE 8388608.0f
#define GP100_DSP_PCM_SCALE 7612500.0f

_Static_assert(GP100_AUDIO_BLOCK_FRAMES == GP100_LOOPER_FACTORY_BLOCK_FRAMES,
               "looper and DMA transport must use the same block size");
_Static_assert(GP100_AUDIO_SAMPLE_RATE_HZ == GP100_LIMITER_SAMPLE_RATE &&
               GP100_AUDIO_CHANNEL_COUNT == 2u,
               "output limiter expects the 44.1 kHz stereo DAC stream");

typedef struct gp100_audio_patch
{
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t effects;
    uint16_t tempoBpm;
    uint8_t volumePedalPercent;
    gp100_volume_pedal_position_t volumePedalPosition;
} gp100_audio_patch_t;

static gp100_audio_engine_t s_engine;
static gp100_audio_mode_t s_monitorMode;
static gp100_audio_boundaries_t s_audioBoundaries;
static gp100_effect_chain_t s_chain;
static gp100_effect_runtime_t s_effects;
static gp100_audio_patch_t s_pendingPatch;
static gp100_audio_mode_t s_pendingMode;
static bool s_patchPending;
static bool s_presetRecallPending;
static gp100_effect_chain_ops_t s_effectOps;
static gp100_global_equalizer_t s_globalEqualizer;
static gp100_audio_master_level_t s_masterLevel;
static gp100_output_limiter_t s_outputLimiter;
static gp100_output_limiter_config_t s_outputLimiterConfig;
static gp100_audio_input_level_t s_inputLevel;
static gp100_audio_volume_pedal_t s_volumePedalLevel;
static gp100_volume_pedal_position_t s_volumePedalPosition;
static gp100_rhythm_assets_t s_rhythmAssets;
static gp100_rhythm_player_t s_rhythmPlayer;
static gp100_rhythm_state_t s_rhythmState;
static gp100_rhythm_settings_t s_rhythmSettings;
static gp100_rhythm_hooks_t s_rhythmHooks;
static int32_t s_rhythmMono[GP100_RHYTHM_BLOCK_SAMPLES];
static gp100_looper_engine_t *s_looper;
static int32_t s_looperInputPcm[GP100_AUDIO_BLOCK_SAMPLES];
static int32_t s_looperWetPcm[GP100_AUDIO_BLOCK_SAMPLES];
GP100_SDRAM_BSS_SECTION_ALIGN(
    static gp100_effect_processor_t s_effectProcessors[GP100_PATCH_MAX_BLOCKS], 32);
extern uint8_t __effect_state_start__[];
extern uint8_t __effect_state_end__[];
static bool s_effectLayoutDirty;
static bool s_gateActive;
static uint16_t s_effectTempoBpm;

static bool GP100_AudioBindingReadAsset(void *context, uint32_t flashOffset,
                                       uint8_t *destination, size_t size);
static void GP100_AudioBindingProcessEffect(
    void *context, gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot, float *audio, size_t frames)
{
    (void)context;
    (void)gp100_effect_processor_process(&s_effectProcessors[slot->instance_id],
                                         family, slot, audio, frames);
}
static float s_interleaved[GP100_AUDIO_BLOCK_SAMPLES];
static float s_gateDetector[GP100_AUDIO_BLOCK_SAMPLES];
static float s_channelScratch[GP100_AUDIO_BLOCK_FRAMES];
static int32_t s_usbPlaybackPcm[GP100_AUDIO_BLOCK_SAMPLES];
static int32_t s_usbDryPcm[GP100_AUDIO_BLOCK_SAMPLES];
static int32_t s_usbEffectPcm[GP100_AUDIO_BLOCK_SAMPLES];
static int32_t s_usbCapturePcm[GP100_AUDIO_BLOCK_SAMPLES];
static gp100_usb_record_source_t s_usbRecordLeft;
static gp100_usb_record_source_t s_usbRecordRight;
static volatile gp100_audio_binding_status_t s_status;

static bool GP100_AudioBindingModelsChanged(
    const gp100_effect_runtime_t *effects)
{
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i)
    {
        const gp100_effect_slot_state_t *next = &effects->slots[i];
        const gp100_effect_slot_state_t *current = &s_effects.slots[i];
        if (next->algorithm_loaded != current->algorithm_loaded ||
            (next->algorithm_loaded &&
             (next->family != current->family ||
              next->algorithm_index != current->algorithm_index)))
        {
            return true;
        }
    }
    return false;
}

static void GP100_AudioBindingPublishPatch(
    const gp100_audio_patch_t *patch, bool presetRecalled)
{
    s_effectLayoutDirty |= presetRecalled ||
        GP100_AudioBindingModelsChanged(&patch->effects);
    s_chain = patch->chain;
    s_effects = patch->effects;
    s_effectTempoBpm = patch->tempoBpm;
    s_gateActive = false;
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i)
    {
        const gp100_effect_slot_state_t *slot = &s_effects.slots[i];
        s_gateActive |= slot->enabled && slot->family == GP100_EFFECT_NOISE_REDUCTION &&
                        gp100_effect_available(slot->family, slot->algorithm_index);
        if (!s_effectLayoutDirty && slot->algorithm_loaded)
        {
            (void)gp100_effect_processor_set_tempo_bpm(
                &s_effectProcessors[i], s_effectTempoBpm);
        }
    }
    (void)gp100_looper_set_tempo(s_looper, GP100_AUDIO_SAMPLE_RATE_HZ, patch->tempoBpm);
    s_volumePedalLevel.target_percent = patch->volumePedalPercent;
    s_volumePedalPosition = patch->volumePedalPosition;
}

static bool GP100_AudioBindingPrepareEffects(void)
{
    if (s_effectOps.process_effect != GP100_AudioBindingProcessEffect)
    {
        return true;
    }
    size_t used = 0;
    const bool rebuild = s_effectLayoutDirty;
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i)
    {
        const gp100_effect_slot_state_t *slot = &s_effects.slots[i];
        gp100_effect_processor_t *processor = &s_effectProcessors[i];
        if (rebuild)
        {
            gp100_effect_processor_reset(processor);
        }
        if (!slot->algorithm_loaded ||
            !gp100_effect_available(slot->family, slot->algorithm_index))
        {
            continue;
        }
        if (rebuild)
        {
            const size_t bytes = gp100_patch_state_bytes(slot->family);
            if (bytes == 0u || bytes > GP100_PATCH_DSP_BYTES - used ||
                !gp100_effect_processor_bind(
                    processor, slot->family, __effect_state_start__ + used))
            {
                return false;
            }
            memset(__effect_state_start__ + used, 0, bytes);
            used += bytes;
            if (!gp100_effect_processor_set_asset_reader(
                    processor, GP100_AudioBindingReadAsset, NULL))
            {
                return false;
            }
            gp100_effect_processor_set_gate_detector(
                processor, s_gateDetector, GP100_AUDIO_BLOCK_FRAMES);
        }
        if (!gp100_effect_processor_set_tempo_bpm(processor, s_effectTempoBpm) ||
            !gp100_effect_processor_prepare_slot(processor, slot->family, slot))
        {
            return false;
        }
    }
    s_effectLayoutDirty = false;
    return true;
}

static bool GP100_AudioBindingProcessLooper(const float *interleavedStereo)
{
    gp100_looper_process_result_t result;
    size_t sample;

    if (!gp100_audio_float_to_pcm24(
            interleavedStereo, s_looperInputPcm, GP100_AUDIO_BLOCK_SAMPLES, NULL))
    {
        return false;
    }
    memcpy(s_looperWetPcm, s_looperInputPcm, sizeof(s_looperWetPcm));
    if (!gp100_looper_process_factory_block(
            s_looper, s_looperWetPcm, &result))
    {
        return false;
    }
    /* The Looper API adds wet to its input. The board needs wet separately
     * for the effect input, USB capture, and the monitor mix. */
    for (sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; ++sample)
    {
        s_looperWetPcm[sample] -= s_looperInputPcm[sample];
    }
    ++s_status.looper_blocks;
    if (result.loop_wrapped)
    {
        ++s_status.looper_wraps;
    }
    if (result.capacity_reached)
    {
        ++s_status.looper_capacity_stops;
    }
    return true;
}

static const uint8_t *GP100_AudioBindingAssetPointer(
    uint32_t flashOffset,
    size_t size)
{
    if (flashOffset > BOARD_FLASH_SIZE ||
        size > BOARD_FLASH_SIZE - flashOffset)
    {
        return NULL;
    }
    return (const uint8_t *)(uintptr_t)(
        GP100_FLEXSPI_MAPPED_BASE + flashOffset);
}

static bool GP100_AudioBindingReadAsset(
    void *context,
    uint32_t flashOffset,
    uint8_t *destination,
    size_t size)
{
    const uint8_t *source;

    (void)context;
    if (destination == NULL)
    {
        return false;
    }
    source = GP100_AudioBindingAssetPointer(flashOffset, size);
    if (source == NULL)
    {
        return false;
    }
    memcpy(destination, source, size);
    return true;
}

static void GP100_AudioBindingProcessPostEffects(
    float *interleavedStereo,
    size_t frameCount)
{
    if (!gp100_global_equalizer_process(
            &s_globalEqualizer, interleavedStereo, frameCount))
    {
        ++s_status.global_eq_errors;
    }
    else
    {
        ++s_status.global_eq_blocks;
    }
    if (s_volumePedalPosition == GP100_VOLUME_PEDAL_POST &&
        !gp100_audio_volume_pedal_apply_pcm(
            &s_volumePedalLevel, interleavedStereo, frameCount))
    {
        ++s_status.volume_pedal_errors;
    }
}

static void GP100_AudioBindingMixRhythm(
    float *interleavedStereo,
    size_t frameCount,
    float gain)
{
    bool rhythmReady;
    size_t frame;

    rhythmReady = frameCount == GP100_RHYTHM_BLOCK_SAMPLES &&
        gp100_rhythm_process_block(
            &s_rhythmState, &s_rhythmSettings, &s_rhythmHooks,
            s_rhythmMono, GP100_RHYTHM_BLOCK_SAMPLES);
    if (!rhythmReady)
    {
        ++s_status.rhythm_errors;
    }
    else
    {
        for (frame = 0u; gain != 0.0f && frame < frameCount; ++frame)
        {
            const float rhythm =
                (float)s_rhythmMono[frame] * (gain / GP100_PCM24_SCALE);

            interleavedStereo[frame * 2u] += rhythm;
            interleavedStereo[frame * 2u + 1u] += rhythm;
        }
        ++s_status.rhythm_blocks;
        s_status.rhythm_dropped_triggers =
            gp100_rhythm_player_dropped_triggers(&s_rhythmPlayer);
    }
}

static void GP100_AudioBindingAddPcm(
    float *interleavedStereo,
    const int32_t *source)
{
    size_t sample;

    for (sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; ++sample)
    {
        interleavedStereo[sample] +=
            (float)source[sample] / GP100_PCM24_SCALE;
    }
}

static bool GP100_AudioBindingWriteUsbCapture(bool looperWetReady)
{
    size_t sample;

    if (!gp100_audio_float_to_pcm24(
            s_interleaved, s_usbEffectPcm, GP100_AUDIO_BLOCK_SAMPLES, NULL))
    {
        return false;
    }
    if (looperWetReady)
    {
        for (sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; ++sample)
        {
            s_usbEffectPcm[sample] += s_looperWetPcm[sample];
        }
    }
    return gp100_usb_capture_select_sources(
               s_usbDryPcm, s_usbEffectPcm, s_usbCapturePcm,
               GP100_AUDIO_BLOCK_FRAMES, s_usbRecordLeft, s_usbRecordRight) &&
           GP100_USB_CaptureWrite(s_usbCapturePcm, GP100_AUDIO_BLOCK_FRAMES);
}

static bool GP100_AudioBindingCabinetBypass(
    const gp100_settings_record_t *settings,
    gp100_cabinet_bypass_t *bypass)
{
    int16_t left;
    int16_t right;
    uint32_t value = 0u;

    if (settings == NULL || bypass == NULL ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_NO_CAB_LEFT, &left) ||
        !gp100_settings_get_i16(
            settings, GP100_SETTINGS_NO_CAB_RIGHT, &right))
    {
        return false;
    }
    if (left != 0)
    {
        value |= GP100_CABINET_BYPASS_LEFT;
    }
    if (right != 0)
    {
        value |= GP100_CABINET_BYPASS_RIGHT;
    }
    *bypass = (gp100_cabinet_bypass_t)value;
    return true;
}

static bool GP100_AudioBindingDesiredMode(
    const gp100_firmware_runtime_t *runtime,
    gp100_audio_mode_t *mode)
{
    gp100_tuner_audio_source_t source;

    if (runtime == NULL || mode == NULL)
    {
        return false;
    }
    if (runtime->ui.mode != GP100_UI_MODE_TUNER)
    {
        *mode = runtime->ui.hardware_test.effect_enabled
                    ? GP100_AUDIO_MODE_EFFECTS
                    : GP100_AUDIO_MODE_BYPASS;
        return true;
    }
    if ((uint32_t)runtime->ui.tuner.output_mode >
        (uint32_t)GP100_TUNER_OUTPUT_BYPASS)
    {
        return false;
    }
    source = gp100_tuner_audio_source(
        runtime->ui.tuner.output_mode);
    *mode = source == GP100_TUNER_AUDIO_DRY
                ? GP100_AUDIO_MODE_BYPASS
                : source == GP100_TUNER_AUDIO_EFFECTS
                      ? GP100_AUDIO_MODE_EFFECTS
                      : GP100_AUDIO_MODE_STOPPED;
    return true;
}

static void GP100_AudioBindingProcess(
    const int32_t input[GP100_AUDIO_BLOCK_SAMPLES],
    int32_t output[GP100_AUDIO_BLOCK_SAMPLES],
    uint32_t sequence,
    void *userData)
{
    gp100_audio_block_result_t result;
    bool masterApplied;
    bool looperWetReady = false;
    const bool preLooper = s_looper != NULL &&
        s_looper->position == GP100_LOOPER_PRE_EFFECTS;
    const bool monitorEffects = s_monitorMode == GP100_AUDIO_MODE_EFFECTS;
    const bool monitorMuted = s_monitorMode == GP100_AUDIO_MODE_STOPPED;
    const bool effectsActive = s_engine.transition.mode == GP100_AUDIO_MODE_EFFECTS;
    size_t clipped = 0u;
    size_t frame;
    bool usbDryReady;

    (void)userData;
    if (!gp100_audio_pcm24_to_float(
            input, s_interleaved, GP100_AUDIO_BLOCK_SAMPLES))
    {
        memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
        ++s_status.input_level_errors;
        ++s_status.processing_errors;
        return;
    }
    /* The factory conditions the guitar's left ADC channel as dual mono. */
    for (frame = 0u; frame < GP100_AUDIO_BLOCK_FRAMES; ++frame)
    {
        const float guitar = s_interleaved[2u * frame] * GP100_EFFECT_DSP_FULL_SCALE;
        s_interleaved[2u * frame] = guitar;
        s_interleaved[2u * frame + 1u] = guitar;
    }
    if (!gp100_audio_boundaries_process_pre(
            &s_audioBoundaries, s_interleaved, GP100_AUDIO_BLOCK_FRAMES))
    {
        memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
        ++s_status.boundary_filter_errors;
        ++s_status.processing_errors;
        return;
    }
    /* Input to the stable ADC biquad is PCM24-bounded. Its output fits in
     * int32_t PCM units before the separately smoothed input gain. */
    for (frame = 0u; frame < GP100_AUDIO_BLOCK_SAMPLES; ++frame)
    {
        const int32_t pcm = (int32_t)(s_interleaved[frame] * GP100_DSP_PCM_SCALE);
        s_interleaved[frame] = (float)pcm / GP100_PCM24_SCALE;
    }
    if (!gp100_audio_input_level_apply(
            &s_inputLevel, s_interleaved, GP100_AUDIO_BLOCK_FRAMES))
    {
        memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
        ++s_status.input_level_errors;
        ++s_status.processing_errors;
        return;
    }
    for (frame = 0u; frame < GP100_AUDIO_BLOCK_SAMPLES; ++frame)
    {
        const int32_t pcm = (int32_t)(s_interleaved[frame] * (float)GP100_PCM24_MAX);
        s_interleaved[frame] = (float)pcm / GP100_PCM24_SCALE;
    }
    /* The host playback buffer stays separate until the monitor mix. */
    if (!GP100_USB_PlaybackRead(
            s_usbPlaybackPcm, GP100_AUDIO_BLOCK_FRAMES))
    {
        memset(s_usbPlaybackPcm, 0, sizeof(s_usbPlaybackPcm));
        ++s_status.usb_playback_errors;
    }
    else
    {
        ++s_status.usb_playback_blocks;
    }
    if (preLooper)
    {
        looperWetReady = GP100_AudioBindingProcessLooper(s_interleaved);
        if (!looperWetReady)
        {
            memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
            ++s_status.looper_errors;
            ++s_status.processing_errors;
            return;
        }
    }
    if (s_volumePedalPosition == GP100_VOLUME_PEDAL_PRE &&
        !gp100_audio_volume_pedal_apply_pcm(
            &s_volumePedalLevel, s_interleaved,
            GP100_AUDIO_BLOCK_FRAMES))
    {
        memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
        ++s_status.volume_pedal_errors;
        ++s_status.processing_errors;
        return;
    }
    usbDryReady = gp100_audio_float_to_pcm24(
        s_interleaved, s_usbDryPcm, GP100_AUDIO_BLOCK_SAMPLES, NULL);
    if (looperWetReady)
    {
        GP100_AudioBindingAddPcm(s_interleaved, s_looperWetPcm);
    }
    if (effectsActive)
    {
        /* The nonlinear effects operate in the factory core's PCM units. */
        for (frame = 0u; frame < GP100_AUDIO_BLOCK_SAMPLES; ++frame)
        {
            s_interleaved[frame] *= GP100_EFFECT_DSP_FULL_SCALE;
        }
        if (s_gateActive)
        {
            memcpy(s_gateDetector, s_interleaved, sizeof(s_gateDetector));
        }
    }
    result = gp100_audio_engine_process_block(
        &s_engine, &s_chain, &s_effects,
        s_interleaved, GP100_AUDIO_BLOCK_FRAMES,
        s_channelScratch, GP100_AUDIO_BLOCK_FRAMES, &s_effectOps);
    if (result != GP100_AUDIO_BLOCK_INVALID)
    {
        if (effectsActive)
        {
            for (frame = 0u; frame < GP100_AUDIO_BLOCK_SAMPLES; ++frame)
            {
                s_interleaved[frame] *= GP100_DSP_PCM_SCALE / GP100_PCM24_SCALE;
            }
        }
        GP100_AudioBindingProcessPostEffects(
            s_interleaved, GP100_AUDIO_BLOCK_FRAMES);
        if (s_looper != NULL && !preLooper)
        {
            looperWetReady = GP100_AudioBindingProcessLooper(s_interleaved);
            if (!looperWetReady)
            {
                ++s_status.looper_errors;
            }
        }
    }
    if (result != GP100_AUDIO_BLOCK_INVALID && usbDryReady &&
        GP100_AudioBindingWriteUsbCapture(looperWetReady))
    {
        ++s_status.usb_capture_blocks;
    }
    else
    {
        ++s_status.usb_capture_errors;
    }
    if (result != GP100_AUDIO_BLOCK_INVALID)
    {
        if (monitorEffects && looperWetReady)
        {
            GP100_AudioBindingAddPcm(s_interleaved, s_looperWetPcm);
        }
        GP100_AudioBindingMixRhythm(
            s_interleaved, GP100_AUDIO_BLOCK_FRAMES,
            monitorMuted ? 0.0f : (monitorEffects ? 4.0f : 2.0f));
        if (monitorMuted)
        {
            memset(s_interleaved, 0, sizeof(s_interleaved));
        }
        GP100_AudioBindingAddPcm(s_interleaved, s_usbPlaybackPcm);
        /* Foreground publication follows the core acknowledgement. Keep the
         * prior monitor route for the block that changes the core mode. */
        if (s_engine.transition.mode == s_engine.transition.target_mode)
        {
            s_monitorMode = s_engine.transition.mode;
        }
    }
    if (result != GP100_AUDIO_BLOCK_INVALID &&
        !gp100_audio_boundaries_process_post(
            &s_audioBoundaries, s_interleaved,
            GP100_AUDIO_BLOCK_FRAMES))
    {
        result = GP100_AUDIO_BLOCK_INVALID;
        ++s_status.boundary_filter_errors;
    }
    masterApplied = result != GP100_AUDIO_BLOCK_INVALID &&
        gp100_audio_master_level_apply(
            &s_masterLevel, s_interleaved,
            GP100_AUDIO_BLOCK_FRAMES);
    if (masterApplied)
    {
        gp100_output_limiter_process(&s_outputLimiter, &s_outputLimiterConfig,
                                     s_interleaved, GP100_AUDIO_BLOCK_FRAMES);
    }
    if (result == GP100_AUDIO_BLOCK_INVALID ||
        !masterApplied ||
        !gp100_audio_float_to_pcm24(
            s_interleaved, output, GP100_AUDIO_BLOCK_SAMPLES, &clipped))
    {
        memset(output, 0, sizeof(int32_t) * GP100_AUDIO_BLOCK_SAMPLES);
        if (result != GP100_AUDIO_BLOCK_INVALID && !masterApplied)
        {
            ++s_status.master_output_errors;
        }
        ++s_status.processing_errors;
        return;
    }
    ++s_status.processed_blocks;
    s_status.clipped_samples += (uint32_t)clipped;
    s_status.last_sequence = sequence;
}

static bool GP100_AudioBindingSyncRuntime(
    const gp100_firmware_runtime_t *runtime, bool presetRecalled)
{
    gp100_audio_patch_t patch;
    gp100_cabinet_bypass_t cabinetBypass;
    gp100_global_equalizer_t equalizerConfiguration;
    gp100_output_limiter_config_t limiterConfiguration = s_outputLimiterConfig;
    gp100_sysex_global_state_t globalState;
    gp100_audio_mode_t desiredMode;
    int16_t masterVolume;
    int16_t inputLevel;
    int16_t limiterEnabled;
    int16_t limiterThreshold;
    int16_t limiterCeiling;
    int16_t limiterRelease;
    int16_t usbRecordLeft;
    int16_t usbRecordRight;
    int16_t usbMonitorLevel;
    int16_t usbRecordLevel;
    uint32_t interruptMask;

    if (runtime == NULL ||
        !GP100_AudioBindingCabinetBypass(
            &runtime->settings, &cabinetBypass) ||
        !GP100_AudioBindingDesiredMode(runtime, &desiredMode) ||
        !gp100_settings_get_global_eq(
            &runtime->settings, &globalState) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_MASTER_VOLUME,
            &masterVolume) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_INPUT_LEVEL_DB,
            &inputLevel) ||
        !gp100_settings_get_i16(&runtime->settings, GP100_SETTINGS_LIMITER_ENABLED,
                               &limiterEnabled) ||
        !gp100_settings_get_i16(&runtime->settings, GP100_SETTINGS_LIMITER_THRESHOLD_DB,
                               &limiterThreshold) ||
        !gp100_settings_get_i16(&runtime->settings, GP100_SETTINGS_LIMITER_CEILING_DB,
                               &limiterCeiling) ||
        !gp100_settings_get_i16(&runtime->settings, GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC,
                               &limiterRelease) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT,
            &usbRecordLeft) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT,
            &usbRecordRight) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_MONITOR_LEVEL_DB,
            &usbMonitorLevel) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_LEVEL_DB,
            &usbRecordLevel) ||
        masterVolume < 0 || masterVolume > 100 ||
        inputLevel < -20 || inputLevel > 20 ||
        usbRecordLeft < GP100_USB_RECORD_SOURCE_DRY ||
        usbRecordLeft > GP100_USB_RECORD_SOURCE_EFFECT ||
        usbRecordRight < GP100_USB_RECORD_SOURCE_DRY ||
        usbRecordRight > GP100_USB_RECORD_SOURCE_EFFECT ||
        usbMonitorLevel < -20 || usbMonitorLevel > 6 ||
        usbRecordLevel < GP100_USB_CAPTURE_GAIN_MIN_DB ||
        usbRecordLevel > GP100_USB_CAPTURE_GAIN_MAX_DB ||
        runtime->volume_pedal_percent > 100u ||
        (runtime->volume_pedal_position != GP100_VOLUME_PEDAL_PRE &&
         runtime->volume_pedal_position != GP100_VOLUME_PEDAL_POST))
    {
        return false;
    }
    patch.chain = runtime->chain;
    patch.effects = runtime->effects;
    patch.tempoBpm = gp100_preset_tempo_bpm(&runtime->preset);
    patch.volumePedalPercent = runtime->volume_pedal_percent;
    patch.volumePedalPosition = runtime->volume_pedal_position;
    gp100_global_equalizer_reset(&equalizerConfiguration);
    if (s_looper == NULL || patch.tempoBpm < 40u || patch.tempoBpm > 250u ||
        !gp100_global_equalizer_configure(
            &equalizerConfiguration, &globalState))
    {
        return false;
    }
    gp100_output_limiter_configure(&limiterConfiguration, limiterEnabled != 0,
                                   limiterThreshold, limiterCeiling, limiterRelease, 1.0f);
    interruptMask = DisableGlobalIRQ();
    s_rhythmSettings = runtime->rhythm;
    s_usbRecordLeft = (gp100_usb_record_source_t)usbRecordLeft;
    s_usbRecordRight = (gp100_usb_record_source_t)usbRecordRight;
    if (!GP100_USB_SetAudioLevels(usbMonitorLevel, usbRecordLevel))
    {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    if (s_status.running && (s_patchPending || presetRecalled ||
        GP100_AudioBindingModelsChanged(&patch.effects)))
    {
        s_pendingPatch = patch;
        s_pendingMode = desiredMode;
        s_presetRecallPending |= presetRecalled;
        s_patchPending = true;
        desiredMode = GP100_AUDIO_MODE_STOPPED;
    }
    else
    {
        GP100_AudioBindingPublishPatch(&patch, presetRecalled);
        s_patchPending = false;
    }
    if (!gp100_audio_engine_request_mode(&s_engine, desiredMode))
    {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    s_engine.output_level.target_percent = runtime->output.target_percent;
    s_engine.cabinet_bypass = cabinetBypass;
    memcpy(s_globalEqualizer.coefficients,
           equalizerConfiguration.coefficients,
           sizeof(s_globalEqualizer.coefficients));
    memcpy(s_globalEqualizer.parameters,
           equalizerConfiguration.parameters,
           sizeof(s_globalEqualizer.parameters));
    memcpy(s_globalEqualizer.filter_active,
           equalizerConfiguration.filter_active,
           sizeof(s_globalEqualizer.filter_active));
    s_globalEqualizer.enabled = equalizerConfiguration.enabled;
    s_globalEqualizer.configured = true;
    s_outputLimiterConfig = limiterConfiguration;
    s_masterLevel.target_percent = (uint8_t)masterVolume;
    s_inputLevel.target_db = inputLevel;
    __DMB();
    EnableGlobalIRQ(interruptMask);
    return true;
}

bool GP100_AudioBindingSync(const gp100_firmware_runtime_t *runtime)
{
    return GP100_AudioBindingSyncRuntime(runtime, false);
}

bool GP100_AudioBindingRecallPreset(const gp100_firmware_runtime_t *runtime)
{
    return GP100_AudioBindingSyncRuntime(runtime, true);
}

bool GP100_AudioBindingService(void)
{
    uint32_t interruptMask;

    if (!s_patchPending)
    {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    if (s_engine.transition.mode != GP100_AUDIO_MODE_STOPPED ||
        s_engine.transition.phase != GP100_AUDIO_TRANSITION_IDLE)
    {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    GP100_AudioBindingPublishPatch(&s_pendingPatch, s_presetRecallPending);
    s_patchPending = false;
    s_presetRecallPending = false;
    __DMB();
    EnableGlobalIRQ(interruptMask);

    /* The DMA interrupt keeps running while the muted core loads assets and
     * clears effect histories. Resume processing only after preparation. */
    const bool ready = GP100_AudioBindingPrepareEffects();
    interruptMask = DisableGlobalIRQ();
    if (ready)
    {
        (void)gp100_audio_engine_request_mode(&s_engine, s_pendingMode);
    }
    else
    {
        ++s_status.processing_errors;
    }
    __DMB();
    EnableGlobalIRQ(interruptMask);
    return true;
}

bool GP100_AudioBindingInvalidateCabinet(uint16_t cabinetIndex)
{
    uint32_t interruptMask;
    bool valid;

    if (!s_status.initialized)
    {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    valid = true;
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i)
    {
        valid &= gp100_effect_processor_invalidate_cabinet(
            &s_effectProcessors[i], cabinetIndex);
    }
    __DMB();
    EnableGlobalIRQ(interruptMask);
    return valid;
}

bool GP100_AudioBindingInvalidateUserAmp(uint8_t userIndex)
{
    uint32_t interruptMask;
    bool valid;

    if (!s_status.initialized)
    {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    valid = true;
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i)
    {
        valid &= gp100_effect_processor_invalidate_user_amp(
            &s_effectProcessors[i], userIndex);
    }
    __DMB();
    EnableGlobalIRQ(interruptMask);
    return valid;
}

bool GP100_AudioBindingInitialize(
    gp100_firmware_runtime_t *runtime)
{
    const uint8_t *patternBank = GP100_AudioBindingAssetPointer(
        GP100_RHYTHM_PATTERN_BANK_OFFSET,
        GP100_RHYTHM_PATTERN_BANK_SIZE);
    const uint8_t *sampleBank = GP100_AudioBindingAssetPointer(
        GP100_RHYTHM_SAMPLE_BANK_OFFSET,
        GP100_RHYTHM_SAMPLE_BANK_SIZE);
    gp100_rhythm_clock_t rhythmClock;

    memset((void *)&s_status, 0, sizeof(s_status));
    s_patchPending = false;
    s_presetRecallPending = false;
    if (runtime == NULL)
    {
        return false;
    }
    s_looper = &runtime->looper;
    /* The factory clears the beat-extension gate on startup. */
    s_looper->beat_extension_enabled = false;
    memset(&rhythmClock, 0, sizeof(rhythmClock));
    GP100_RhythmClockGetPlayerClock(&rhythmClock);
    if (patternBank == NULL || sampleBank == NULL ||
        !gp100_rhythm_assets_parse(
            &s_rhythmAssets,
            patternBank, GP100_RHYTHM_PATTERN_BANK_SIZE,
            sampleBank, GP100_RHYTHM_SAMPLE_BANK_SIZE) ||
        !gp100_rhythm_player_init(
            &s_rhythmPlayer,
            s_rhythmAssets.patterns,
            GP100_RHYTHM_FACTORY_PATTERN_COUNT,
            s_rhythmAssets.samples,
            GP100_RHYTHM_FACTORY_SAMPLE_COUNT,
            &rhythmClock))
    {
        return false;
    }
    gp100_rhythm_reset(&s_rhythmState);
    s_rhythmHooks.update_sequencer = gp100_rhythm_player_update;
    s_rhythmHooks.render_voices = gp100_rhythm_player_render;
    s_rhythmHooks.context = &s_rhythmPlayer;
    if ((uintptr_t)__effect_state_end__ - (uintptr_t)__effect_state_start__ !=
        GP100_PATCH_DSP_BYTES)
    {
        return false;
    }
    gp100_effect_runtime_reset(&s_effects);
    s_effectLayoutDirty = true;
    s_effectOps = (gp100_effect_chain_ops_t){
        .process_effect = GP100_AudioBindingProcessEffect
    };
    gp100_audio_engine_reset(&s_engine);
    gp100_audio_boundaries_reset(&s_audioBoundaries);
    gp100_global_equalizer_reset(&s_globalEqualizer);
    gp100_audio_master_level_reset(&s_masterLevel);
    memset(&s_outputLimiter, 0, sizeof(s_outputLimiter));
    gp100_audio_input_level_reset(&s_inputLevel);
    gp100_audio_volume_pedal_reset(&s_volumePedalLevel);
    s_volumePedalPosition = GP100_VOLUME_PEDAL_POST;
    s_engine.transition.mode = GP100_AUDIO_MODE_EFFECTS;
    s_engine.transition.target_mode = GP100_AUDIO_MODE_EFFECTS;
    s_monitorMode = GP100_AUDIO_MODE_EFFECTS;
    if (!GP100_AudioBindingSync(runtime))
    {
        return false;
    }
    s_status.initialized = true;
    return true;
}

void GP100_AudioBindingSetEffectOps(
    const gp100_effect_chain_ops_t *ops)
{
    uint32_t interruptMask = DisableGlobalIRQ();

    if (ops == NULL)
    {
        memset(&s_effectOps, 0, sizeof(s_effectOps));
    }
    else
    {
        s_effectOps = *ops;
    }
    __DMB();
    EnableGlobalIRQ(interruptMask);
}

bool GP100_AudioBindingStart(void)
{
    if (!s_status.initialized)
    {
        return false;
    }
    if (s_status.running)
    {
        return true;
    }
    if (!GP100_AudioBindingPrepareEffects())
    {
        ++s_status.processing_errors;
        return false;
    }
    GP100_AudioTransportSetCallback(GP100_AudioBindingProcess, NULL);
    if (!GP100_AudioTransportStart())
    {
        GP100_AudioTransportSetCallback(NULL, NULL);
        return false;
    }
    GP100_USB_SetAudioActive(true);
    s_status.running = true;
    return true;
}

void GP100_AudioBindingStop(void)
{
    GP100_USB_SetAudioActive(false);
    GP100_AudioTransportStop();
    GP100_AudioTransportSetCallback(NULL, NULL);
    s_status.running = false;
    if (s_patchPending)
    {
        GP100_AudioBindingPublishPatch(&s_pendingPatch, s_presetRecallPending);
        (void)gp100_audio_engine_request_mode(&s_engine, s_pendingMode);
        s_patchPending = false;
        s_presetRecallPending = false;
    }
}

void GP100_AudioBindingGetStatus(gp100_audio_binding_status_t *status)
{
    uint32_t interruptMask;

    if (status == NULL)
    {
        return;
    }
    interruptMask = DisableGlobalIRQ();
    *status = *(const gp100_audio_binding_status_t *)&s_status;
    EnableGlobalIRQ(interruptMask);
}
