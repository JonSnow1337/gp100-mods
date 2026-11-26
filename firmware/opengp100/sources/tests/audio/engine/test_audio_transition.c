#include "audio/engine/audio_transition.h"

#include <math.h>
#include <stdio.h>

#define TEST_FRAMES 300u

static void fill_samples(float samples[TEST_FRAMES * 2u])
{
    size_t index;
    for (index = 0u; index < TEST_FRAMES * 2u; ++index) {
        samples[index] = 1.0f;
    }
}

static int test_fade_out(void)
{
    gp100_audio_transition_t transition;
    float samples[TEST_FRAMES * 2u];

    transition.mode = GP100_AUDIO_MODE_EFFECTS;
    transition.target_mode = GP100_AUDIO_MODE_EFFECTS;
    transition.phase = GP100_AUDIO_TRANSITION_IDLE;
    transition.gain = 255u;
    fill_samples(samples);
    gp100_audio_transition_begin_fade_out(&transition);
    if (gp100_audio_transition_apply_gain(
            &transition, samples, TEST_FRAMES) != 256u ||
        transition.mode != GP100_AUDIO_MODE_STOPPED ||
        transition.phase != GP100_AUDIO_TRANSITION_IDLE ||
        transition.gain != 0u || samples[0] != 1.0f ||
        fabsf(samples[254u * 2u] - (1.0f / 255.0f)) > 0.000001f ||
        samples[255u * 2u] != 0.0f || samples[256u * 2u] != 1.0f) {
        return 1;
    }
    return 0;
}

static int test_fade_in(void)
{
    gp100_audio_transition_t transition;
    float samples[TEST_FRAMES * 2u];

    gp100_audio_transition_reset(&transition);
    if (transition.mode != GP100_AUDIO_MODE_BYPASS ||
        transition.target_mode != GP100_AUDIO_MODE_BYPASS ||
        transition.phase != GP100_AUDIO_TRANSITION_IDLE ||
        transition.gain != 255u) {
        return 1;
    }
    transition.mode = GP100_AUDIO_MODE_STOPPED;
    transition.gain = 0u;
    fill_samples(samples);
    if (!gp100_audio_transition_begin_fade_in(
            &transition, GP100_AUDIO_MODE_BYPASS) ||
        gp100_audio_transition_apply_gain(
            &transition, samples, TEST_FRAMES) != 256u ||
        transition.mode != GP100_AUDIO_MODE_BYPASS ||
        transition.phase != GP100_AUDIO_TRANSITION_IDLE ||
        transition.gain != 255u || samples[0] != 0.0f ||
        fabsf(samples[254u * 2u] - (254.0f / 255.0f)) > 0.000001f ||
        samples[255u * 2u] != 1.0f || samples[256u * 2u] != 1.0f ||
        gp100_audio_transition_begin_fade_in(
            &transition, GP100_AUDIO_MODE_STOPPED)) {
        return 2;
    }
    return 0;
}

static int test_steady_gain_and_modes(void)
{
    gp100_audio_transition_t transition;
    float samples[4] = {1.0f, -1.0f, 0.5f, -0.5f};

    transition.mode = GP100_AUDIO_MODE_EFFECTS;
    transition.target_mode = GP100_AUDIO_MODE_EFFECTS;
    transition.phase = GP100_AUDIO_TRANSITION_IDLE;
    transition.gain = 128u;
    if (gp100_audio_transition_apply_gain(&transition, samples, 2u) !=
            2u ||
        fabsf(samples[0] - 128.0f / 255.0f) > 0.000001f ||
        fabsf(samples[3] + 64.0f / 255.0f) > 0.000001f ||
        !gp100_audio_mode_processes_effects(
            GP100_AUDIO_MODE_EFFECTS) ||
        gp100_audio_mode_processes_effects(
            GP100_AUDIO_MODE_STOPPED) ||
        gp100_audio_mode_processes_effects(
            GP100_AUDIO_MODE_BYPASS)) {
        return 1;
    }
    transition.gain = 256u;
    return gp100_audio_transition_apply_gain(
               &transition, samples, 2u) == 0u
               ? 0
               : 2;
}

static int test_public_route_states(void)
{
    gp100_audio_mode_t mode = GP100_AUDIO_MODE_BYPASS;
    unsigned int state;

    if (GP100_AUDIO_ROUTE_TUNER_MUTE != 0 ||
        GP100_AUDIO_ROUTE_NORMAL != 1 ||
        GP100_AUDIO_ROUTE_TUNER_BYPASS != 2) {
        return 1;
    }
    for (state = 0u; state <= 5u; ++state) {
        if (!gp100_audio_route_state_valid(
                (gp100_audio_route_state_t)state)) {
            return 2;
        }
    }
    if (gp100_audio_route_state_valid(
            (gp100_audio_route_state_t)6) ||
        gp100_audio_route_state_valid(
            (gp100_audio_route_state_t)-1)) {
        return 3;
    }
    if (!gp100_audio_route_core_mode(
            GP100_AUDIO_ROUTE_TUNER_MUTE, &mode) ||
        mode != GP100_AUDIO_MODE_STOPPED ||
        !gp100_audio_route_core_mode(
            GP100_AUDIO_ROUTE_NORMAL, &mode) ||
        mode != GP100_AUDIO_MODE_EFFECTS ||
        !gp100_audio_route_core_mode(
            GP100_AUDIO_ROUTE_TUNER_BYPASS, &mode) ||
        mode != GP100_AUDIO_MODE_BYPASS) {
        return 4;
    }
    for (state = 3u; state <= 5u; ++state) {
        mode = GP100_AUDIO_MODE_EFFECTS;
        if (gp100_audio_route_core_mode(
                (gp100_audio_route_state_t)state, &mode) ||
            mode != GP100_AUDIO_MODE_EFFECTS) {
            return 5;
        }
    }
    if (gp100_audio_route_core_mode(
            GP100_AUDIO_ROUTE_NORMAL, NULL) ||
        gp100_audio_route_core_mode(
            (gp100_audio_route_state_t)6, &mode)) {
        return 6;
    }
    return 0;
}

int main(void)
{
    int result = test_fade_out();
    if (result != 0) {
        fprintf(stderr, "fade-out test failed: %d\n", result);
        return 10 + result;
    }
    result = test_fade_in();
    if (result != 0) {
        fprintf(stderr, "fade-in test failed: %d\n", result);
        return 20 + result;
    }
    result = test_steady_gain_and_modes();
    if (result != 0) {
        fprintf(stderr, "steady-gain test failed: %d\n", result);
        return 30 + result;
    }
    result = test_public_route_states();
    if (result != 0) {
        fprintf(stderr, "public-route test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
