#include "audio/effects/dynamics/noise_gate.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static int test_gate1_coefficients_and_envelope(void)
{
    gp100_noise_gate_t gate;
    const float parameters[1] = {20.0f};
    float audio[6] = {1.0f, -0.5f, 1.0f, 0.25f, 1.0f, 0.75f};
    const float detector[6] = {0.0f, 0.001f, 0.0f, 0.0f, 0.0f, 0.0f};
    float opened;

    if (!gp100_noise_gate_reset(
            &gate, GP100_NOISE_GATE_1, parameters) ||
        !close_enough(gate.threshold_linear, 0.0001f, 0.0000001f) ||
        !close_enough(gate.attack_seconds, 0.003f, 0.0000001f) ||
        !close_enough(gate.release_seconds, 0.1f, 0.0000001f) ||
        !gp100_noise_gate_process(
            &gate, parameters, audio, detector, 1u, true)) {
        return 1;
    }
    opened = 1.0f - 0.9835000038146973f;
    if (!close_enough(audio[0], opened, 0.0000001f) ||
        !close_enough(audio[1], opened, 0.0000001f)) {
        return 2;
    }

    if (!gp100_noise_gate_process(
            &gate, parameters, &audio[2], &detector[2], 1u, true) ||
        !close_enough(audio[2], opened, 0.0000001f) ||
        !gp100_noise_gate_process(
            &gate, parameters, &audio[4], &detector[4], 1u, true) ||
        !close_enough(audio[4], opened * 0.9994999766349792f,
                      0.0000001f)) {
        return 3;
    }
    return 0;
}

static int test_gate2_parameter_mapping_and_live_update(void)
{
    gp100_noise_gate_t gate;
    float parameters[3] = {20.0f, 25.0f, 60.0f};
    float audio[4] = {0.5f, -0.5f, -0.25f, 0.25f};
    const float detector[4] = {0.0f, -0.001f, 0.0f, 0.001f};
    float expected_attack_seconds = 0.25f * 0.019f + 0.001f;
    float expected_release_seconds = 0.60f * 0.19f + 0.01f;
    float expected_coefficient =
        expf(-2.1972f / (expected_attack_seconds * 44100.0f));

    if (!gp100_noise_gate_reset(
            &gate, GP100_NOISE_GATE_2, parameters) ||
        !close_enough(gate.attack_seconds, expected_attack_seconds,
                      0.0000001f) ||
        !close_enough(gate.release_seconds, expected_release_seconds,
                      0.0000001f) ||
        !close_enough(gate.attack_coefficient, expected_coefficient,
                      0.0000001f) ||
        !gp100_noise_gate_process(
            &gate, parameters, audio, detector, 2u, true)) {
        return 1;
    }
    if (!(audio[0] > 0.0f && audio[0] < 0.5f) ||
        !close_enough(audio[0], audio[1], 0.0000001f) ||
        !(audio[2] < 0.0f) ||
        !close_enough(audio[2], audio[3], 0.0000001f)) {
        return 2;
    }

    parameters[1] = 99.0f;
    if (!gp100_noise_gate_process(
            &gate, parameters, audio, detector, 1u, true) ||
        !close_enough(gate.attack_seconds, 0.01981f, 0.0000001f)) {
        return 3;
    }
    return 0;
}

static int test_bypass_and_validation(void)
{
    gp100_noise_gate_t gate;
    const float valid[3] = {20.0f, 25.0f, 60.0f};
    const float invalid[3] = {20.0f, 100.0f, 60.0f};
    float audio[2] = {0.25f, -0.5f};
    const float detector[2] = {0.0f, 1.0f};

    if (!gp100_noise_gate_reset(
            &gate, GP100_NOISE_GATE_2, valid) ||
        !gp100_noise_gate_process(
            &gate, valid, audio, detector, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f || gate.gain != 0.0f ||
        gp100_noise_gate_process(
            &gate, invalid, audio, detector, 1u, true) ||
        gp100_noise_gate_reset(NULL, GP100_NOISE_GATE_1, valid) ||
        gp100_noise_gate_reset(
            &gate, (gp100_noise_gate_type_t)2, valid) ||
        gp100_noise_gate_process(
            &gate, valid, NULL, detector, 1u, true)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_gate1_coefficients_and_envelope();

    if (result != 0) {
        fprintf(stderr, "Gate 1 test failed: %d\n", result);
        return 10 + result;
    }
    result = test_gate2_parameter_mapping_and_live_update();
    if (result != 0) {
        fprintf(stderr, "Gate 2 test failed: %d\n", result);
        return 20 + result;
    }
    result = test_bypass_and_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
