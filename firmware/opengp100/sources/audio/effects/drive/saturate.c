#include "saturate.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct saturate_coefficients {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} saturate_coefficients_t;

/* Factory level taper sampled at the 100 integer UI positions, scaled x10000. */
static const uint16_t level_curve[100] = {
    0, 0, 0, 0, 0, 0, 1, 2, 3, 5,
    7, 11, 13, 18, 22, 29, 34, 43, 50, 61,
    74, 83, 99, 111, 129, 143, 165, 181, 207, 226,
    256, 288, 311, 348, 374, 416, 445, 492, 525, 577,
    632, 671, 732, 774, 841, 888, 961, 1012, 1092, 1148,
    1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274,
    3447, 3626, 3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983,
    5212, 5368, 5609, 5773, 6025, 6197, 6461, 6642, 6918, 7107,
    7397, 7694, 7896, 8207, 8418, 8742, 8962, 9299, 9529, 9881
};

/* Factory H-Cut taper sampled at integer UI positions, scaled x100000. */
static const uint32_t high_cut_curve[100] = {
    0, 0, 26499, 30888, 35565, 38028, 41142, 42946, 45361, 46817,
    48822, 50651, 51790, 53396, 54407, 55846, 56759, 58067, 58903, 60107,
    61257, 61997, 63069, 63761, 64767, 65418, 66367, 66982, 67882, 68466,
    69321, 70153, 70695, 71489, 72008, 72770, 73268, 74000, 74479, 75185,
    75875, 76327, 76994, 77431, 78076, 78499, 79124, 79534, 80141, 80539,
    81128, 81707, 82088, 82652, 83023, 83572, 83934, 84469, 84822, 85345,
    85860, 86200, 86703, 87035, 87528, 87853, 88334, 88652, 89124, 89436,
    89898, 90355, 90657, 91105, 91401, 91841, 92131, 92563, 92848, 93273,
    93692, 93970, 94382, 94655, 95060, 95328, 95727, 95991, 96384, 96643,
    97030, 97413, 97666, 98043, 98293, 98665, 98911, 99277, 99519, 99880
};

static const saturate_coefficients_t input_coefficients = {
    -1.89960003f, 2.94190001f, -1.0316f, -1.09889996f, 0.108400002f
};

static const saturate_coefficients_t reconstruction_coefficients = {
    -0.514599979f, 0.576499999f, -0.057f, -1.52460003f, 0.530099988f
};

static bool parameters_valid(uint8_t gain,
                             uint8_t mix,
                             uint8_t output,
                             uint8_t high_cut)
{
    return gain <= 99u && mix <= 99u && output <= 99u && high_cut <= 99u;
}

static float level_value(uint8_t raw)
{
    return (float)level_curve[raw] * 0.0001f;
}

static float high_cut_value(uint8_t raw)
{
    return 1.0f - (float)high_cut_curve[raw] * 0.00001f;
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

static float filter_sample(gp100_saturate_filter_t *state,
                           saturate_coefficients_t coefficients,
                           float input)
{
    float output = input * coefficients.b0 + state->x1 * coefficients.b1 +
                   state->x2 * coefficients.b2 - state->y1 * coefficients.a1 -
                   state->y2 * coefficients.a2;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static saturate_coefficients_t make_high_cut(float control)
{
    float cutoff = control * 8000.0f + 2000.0f;
    float k = tanf(cutoff * 3.1415927f / 44100.0f);
    float k2 = k * k;
    float scale = 1.0f / (k + k2 * 0.707f + 0.707f);
    saturate_coefficients_t coefficients;

    coefficients.b0 = k2 * 0.707f * scale;
    coefficients.b1 = 2.0f * k2 * 0.707f * scale;
    coefficients.b2 = coefficients.b0;
    coefficients.a1 = (k2 - 1.0f) * 1.414f * scale;
    coefficients.a2 = ((k2 * 0.707f - k) + 0.707f) * scale;
    return coefficients;
}

static float shape_sample(float input, float gain)
{
    float drive = gain * 20.0f + 1.0f;
    float value = 1.0f - drive * fabsf(input) * (1.0f / 1024.0f);
    size_t square;

    for (square = 0u; square < 10u; ++square) {
        value *= value;
    }
    return input > 0.0f ? 1.0f - value : value - 1.0f;
}

static float oversampled_shape(gp100_saturate_t *saturate, float input)
{
    float shaped[4];
    float step = (input - saturate->previous_oversample_input) / 3.0f;
    size_t index;

    for (index = 0u; index < 4u; ++index) {
        float sample = saturate->previous_oversample_input + step * (float)index;
        shaped[index] = shape_sample(sample, saturate->gain);
    }
    saturate->previous_oversample_input = input;
    return gp100_decimate_four(saturate->shaped_history, shaped);
}

static void set_targets(gp100_saturate_t *saturate,
                        uint8_t gain,
                        uint8_t mix,
                        uint8_t output,
                        uint8_t high_cut)
{
    saturate->gain_target = level_value(gain);
    saturate->mix_target = level_value(mix);
    saturate->output_target = (float)output * 0.01f;
    saturate->high_cut_target = high_cut_value(high_cut);
}

bool gp100_saturate_reset(gp100_saturate_t *saturate,
                               uint8_t gain,
                               uint8_t mix,
                               uint8_t output,
                               uint8_t high_cut)
{
    if (saturate == NULL ||
        !parameters_valid(gain, mix, output, high_cut)) {
        return false;
    }
    memset(saturate, 0, sizeof(*saturate));
    set_targets(saturate, gain, mix, output, high_cut);
    saturate->gain = saturate->gain_target;
    saturate->mix = saturate->mix_target;
    saturate->output_level = saturate->output_target;
    saturate->high_cut_control = saturate->high_cut_target;
    saturate->configured = true;
    return true;
}

bool gp100_saturate_process(gp100_saturate_t *saturate,
                                 uint8_t gain,
                                 uint8_t mix,
                                 uint8_t output,
                                 uint8_t high_cut,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled)
{
    saturate_coefficients_t high_cut_coefficients;
    size_t frame;

    if (saturate == NULL || !saturate->configured ||
        !parameters_valid(gain, mix, output, high_cut) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_targets(saturate, gain, mix, output, high_cut);
    high_cut_coefficients = make_high_cut(saturate->high_cut_target);
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float processed;
        float output_sample;

        saturate->gain = move_toward(saturate->gain, saturate->gain_target);
        saturate->mix = move_toward(saturate->mix, saturate->mix_target);
        saturate->output_level = move_toward(
            saturate->output_level, saturate->output_target);
        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        if (saturate->high_cut_control != saturate->high_cut_target) {
            saturate->high_cut_control = saturate->high_cut_target;
        }
        processed = filter_sample(
            &saturate->input_filter, input_coefficients, input);
        processed = oversampled_shape(saturate, processed);
        processed = filter_sample(&saturate->reconstruction_filter,
                                  reconstruction_coefficients, processed);
        processed = filter_sample(&saturate->high_cut_filter,
                                  high_cut_coefficients, processed);
        output_sample = ((1.0f - saturate->mix) * input +
                         saturate->mix * processed) *
                        saturate->output_level * 2.0f;
        interleaved_stereo[frame * 2u] = output_sample;
        interleaved_stereo[frame * 2u + 1u] = output_sample;
    }
    return true;
}
