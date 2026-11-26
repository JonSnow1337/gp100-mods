#include "ring_modulator.h"

#include <math.h>
#include <string.h>

#define RING_MOD_SAMPLE_RATE 44100.0f
#define RING_MOD_TWO_PI 6.2831855f

static bool parameters_valid(uint8_t mix,
                             uint8_t frequency,
                             int8_t fine,
                             uint8_t tone)
{
    return mix <= 99u && frequency <= 99u && fine >= -50 && fine <= 50 &&
           tone <= 99u;
}

/* The shipped table is a four-decimal, clamped ramp with two leading zeros. */
static float control_curve_from_index(unsigned index)
{
    float value;

    if (index <= 2u) {
        return 0.0f;
    }
    if (index >= 253u) {
        return 1.0f;
    }
    value = (float)(index - 2u) / 251.0f;
    return floorf(value * 10000.0f) * 0.0001f;
}

static float raw_control_curve(uint8_t raw)
{
    return control_curve_from_index((unsigned)((float)raw * 2.55f));
}

static float fine_control_curve(int8_t fine)
{
    return control_curve_from_index(
        (unsigned)(((float)fine + 50.0f) * 2.55f));
}

static float tone_curve(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    float value;

    if (index <= 2u) {
        return 0.1f;
    }
    if (index >= 253u) {
        return 1.0f;
    }
    value = 0.1f + 0.9f * ((float)(index - 2u) / 251.0f);
    return floorf(value * 10000.0f) * 0.0001f;
}

static float normalized_frequency(uint8_t frequency, int8_t fine)
{
    float coarse = raw_control_curve(frequency);
    float fine_value = fine_control_curve(fine);
    float hertz = coarse * 2000.0f * coarse * coarse + 100.0f +
                  (fine_value - 0.5f) * 50.0f;

    return hertz / RING_MOD_SAMPLE_RATE;
}

static float move_toward(float current, float target)
{
    if (current < target) {
        current += 0.0001f;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= 0.0001f;
        return current < target ? target : current;
    }
    return current;
}

bool gp100_ring_modulator_reset(gp100_ring_modulator_t *modulator,
                                     uint8_t mix,
                                     uint8_t frequency,
                                     int8_t fine,
                                     uint8_t tone)
{
    if (modulator == NULL ||
        !parameters_valid(mix, frequency, fine, tone)) {
        return false;
    }
    memset(modulator, 0, sizeof(*modulator));
    modulator->mix = raw_control_curve(mix);
    modulator->frequency = frequency;
    modulator->fine = fine;
    modulator->tone = tone;
    modulator->normalized_frequency = normalized_frequency(frequency, fine);
    modulator->configured = true;
    return true;
}

bool gp100_ring_modulator_process(gp100_ring_modulator_t *modulator,
                                       uint8_t mix,
                                       uint8_t frequency,
                                       int8_t fine,
                                       uint8_t tone,
                                       float *interleaved_stereo,
                                       size_t frame_count,
                                       bool enabled)
{
    float mix_target;
    float next_frequency;
    float tone_coefficient;
    size_t frame;

    if (modulator == NULL || !modulator->configured ||
        !parameters_valid(mix, frequency, fine, tone) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    mix_target = raw_control_curve(mix);
    next_frequency = normalized_frequency(frequency, fine);
    tone_coefficient = tone_curve(tone);
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float modulated;
        float filtered;
        float output;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        if (modulator->normalized_frequency != next_frequency) {
            modulator->oscillator_samples *=
                modulator->normalized_frequency / next_frequency;
            modulator->normalized_frequency = next_frequency;
        }
        modulated = input * cosf(
            modulator->normalized_frequency * RING_MOD_TWO_PI *
            modulator->oscillator_samples);
        modulator->oscillator_samples += 1.0f;
        if (modulator->oscillator_samples >
            1.0f / modulator->normalized_frequency) {
            modulator->oscillator_samples -=
                1.0f / modulator->normalized_frequency;
        }
        filtered = modulated * tone_coefficient +
                   modulator->previous_filtered * (1.0f - tone_coefficient);
        modulator->mix = move_toward(modulator->mix, mix_target);
        output = filtered * modulator->mix + input * (1.0f - modulator->mix);
        modulator->previous_filtered = filtered;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    modulator->frequency = frequency;
    modulator->fine = fine;
    modulator->tone = tone;
    return true;
}
