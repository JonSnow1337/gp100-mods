#include "rhythm/rhythm_engine.h"

#include <stdio.h>
#include <string.h>

typedef struct hook_log {
    unsigned update_calls;
    unsigned render_calls;
    uint16_t pattern_index;
    bool enabled;
    uint16_t tempo_bpm;
    bool fail_update;
    bool fail_render;
} hook_log_t;

static bool update_sequencer(
    void *context,
    uint16_t pattern_index,
    bool enabled,
    uint16_t tempo_bpm)
{
    hook_log_t *log = (hook_log_t *)context;

    ++log->update_calls;
    log->pattern_index = pattern_index;
    log->enabled = enabled;
    log->tempo_bpm = tempo_bpm;
    return !log->fail_update;
}

static bool render_voices(
    void *context,
    int32_t *mono_samples,
    size_t sample_count)
{
    hook_log_t *log = (hook_log_t *)context;
    size_t sample;

    ++log->render_calls;
    for (sample = 0u; sample < sample_count; ++sample) {
        mono_samples[sample] = 256;
    }
    return !log->fail_render;
}

static int test_processing_order_and_ramp(void)
{
    gp100_rhythm_state_t state;
    const gp100_rhythm_settings_t settings = {
        7u, true, 120u, 2
    };
    hook_log_t log = {0};
    const gp100_rhythm_hooks_t hooks = {
        update_sequencer, render_voices, &log
    };
    int32_t samples[GP100_RHYTHM_BLOCK_SAMPLES];

    gp100_rhythm_reset(&state);
    if (!gp100_rhythm_process_block(
            &state, &settings, &hooks,
            samples, GP100_RHYTHM_BLOCK_SAMPLES) ||
        log.update_calls != 1u || log.render_calls != 1u ||
        log.pattern_index != 7u || !log.enabled || log.tempo_bpm != 120u ||
        samples[0] != 1 || samples[1] != 2 || samples[63] != 2 ||
        state.current_gain_q8 != 2) {
        return 1;
    }
    return 0;
}

static int test_down_ramp(void)
{
    gp100_rhythm_state_t state = {3};
    const gp100_rhythm_settings_t settings = {
        0u, false, 80u, 1
    };
    hook_log_t log = {0};
    const gp100_rhythm_hooks_t hooks = {
        update_sequencer, render_voices, &log
    };
    int32_t samples[GP100_RHYTHM_BLOCK_SAMPLES];

    if (!gp100_rhythm_process_block(
            &state, &settings, &hooks,
            samples, GP100_RHYTHM_BLOCK_SAMPLES) ||
        samples[0] != 2 || samples[1] != 1 || state.current_gain_q8 != 1) {
        return 1;
    }
    return 0;
}

static int test_validation_and_hook_failure(void)
{
    gp100_rhythm_state_t state;
    const gp100_rhythm_settings_t settings = {
        0u, false, 80u, 0
    };
    hook_log_t log = {0};
    const gp100_rhythm_hooks_t hooks = {
        update_sequencer, render_voices, &log
    };
    int32_t samples[GP100_RHYTHM_BLOCK_SAMPLES];

    gp100_rhythm_reset(&state);
    if (gp100_rhythm_process_block(
            NULL, &settings, &hooks,
            samples, GP100_RHYTHM_BLOCK_SAMPLES) ||
        gp100_rhythm_process_block(
            &state, &settings, &hooks, samples, 63u)) {
        return 1;
    }
    log.fail_update = true;
    if (gp100_rhythm_process_block(
            &state, &settings, &hooks,
            samples, GP100_RHYTHM_BLOCK_SAMPLES) ||
        log.render_calls != 0u) {
        return 2;
    }
    return 0;
}

static int test_settings_adapter(void)
{
    gp100_settings_record_t record;
    gp100_rhythm_settings_t settings;

    memset(&record, 0, sizeof(record));
    if (!gp100_settings_set_i16(
            &record, GP100_SETTINGS_DRUM_STYLE, 42) ||
        !gp100_settings_set_i16(
            &record, GP100_SETTINGS_DRUM_TEMPO, 10) ||
        !gp100_settings_set_i16(
            &record, GP100_SETTINGS_DRUM_VOLUME, 81) ||
        !gp100_rhythm_settings_from_record(
            &record, true, &settings) ||
        settings.pattern_index != 42u || !settings.enabled ||
        settings.tempo_bpm != 10u || settings.target_gain_q8 != 81 ||
        !gp100_settings_set_i16(
            &record, GP100_SETTINGS_DRUM_TEMPO, 9) ||
        gp100_rhythm_settings_from_record(
            &record, false, &settings) ||
        gp100_rhythm_settings_from_record(
            NULL, false, &settings)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_processing_order_and_ramp();
    if (result != 0) {
        fprintf(stderr, "processing/ramp test failed: %d\n", result);
        return 10 + result;
    }
    result = test_down_ramp();
    if (result != 0) {
        fprintf(stderr, "down-ramp test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation_and_hook_failure();
    if (result != 0) {
        fprintf(stderr, "validation/hook test failed: %d\n", result);
        return 30 + result;
    }
    result = test_settings_adapter();
    if (result != 0) {
        fprintf(stderr, "settings adapter test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
