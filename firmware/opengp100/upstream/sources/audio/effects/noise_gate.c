#include "noise_gate.h"

#include <math.h>
#include <stdint.h>

#define GP100_GATE_THRESHOLD_SCALE 0.01f
#define GP100_GATE_DB_SPAN -100.0f
#define GP100_GATE_DECAY_E_FOLDS -2.1972f

static bool parameter_is_valid(float value)
{
    return value >= 0.0f && value <= 99.0f;
}

static float threshold_to_linear(float parameter)
{
    float normalized = parameter * GP100_GATE_THRESHOLD_SCALE;
    float threshold_db = (1.0f - normalized) * GP100_GATE_DB_SPAN;

    return powf(10.0f, threshold_db / 20.0f);
}

static float smoothing_coefficient(float seconds)
{
    return expf(GP100_GATE_DECAY_E_FOLDS /
                (seconds * GP100_NOISE_GATE_SAMPLE_RATE));
}

static bool configure(gp100_noise_gate_t *gate,
                      const float *parameters)
{
    float threshold;

    if (gate == NULL || parameters == NULL ||
        !parameter_is_valid(parameters[0])) {
        return false;
    }
    if (gate->type == GP100_NOISE_GATE_2 &&
        (!parameter_is_valid(parameters[1]) ||
         !parameter_is_valid(parameters[2]))) {
        return false;
    }

    threshold = parameters[0];
    if (!gate->configured || gate->threshold_parameter != threshold) {
        gate->threshold_parameter = threshold;
        gate->threshold_linear = threshold_to_linear(threshold);
    }

    if (gate->type == GP100_NOISE_GATE_1) {
        gate->attack_parameter = 0.0f;
        gate->release_parameter = 0.0f;
        gate->attack_seconds = 0.003f;
        gate->release_seconds = 0.1f;
        /* The shipped Gate 1 stores these rounded constants directly. */
        gate->attack_coefficient = 0.9835000038146973f;
        gate->release_coefficient = 0.9994999766349792f;
    } else {
        if (!gate->configured || gate->attack_parameter != parameters[1]) {
            gate->attack_parameter = parameters[1];
            gate->attack_seconds = parameters[1] * 0.01f * 0.019f + 0.001f;
            gate->attack_coefficient =
                smoothing_coefficient(gate->attack_seconds);
        }
        if (!gate->configured || gate->release_parameter != parameters[2]) {
            gate->release_parameter = parameters[2];
            gate->release_seconds = parameters[2] * 0.01f * 0.19f + 0.01f;
            gate->release_coefficient =
                smoothing_coefficient(gate->release_seconds);
        }
    }
    gate->configured = true;
    return true;
}

bool gp100_noise_gate_reset(
    gp100_noise_gate_t *gate,
    gp100_noise_gate_type_t type,
    const float *parameters)
{
    if (gate == NULL ||
        (type != GP100_NOISE_GATE_1 &&
         type != GP100_NOISE_GATE_2)) {
        return false;
    }

    gate->type = type;
    gate->threshold_parameter = 0.0f;
    gate->attack_parameter = 0.0f;
    gate->release_parameter = 0.0f;
    gate->threshold_linear = 0.0f;
    gate->attack_seconds = 0.0f;
    gate->release_seconds = 0.0f;
    gate->attack_coefficient = 0.0f;
    gate->release_coefficient = 0.0f;
    gate->gain = 0.0f;
    gate->hold_counter = 0.0f;
    gate->configured = false;
    return configure(gate, parameters);
}

bool gp100_noise_gate_process(
    gp100_noise_gate_t *gate,
    const float *parameters,
    float *interleaved_stereo,
    const float *detector_interleaved,
    size_t frame_count,
    bool enabled)
{
    size_t frame;

    if (gate == NULL || parameters == NULL || !gate->configured ||
        (gate->type != GP100_NOISE_GATE_1 &&
         gate->type != GP100_NOISE_GATE_2) ||
        (frame_count != 0u &&
         (interleaved_stereo == NULL || detector_interleaved == NULL)) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    if (!enabled || frame_count == 0u) {
        return true;
    }
    if (!configure(gate, parameters)) {
        return false;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;
        float input = interleaved_stereo[sample];
        float detector = detector_interleaved[sample + 1u];
        float target;

        if (detector < 0.0f) {
            detector = -detector;
        }
        target = detector < gate->threshold_linear ? 0.0f : 1.0f;

        if (target > gate->gain) {
            gate->gain = gate->attack_coefficient * gate->gain +
                         (1.0f - gate->attack_coefficient) * target;
            gate->hold_counter = 0.0f;
        } else if (gate->hold_counter <= 0.0f) {
            /* Both shipped gates retain the current gain for one sample. */
            gate->hold_counter += 1.0f;
        } else if (target <= gate->gain) {
            gate->gain = gate->release_coefficient * gate->gain +
                         (1.0f - gate->release_coefficient) * target;
        }

        input *= gate->gain;
        interleaved_stereo[sample] = input;
        interleaved_stereo[sample + 1u] = input;
    }
    return true;
}
