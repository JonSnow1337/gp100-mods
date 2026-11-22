#include "darktale.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct darktale_coefficients {
    float b0, b1, b2, b3;
    float a0, a1, a2, a3;
} darktale_coefficients_t;

/* These are the 100 UI-reachable entries of the two factory 256-entry
 * tables. Tone and Volume deliberately share the same table. */
static const float factory_gain[100] = {
    0.000000f, 0.000000f, 0.002629f, 0.004382f, 0.007012f,
    0.008765f, 0.011394f, 0.013147f, 0.015777f, 0.017530f,
    0.020159f, 0.022789f, 0.024542f, 0.027172f, 0.028925f,
    0.031555f, 0.033308f, 0.035939f, 0.037693f, 0.040324f,
    0.042957f, 0.044712f, 0.047347f, 0.049105f, 0.051744f,
    0.053505f, 0.056150f, 0.057916f, 0.060571f, 0.062344f,
    0.065012f, 0.067689f, 0.069482f, 0.072182f, 0.073992f,
    0.076724f, 0.078558f, 0.081331f, 0.083196f, 0.086024f,
    0.088892f, 0.090830f, 0.093781f, 0.095780f, 0.098836f,
    0.100915f, 0.104103f, 0.106280f, 0.109632f, 0.111931f,
    0.115488f, 0.119187f, 0.121742f, 0.125722f, 0.128483f,
    0.132801f, 0.135809f, 0.140535f, 0.143839f, 0.149050f,
    0.154595f, 0.158494f, 0.164675f, 0.169034f, 0.175962f,
    0.180860f, 0.188663f, 0.194191f, 0.203012f, 0.209270f,
    0.219271f, 0.230065f, 0.237737f, 0.250012f, 0.258740f,
    0.272713f, 0.282653f, 0.298566f, 0.309886f, 0.328008f,
    0.347596f, 0.361526f, 0.383816f, 0.399660f, 0.424999f,
    0.443002f, 0.471776f, 0.492205f, 0.524837f, 0.547988f,
    0.584939f, 0.624723f, 0.652914f, 0.697850f, 0.729665f,
    0.780335f, 0.816180f, 0.873219f, 0.913533f, 0.977629f
};

static const float factory_tone_volume[100] = {
    0.000000f, 0.000000f, 0.001793f, 0.002988f, 0.004781f,
    0.005976f, 0.007769f, 0.008964f, 0.010757f, 0.011952f,
    0.013745f, 0.015538f, 0.016733f, 0.018526f, 0.019722f,
    0.021515f, 0.022711f, 0.024505f, 0.025701f, 0.027496f,
    0.029292f, 0.030490f, 0.032289f, 0.033490f, 0.035293f,
    0.036497f, 0.038307f, 0.039516f, 0.041336f, 0.042553f,
    0.044388f, 0.046233f, 0.047471f, 0.049342f, 0.050599f,
    0.052503f, 0.053786f, 0.055736f, 0.057054f, 0.059062f,
    0.061115f, 0.062512f, 0.064655f, 0.066119f, 0.068376f,
    0.069926f, 0.072327f, 0.073985f, 0.076566f, 0.078356f,
    0.081159f, 0.084118f, 0.086187f, 0.089451f, 0.091745f,
    0.095378f, 0.097941f, 0.102018f, 0.104904f, 0.109510f,
    0.114480f, 0.118014f, 0.123676f, 0.127711f, 0.134189f,
    0.138812f, 0.146242f, 0.151551f, 0.160091f, 0.166196f,
    0.176021f, 0.186712f, 0.194357f, 0.206660f, 0.215458f,
    0.229612f, 0.239728f, 0.255997f, 0.267617f, 0.286293f,
    0.306567f, 0.321032f, 0.344249f, 0.360800f, 0.387341f,
    0.406244f, 0.436528f, 0.458075f, 0.492563f, 0.517076f,
    0.556272f, 0.598553f, 0.628558f, 0.676454f, 0.710410f,
    0.764555f, 0.802901f, 0.863986f, 0.907204f, 0.975979f
};

static float filter(gp100_darktale_filter_t *state,
                    darktale_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 +
                    state->x2 * c.b2 + state->x3 * c.b3 -
                    state->y1 * c.a1 - state->y2 * c.a2 -
                    state->y3 * c.a3) / c.a0;
    state->x3 = state->x2; state->x2 = state->x1; state->x1 = input;
    state->y3 = state->y2; state->y2 = state->y1; state->y1 = output;
    return output;
}

