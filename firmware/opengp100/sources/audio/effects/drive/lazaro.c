#include "lazaro.h"

#include "audio/dsp/decimation_internal.h"
#include "lazaro_nonlinearity.h"

#include <math.h>
#include <string.h>

typedef struct lazaro_coefficients { float b0, b1, b2, a1, a2; }
    lazaro_coefficients_t;

static const float reachable_gain[25] = {
    0.017680f, 0.017805f, 0.018191f, 0.018849f, 0.019793f,
    0.021038f, 0.022599f, 0.024491f, 0.026732f, 0.029339f,
    0.032329f, 0.035722f, 0.039538f, 0.043796f, 0.048519f,
    0.053727f, 0.059444f, 0.065692f, 0.072497f, 0.079882f,
    0.087874f, 0.096499f, 0.105784f, 0.115756f, 0.126446f
};

static const lazaro_coefficients_t input_coefficients = {
    -0.54369998f, 0.0f, 0.54369998f, -1.86689997f, 0.867200017f
};

static const lazaro_coefficients_t nonlinear_filter_coefficients = {
    -1.8678f, 0.0f, 1.8678f, -1.71469998f, 0.714900017f
};

static float filter(gp100_lazaro_filter_t *state,
                    lazaro_coefficients_t c, float input)
{
    float output = input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                   state->y1 * c.a1 - state->y2 * c.a2;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static float blend_amount(uint8_t raw)
{
    float gain = (float)raw * 0.01f;
    float reciprocal = 1.0f / (gain * 10.0f + 1.0f);
    return 0.34377f - gain * 0.54302f + gain * gain * 0.186192f +
           reciprocal * 0.092401f + reciprocal * reciprocal * 0.56383f;
}

static float clipped_sample(gp100_lazaro_t *effect, float input,
                            unsigned stage, float blend)
{
    float step = (input - effect->previous_clip_input[stage]) / 3.0f;
    float samples[4];
    size_t i;
    for (i = 0u; i < 4u; ++i)
        samples[i] = lazaro_blended_nonlinearity(
            effect->previous_clip_input[stage] + step * (float)i, blend);
    effect->previous_clip_input[stage] = input;
    return gp100_decimate_four(effect->oversample_history[stage], samples);
}

static lazaro_coefficients_t tone_coefficients(uint8_t raw)
{
    float index = (float)(raw >> 2);
    return (lazaro_coefficients_t){
        0.065738f + index * 0.012649f,
        -0.002852f - index * 0.025645f,
        -0.036173f + index * 0.012649f,
        -1.60498905f, 0.63677901f};
}

bool gp100_lazaro_reset(gp100_lazaro_t *effect,
                             uint8_t gain, uint8_t tone, uint8_t volume)
{
    if (effect == NULL || gain > 99u || tone > 99u || volume > 99u)
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->volume_target = (float)volume * 0.01f;
    effect->configured = true;
    return true;
}

bool gp100_lazaro_process(gp100_lazaro_t *effect,
                               uint8_t gain, uint8_t tone, uint8_t volume,
                               float *audio, size_t frames, bool enabled)
{
    float drive;
    float blend;
    lazaro_coefficients_t tone_filter;
    lazaro_coefficients_t gain_filter;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || tone > 99u ||
        volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u) return false;
    drive = reachable_gain[gain >> 2];
    blend = blend_amount(gain);
    tone_filter = tone_coefficients(tone);
    gain_filter = (lazaro_coefficients_t){
        drive, -drive, 0.0f, -0.999777257f, 0.0f};
    effect->volume_target = (float)volume * 0.01f;
    if (effect->volume < effect->volume_target)
        effect->volume = fminf(effect->volume + 0.005f, effect->volume_target);
    else if (effect->volume > effect->volume_target)
        effect->volume = fmaxf(effect->volume - 0.005f, effect->volume_target);
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        output = filter(&effect->input_filter, input_coefficients,
                        audio[n * 2u]);
        output = filter(&effect->gain_filter, gain_filter, output);
        output = filter(&effect->nonlinear_filter[0],
                        nonlinear_filter_coefficients, output);
        output = clipped_sample(effect, output, 0u, blend);
        output = filter(&effect->nonlinear_filter[1],
                        nonlinear_filter_coefficients, output);
        output = clipped_sample(effect, output, 1u, blend);
        output = filter(&effect->tone_filter, tone_filter, output);
        output *= effect->volume;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
