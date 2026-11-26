#include "audio/engine/audio_engine.h"

#include <math.h>
#include <stdio.h>

typedef struct engine_test_context {
    size_t effect_calls;
} engine_test_context_t;

static void add_one_effect(void *context,
                           gp100_effect_family_t family,
                           const gp100_effect_slot_state_t *slot,
                           float *samples,
                           size_t frame_count)
{
    engine_test_context_t *test = context;
    size_t sample;
    (void)family;
    (void)slot;
    test->effect_calls++;
    for (sample = 0u; sample < frame_count * 2u; ++sample) {
        samples[sample] += 1.0f;
    }
}

static void identity_chain(gp100_effect_chain_t *chain)
{
    size_t index;
    *chain = (gp100_effect_chain_t){0};
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        chain->family_at_position[index] =
            (gp100_effect_family_t)index;
        chain->position_by_family[index] = (uint8_t)index;
    }
}

static int test_modes(void)
{
    gp100_audio_engine_t engine;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime;
    gp100_effect_chain_ops_t ops;
    engine_test_context_t context = {0};
    float samples[2] = {1.0f, 1.0f};
    float scratch[1];

    gp100_audio_engine_reset(&engine);
    identity_chain(&chain);
    gp100_effect_runtime_reset(&runtime);
    ops.context = &context;
    ops.process_effect = add_one_effect;
    ops.post_process = NULL;

    if (gp100_audio_engine_process_block(
            &engine, &chain, &runtime, samples, 1u, scratch, 1u, &ops) !=
            GP100_AUDIO_BLOCK_TRANSITION_ONLY ||
        context.effect_calls != 0u || samples[0] != 1.0f) {
        return 1;
    }

    engine.transition.mode = GP100_AUDIO_MODE_EFFECTS;
    engine.output_level.current_percent = 49.0f;
    engine.output_level.target_percent = 50u;
    if (gp100_audio_engine_process_block(
            &engine, &chain, &runtime, samples, 1u, scratch, 1u, &ops) !=
            GP100_AUDIO_BLOCK_EFFECTS ||
        context.effect_calls != GP100_EFFECT_FAMILY_COUNT ||
        fabsf(samples[0] - 5.0f) > 0.000001f ||
        engine.output_level.current_percent != 50.0f) {
        return 2;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_audio_engine_t engine;
    float samples[2] = {1.0f, 1.0f};

    gp100_audio_engine_reset(&engine);
    if (!gp100_audio_engine_set_cabinet_bypass(
            &engine, GP100_CABINET_BYPASS_BOTH) ||
        gp100_audio_engine_set_cabinet_bypass(
            &engine, (gp100_cabinet_bypass_t)4) ||
        gp100_audio_engine_process_block(
            NULL, NULL, NULL, samples, 1u, NULL, 0u, NULL) !=
            GP100_AUDIO_BLOCK_INVALID) {
        return 1;
    }
    engine.transition.mode = (gp100_audio_mode_t)2;
    if (gp100_audio_engine_process_block(
            &engine, NULL, NULL, samples, 1u, NULL, 0u, NULL) !=
            GP100_AUDIO_BLOCK_INVALID ||
        engine.transition.mode != GP100_AUDIO_MODE_STOPPED) {
        return 2;
    }
    return 0;
}

static int test_nonblocking_mode_transition(void)
{
    gp100_audio_engine_t engine;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime;
    gp100_effect_chain_ops_t ops;
    engine_test_context_t context = {0};
    float samples[512];
    float scratch[256];
    size_t sample;

    gp100_audio_engine_reset(&engine);
    identity_chain(&chain);
    gp100_effect_runtime_reset(&runtime);
    ops.context = &context;
    ops.process_effect = add_one_effect;
    ops.post_process = NULL;
    engine.transition.mode = GP100_AUDIO_MODE_EFFECTS;
    engine.transition.target_mode = GP100_AUDIO_MODE_EFFECTS;
    for (sample = 0u; sample < 512u; ++sample) samples[sample] = 1.0f;

    if (!gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_BYPASS) ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_FADE_OUT ||
        gp100_audio_engine_process_block(
            &engine, &chain, &runtime, samples, 256u,
            scratch, 256u, &ops) != GP100_AUDIO_BLOCK_EFFECTS ||
        engine.transition.mode != GP100_AUDIO_MODE_STOPPED ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_FADE_IN ||
        engine.transition.target_mode != GP100_AUDIO_MODE_BYPASS) {
        return 1;
    }
    for (sample = 0u; sample < 512u; ++sample) samples[sample] = 1.0f;
    if (gp100_audio_engine_process_block(
            &engine, &chain, &runtime, samples, 256u,
            scratch, 256u, &ops) != GP100_AUDIO_BLOCK_TRANSITION_ONLY ||
        engine.transition.mode != GP100_AUDIO_MODE_BYPASS ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_IDLE ||
        context.effect_calls != GP100_EFFECT_FAMILY_COUNT ||
        !gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_EFFECTS) ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_FADE_OUT) {
        return 2;
    }

    /* A newer request while fading out replaces the target without restarting
     * the ramp. */
    engine.transition.gain = 127u;
    if (!gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_BYPASS) ||
        engine.transition.gain != 127u ||
        engine.transition.target_mode != GP100_AUDIO_MODE_BYPASS ||
        !gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_STOPPED) ||
        engine.transition.target_mode != GP100_AUDIO_MODE_STOPPED ||
        gp100_audio_engine_request_mode(NULL,
            GP100_AUDIO_MODE_EFFECTS)) {
        return 3;
    }
    engine.transition.mode = GP100_AUDIO_MODE_STOPPED;
    engine.transition.phase = GP100_AUDIO_TRANSITION_IDLE;
    engine.transition.gain = 0u;
    if (!gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_STOPPED) ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_IDLE ||
        !gp100_audio_engine_request_mode(
            &engine, GP100_AUDIO_MODE_EFFECTS) ||
        engine.transition.phase != GP100_AUDIO_TRANSITION_FADE_IN) {
        return 4;
    }
    return 0;
}

static int test_fade_out_silences_block_tail(void)
{
    gp100_audio_engine_t engine;
    float samples[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    gp100_audio_engine_reset(&engine);
    engine.transition.gain = 1u;
    if (!gp100_audio_engine_request_mode(&engine, GP100_AUDIO_MODE_STOPPED) ||
        gp100_audio_engine_process_block(
            &engine, NULL, NULL, samples, 3u, NULL, 0u, NULL) !=
            GP100_AUDIO_BLOCK_TRANSITION_ONLY ||
        engine.transition.mode != GP100_AUDIO_MODE_STOPPED ||
        samples[0] <= 0.0f || samples[1] != samples[0] ||
        samples[2] != 0.0f || samples[3] != 0.0f ||
        samples[4] != 0.0f || samples[5] != 0.0f) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_modes();
    if (result != 0) {
        fprintf(stderr, "mode test failed: %d\n", result);
        return 10 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 20 + result;
    }
    result = test_nonblocking_mode_transition();
    if (result != 0) {
        fprintf(stderr, "mode transition test failed: %d\n", result);
        return 30 + result;
    }
    result = test_fade_out_silences_block_tail();
    if (result != 0) {
        fprintf(stderr, "fade-out tail test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