static darktale_coefficients_t gain_coefficients(float gain)
{
    float scaled = gain * 0.9999f;
    float x = fmaxf(scaled, 0.01f);
    float x2 = scaled * scaled;
    return (darktale_coefficients_t){
        (x * 35.80437f + 0.8924363f) * x,
        (1.010285f - x * 30.08755f) * x,
        (x * -35.80437f - 0.6567398f) * x,
        (x * 30.08755f - 0.774588f) * x,
        x2 + x * 0.04869312f + 0.0005630513f,
        -3.0f * x2 - x * 0.0447584f + 0.0006374035f,
        3.0f * x2 - x * 0.04869312f - 0.0004143469f,
        x * 0.0447584f - x2 - 0.0004886991f};
}

static void interpolate(float previous, float input, float samples[4])
{
    float step = (input - previous) / 3.0f;
    size_t i;
    for (i = 0u; i < 4u; ++i) samples[i] = previous + step * (float)i;
}

static float hard_clip_stage(gp100_darktale_t *effect, float input)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[0], input, samples);
    for (i = 0u; i < 4u; ++i)
        samples[i] = fmaxf(-9.0f, fminf(9.0f, samples[i]));
    effect->previous_stage_input[0] = input;
    return gp100_decimate_four(effect->oversample_history[0], samples);
}

static float slew_stage(gp100_darktale_t *effect, float input)
{
    const float limit = 1.1337868f;
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[1], input, samples);
    samples[0] = fmaxf(-limit, fminf(limit, samples[0]));
    for (i = 1u; i < 4u; ++i) {
        float delta = samples[i] - samples[i - 1u];
        samples[i] = samples[i - 1u] +
                     fmaxf(-limit, fminf(limit, delta));
    }
    effect->previous_stage_input[1] = input;
    return gp100_decimate_four(effect->oversample_history[1], samples);
}

static float rational_shape(float input)
{
    float magnitude = fabsf(input);
    float square = magnitude * magnitude;
    float cube = square * magnitude;
    float numerator = cube * 0.9465f - square * 0.6001f + magnitude * 0.3038f;
    float denominator = cube - square * 0.1612f -
                        magnitude * 0.2258f + 0.2663f;
    return copysignf(numerator / denominator, input);
}

static float rational_stage(gp100_darktale_t *effect, float input)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[2], input, samples);
    for (i = 0u; i < 4u; ++i) samples[i] = rational_shape(samples[i]);
    effect->previous_stage_input[2] = input;
    return gp100_decimate_four(effect->oversample_history[2], samples);
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

bool gp100_darktale_reset(gp100_darktale_t *effect,
                               uint8_t gain, uint8_t tone, uint8_t volume)
{
    if (effect == NULL || gain > 99u || tone > 99u || volume > 99u)
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->volume = effect->volume_target = factory_tone_volume[volume];
    effect->configured = true;
    return true;
}

bool gp100_darktale_process(gp100_darktale_t *effect,
                                 uint8_t gain, uint8_t tone, uint8_t volume,
                                 float *audio, size_t frames, bool enabled)
{
    static const darktale_coefficients_t post_slew = {
        1.50010443f, -1.46418679f, -1.49783909f, 1.46192300f,
        2.13378716f, -3.69378042f, 1.45741200f, 0.10642558f};
    static const darktale_coefficients_t reconstruction = {
        1.0f, -2.0f, 1.0f, 0.0f,
        1.0f, -1.99597013f, 0.99597830f, 0.0f};
    darktale_coefficients_t gain_filter;
    darktale_coefficients_t tone_filter;
    float tone_value;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || tone > 99u ||
        volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u) return false;
    gain_filter = gain_coefficients(factory_gain[gain]);
    tone_value = factory_tone_volume[tone];
    tone_filter = (darktale_coefficients_t){
        0.03435718f, 0.03435718f, 0.0f, 0.0f,
        tone_value + 0.05035718f, 0.01835718f - tone_value,
        0.0f, 0.0f};
    effect->volume_target = factory_tone_volume[volume];
    effect->volume = glide(effect->volume, effect->volume_target);
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        output = filter(&effect->gain_filter, gain_filter, audio[n * 2u]);
        output = hard_clip_stage(effect, output);
        output = slew_stage(effect, output);
        output = filter(&effect->post_slew_filter, post_slew, output);
        output = rational_stage(effect, output);
        output = filter(&effect->reconstruction_filter,
                        reconstruction, output);
        output = filter(&effect->tone_filter, tone_filter, output);
        output *= effect->volume * 1.5f;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
