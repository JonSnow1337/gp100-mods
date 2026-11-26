#include "audio/effects/tone/acoustic_simulator.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_parameter_mapping_and_modes(void)
{
    static const float expected_first[4] = {
        0.918484747f, 0.920969069f, 1.1655314f, 0.698704123f
    };
    static const float expected_second[4] = {
        -2.53256083f, -2.5258646f, -2.81692696f, -1.93220568f
    };
    uint8_t mode;

    for (mode = 0u; mode < 4u; ++mode) {
        gp100_ac_sim_t simulator;
        float audio[4] = {1.0f, -9.0f, 0.0f, 4.0f};

        if (!gp100_ac_sim_reset(
                &simulator, 50u, 50u, 50u, mode) ||
            !close_enough(simulator.body, 0.2012f, 0.0000001f) ||
            !close_enough(simulator.top, 0.3043f, 0.0000001f) ||
            !close_enough(simulator.volume, 0.2491f, 0.0000001f)) {
            return 1 + mode;
        }

        /* Skip the package's startup fade to isolate the recovered filters. */
        simulator.mode_gain = 1.0f;
        if (!gp100_ac_sim_process(
                &simulator, 50u, 50u, 50u, mode, audio, 2u, true) ||
            !close_enough(audio[0], expected_first[mode], 0.000005f) ||
            !close_enough(audio[2], expected_second[mode], 0.00001f) ||
            audio[0] != audio[1] || audio[2] != audio[3]) {
            return 10 + mode;
        }
    }
    return 0;
}

static int test_bypass_slew_and_mode_transition(void)
{
    gp100_ac_sim_t simulator;
    gp100_ac_sim_filter_state_t saved_filters[
        GP100_AC_SIM_FILTER_COUNT];
    float audio[8] = {0.25f, -0.5f};

    if (!gp100_ac_sim_reset(&simulator, 0u, 0u, 0u, 0u)) {
        return 1;
    }
    memcpy(saved_filters, simulator.filter, sizeof(saved_filters));
    if (!gp100_ac_sim_process(
            &simulator, 99u, 99u, 99u, 3u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        !close_enough(simulator.body, 0.05f, 0.0000001f) ||
        simulator.top != 0.0f || simulator.volume != 0.0f ||
        simulator.mode != 0u ||
        memcmp(saved_filters, simulator.filter, sizeof(saved_filters)) != 0) {
        return 2;
    }

    simulator.mode_fade_out_gain = 0.001f;
    if (!gp100_ac_sim_process(
            &simulator, 99u, 99u, 99u, 3u, audio, 4u, true) ||
        simulator.mode != 3u || simulator.fading_out ||
        simulator.mode_fade_out_gain != 1.0f ||
        !close_enough(simulator.mode_gain, 0.0005f, 0.0000001f) ||
        !close_enough(simulator.body, 0.0504f, 0.0000001f) ||
        !close_enough(simulator.top, 0.0004f, 0.0000001f) ||
        !close_enough(simulator.volume, 0.0004f, 0.0000001f)) {
        return 3;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_ac_sim_t simulator;
    float audio[2] = {0.0f, 0.0f};

    if (gp100_ac_sim_reset(NULL, 0u, 0u, 0u, 0u) ||
        gp100_ac_sim_reset(&simulator, 100u, 0u, 0u, 0u) ||
        gp100_ac_sim_reset(&simulator, 0u, 100u, 0u, 0u) ||
        gp100_ac_sim_reset(&simulator, 0u, 0u, 100u, 0u) ||
        gp100_ac_sim_reset(&simulator, 0u, 0u, 0u, 4u) ||
        !gp100_ac_sim_reset(&simulator, 0u, 0u, 0u, 0u) ||
        gp100_ac_sim_process(
            &simulator, 0u, 0u, 0u, 4u, audio, 1u, true) ||
        gp100_ac_sim_process(
            &simulator, 0u, 0u, 0u, 0u, NULL, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_parameter_mapping_and_modes();

    if (result != 0) {
        fprintf(stderr, "AC Sim mode test failed: %d\n", result);
        return 10 + result;
    }
    result = test_bypass_slew_and_mode_transition();
    if (result != 0) {
        fprintf(stderr, "AC Sim transition test failed: %d\n", result);
        return 40 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "AC Sim validation test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
