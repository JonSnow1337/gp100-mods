#include "penesas.h"

#include <math.h>
#include <string.h>

typedef gp100_penesas_filter_coefficients_t penesas_coefficients_t;
typedef gp100_penesas_fourth_order_coefficients_t
    penesas_fourth_order_coefficients_t;

static const penesas_coefficients_t input_coefficients[5] = {
    {-3.47923803f, 7.17544365f, -3.64915538f,
     -1.48359048f, 0.539509714f},
    {0.242538407f, -0.271863103f, 0.0301686991f,
     -1.7108649f, 0.729342282f},
    {0.000231900005f, 0.00000580000005f, 0.00000540000019f,
     -0.287641495f, 0.0843985006f},
    {-0.284092903f, 0.0f, 0.284092903f,
     -1.93137705f, 0.931409776f},
    {0.367800117f, 0.419172496f, 0.0943225399f,
     0.419172496f, -0.537877381f}
};

static const penesas_coefficients_t output_coefficients[11] = {
    {1.0f, 0.0f, 0.0f, -1.29742396f, 0.307727486f},
    {0.223937899f, -0.262134105f, 0.0881839991f,
     -1.64531505f, 0.69432199f},
    {0.354673594f, -0.704920471f, 0.350293398f,
     -1.98608661f, 0.986132085f},
    {1.52244437f, -2.04002881f, 0.521518886f,
     -1.03873324f, 0.223444894f},
    {1.18197155f, -2.04002881f, 0.904951274f,
     0.323902011f, 0.132221296f},
    {2.04002881f, 0.186564296f, 0.0160195008f,
     0.00404080003f, 0.000311300013f},
    {2.04002881f, -0.187154606f, 0.498952687f,
     1.06052256f, 0.403089613f},
    {2.04002881f, 1.67229986f, 0.745224416f,
     0.847471416f, 0.257180095f},
    {1.20418739f, 0.878133476f, 0.248709604f,
     1.66720557f, 0.711475015f},
    {0.564198613f, 0.754133224f, 0.224507198f,
     1.65458202f, 0.694450617f},
    {1.21282864f, 2.04002881f, 0.877908289f, 0.0f, 0.0f}
};

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f, 0.0015f, 0.0032f,
    0.0048f, 0.0053f, 0.0034f, -0.0016f, -0.0092f,
    -0.0175f, -0.0233f, -0.0227f, -0.0123f, 0.0097f,
    0.0421f, 0.0807f, 0.1193f, 0.1504f, 0.1678f,
    0.1678f, 0.1504f, 0.1193f, 0.0807f, 0.0421f,
    0.0097f, -0.0123f, -0.0227f, -0.0233f, -0.0175f,
    -0.0092f, -0.0016f, 0.0034f, 0.0053f, 0.0048f,
    0.0032f, 0.0015f, 0.0001f, -0.0007f, -0.0011f
};

static bool parameters_valid(uint8_t gain, uint8_t tone, uint8_t volume)
{
    return gain <= 99u && tone <= 99u && volume <= 99u;
}

/* Factory sqrt taper: two leading zeros, 251 active intervals, 4 decimals. */
static float control_curve(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    float value;

    if (index <= 2u) {
        return 0.0f;
    }
    if (index >= 253u) {
        return 1.0f;
    }
    value = sqrtf((float)(index - 2u) / 251.0f);
    return floorf(value * 10000.0f + 0.5f) * 0.0001f;
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

static float diode_shape(float input, float drive, float level)
{
    float magnitude = (1.0f - expf(-fabsf(input) * drive)) * level;
    return input < 0.0f ? -magnitude : magnitude;
}

static float filter(gp100_penesas_filter_t *state,
                    const penesas_coefficients_t *coefficients, float input)
{
    float output = input * coefficients->b0 + state->x1 * coefficients->b1 +
                   state->x2 * coefficients->b2 -
                   state->y1 * coefficients->a1 -
                   state->y2 * coefficients->a2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static penesas_fourth_order_coefficients_t first_feedback_coefficients(
    float gain)
{
    double x = (double)gain;
    /* The factory rounds the square to float before promoting it. */
    double x2 = (double)(gain * gain);
    penesas_fourth_order_coefficients_t c;
    c.b[0] = x2 - x * 1.2941436072342;
    c.b[1] = x2 * -3.8255060798828 + x * 4.4361596608242;
    c.b[2] = x2 * 5.4834204392316 - x * 5.5791140742074;
    c.b[3] = x2 * -3.490248686379 + x * 3.0259102007628;
    c.b[4] = x2 * 0.83233432703019 - x * 0.58881218014546;
    c.a[0] = x2 - x * 1.0185962618263 - 0.0028330520586833;
    c.a[1] = x2 * -3.8255060798828 + x * 3.8994249043264 +
             0.0052191636511448;
    c.a[2] = x2 * 5.4834204392316 - x * 5.5932990503974 +
             0.00043414048090959;
    c.a[3] = x2 * -3.490248686379 + x * 3.5626450411069 -
             0.0052192617513147;
    c.a[4] = x2 * 0.83233432703019 - x * 0.85017446551703 +
             0.0023988134776038;
    return c;
}

static penesas_fourth_order_coefficients_t second_feedback_coefficients(
    float gain)
{
    float complement = 1.0f - gain;
    double x = (double)complement;
    /* As above, this multiplication occurs before the double conversion. */
    double x2 = (double)(complement * complement);
    penesas_fourth_order_coefficients_t c;
    c.b[0] = x * -0.006103454221507;
    c.b[1] = x * 0.022762342499569;
    c.b[2] = x * -0.031814603379203;
    c.b[3] = x * 0.019750761586377;
    c.b[4] = x * -0.0045951044896854;
    c.a[0] = x2 - x * 1.0061034542215 - 0.076779299966849;
    c.a[1] = x2 * -3.7629302546415 + x * 3.7856925971411 +
             0.28461113459314;
    c.a[2] = x2 * 5.3059147448197 - x * 5.3377293481989 -
             0.39536438206669;
    c.a[3] = x2 * -3.3225899932387 + x * 3.3423407548251 +
             0.24392683467334;
    c.a[4] = x2 * 0.77960937002377 - x * 0.78420447451346 -
             0.056395346974241;
    return c;
}

static void update_feedback_coefficients(gp100_penesas_t *effect)
{
    effect->feedback_coefficients[0] =
        first_feedback_coefficients(effect->gain);
    effect->feedback_coefficients[1] =
        second_feedback_coefficients(effect->gain);
}

static float fourth_order(gp100_penesas_fourth_order_t *state,
                          const penesas_fourth_order_coefficients_t *c,
                          double input)
{
    double output = (input * c->b[0] + state->x[0] * c->b[1] +
                     state->x[1] * c->b[2] + state->x[2] * c->b[3] +
                     state->x[3] * c->b[4] - state->y[0] * c->a[1] -
                     state->y[1] * c->a[2] - state->y[2] * c->a[3] -
                     state->y[3] * c->a[4]) / c->a[0];
    state->x[3] = state->x[2]; state->x[2] = state->x[1];
    state->x[1] = state->x[0]; state->x[0] = input;
    state->y[3] = state->y[2]; state->y[2] = state->y[1];
    state->y[1] = state->y[0]; state->y[0] = output;
    return (float)output;
}

static penesas_coefficients_t tone_coefficients(float tone, float volume)
{
    float tone_squared = tone * tone;
    float denominator = tone_squared * 0.09862196f + tone * 0.916198f -
                        1.265742f;
    float common = volume * tone - volume * tone_squared * 0.09337068f;
    penesas_coefficients_t coefficients;

    coefficients.b0 = (common + volume * 0.5354135f) / denominator;
    coefficients.b1 = (volume * tone_squared * 0.1867414f -
                       volume * tone * 2.0f - volume * 0.9812512f) /
                      denominator;
    coefficients.b2 = (common + volume * 0.4458376f) / denominator;
    coefficients.a1 = (tone_squared * -0.1971989f - tone * 1.831978f +
                       2.436292f) / denominator;
    coefficients.a2 = (tone_squared * 0.09857692f + tone * 0.9157795f -
                       1.170593f) / denominator;
    return coefficients;
}

static float oversampled_stage(gp100_penesas_t *effect,
                               size_t stage,
                               float input,
                               float drive,
                               float level)
{
    float shaped[4];
    float step = (input - effect->previous_oversample_input[stage]) / 3.0f;
    float output = 0.0f;
    size_t index;

    for (index = 0u; index < 4u; ++index) {
        shaped[index] = diode_shape(
            effect->previous_oversample_input[stage] + step * (float)index,
            drive, level);
    }
    memmove(&effect->oversample_history[stage][4],
            &effect->oversample_history[stage][0], 36u * sizeof(float));
    effect->oversample_history[stage][0] = shaped[3];
    effect->oversample_history[stage][1] = shaped[2];
    effect->oversample_history[stage][2] = shaped[1];
    effect->oversample_history[stage][3] = shaped[0];
    for (index = 0u; index < 40u; ++index) {
        output += reconstruction_fir[index] *
                  effect->oversample_history[stage][index];
    }
    effect->previous_oversample_input[stage] = input;
    return output;
}

static float process_sample(gp100_penesas_t *effect, float input)
{
    float scaled = input * 1.3f;
    double first_feedback_input;
    float feedback_input;
    float first;
    float second;
    float selected;
    size_t section;

    /* The factory performs this feedback sum and first IIR in double. */
    first_feedback_input =
        (double)filter(
            &effect->input_filter[0], &input_coefficients[0], scaled) +
        effect->feedback_return;
    first = (float)first_feedback_input;
    second = filter(&effect->input_filter[1], &input_coefficients[1], first);
    second = filter(&effect->input_filter[2], &input_coefficients[2], second);
    first = fourth_order(&effect->feedback_filter[0],
                         &effect->feedback_coefficients[0],
                         first_feedback_input);
    first = oversampled_stage(effect, 0u, first, 2.2f, 0.9f);
    first = filter(&effect->input_filter[3], &input_coefficients[3], first);
    feedback_input = first + second;
    effect->feedback_return = fourth_order(
        &effect->feedback_filter[1],
        &effect->feedback_coefficients[1], feedback_input);
    second = filter(
        &effect->input_filter[4], &input_coefficients[4], scaled);
    second = oversampled_stage(effect, 1u, second, 1.0f, 1.1f);
    if (effect->gain < 0.4f) {
        selected = second * 2.0f;
    } else if (effect->gain < 0.7f) {
        float blend = (effect->gain - 0.4f) / 0.3f;
        selected = first * blend + second * 2.0f * (1.0f - blend);
    } else {
        selected = first;
    }
    selected = filter(
        &effect->tone_filter, &effect->tone_filter_coefficients, selected);
    for (section = 0u; section < 11u; ++section)
        selected = filter(&effect->output_filter[section],
                          &output_coefficients[section], selected);
    return selected * 0.24f;
}

bool gp100_penesas_reset(gp100_penesas_t *effect,
                              uint8_t gain,
                              uint8_t tone,
                              uint8_t volume)
{
    if (effect == NULL || !parameters_valid(gain, tone, volume)) {
        return false;
    }
    memset(effect, 0, sizeof(*effect));
    effect->gain = control_curve(gain);
    effect->gain_target = effect->gain;
    effect->tone = control_curve(tone) * 0.99f + 0.01f;
    effect->output_level = (float)volume * 0.01f;
    update_feedback_coefficients(effect);
    effect->tone_filter_coefficients =
        tone_coefficients(effect->tone, effect->output_level);
    effect->configured = true;
    return true;
}

bool gp100_penesas_process(gp100_penesas_t *effect,
                                uint8_t gain,
                                uint8_t tone,
                                uint8_t volume,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled)
{
    float tone_target;
    float output_level_target;
    size_t frame;

    if (effect == NULL || !effect->configured ||
        !parameters_valid(gain, tone, volume) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    effect->gain_target = control_curve(gain);
    tone_target = control_curve(tone) * 0.99f + 0.01f;
    output_level_target = (float)volume * 0.01f;
    if (effect->tone != tone_target ||
        effect->output_level != output_level_target) {
        effect->tone = tone_target;
        effect->output_level = output_level_target;
        effect->tone_filter_coefficients =
            tone_coefficients(effect->tone, effect->output_level);
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        float output;
        float next_gain;

        if (!enabled) {
            continue;
        }
        next_gain = move_toward(effect->gain, effect->gain_target);
        if (next_gain != effect->gain) {
            effect->gain = next_gain;
            update_feedback_coefficients(effect);
        }
        output = process_sample(effect, interleaved_stereo[frame * 2u]);
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
