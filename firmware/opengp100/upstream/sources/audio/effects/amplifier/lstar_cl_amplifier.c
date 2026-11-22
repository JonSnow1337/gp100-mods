#include "lstar_cl_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct lstar_cl_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} lstar_cl_coefficients_t;

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f,  0.0015f,  0.0032f,  0.0048f,  0.0053f,
    0.0034f,  -0.0016f, -0.0092f, -0.0175f, -0.0233f, -0.0227f, -0.0123f,
    0.0097f,  0.0421f,  0.0807f,  0.1193f,  0.1504f,  0.1678f,  0.1678f,
    0.1504f,  0.1193f,  0.0807f,  0.0421f,  0.0097f,  -0.0123f, -0.0227f,
    -0.0233f, -0.0175f, -0.0092f, -0.0016f, 0.0034f,  0.0053f,  0.0048f,
    0.0032f,  0.0015f,  0.0001f,  -0.0007f, -0.0011f};

static float shifted_curve(uint8_t value, float exponent)
{
    float index = floorf((float)value * 2.55f) - 2.f;
    if (index < 0.f)
        index = 0.f;
    if (index > 251.f)
        index = 251.f;
    return floorf(powf(index / 251.f, exponent) * 10000.f) / 10000.f;
}

static float bass_curve(uint8_t value)
{
    float index = floorf((float)value * 2.55f);
    return floorf(powf(index / 255.f, 4.5f) * 10000.f) / 10000.f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_lstar_cl_filter_t *state,
                    lstar_cl_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0u; i < coefficients.order; ++i)
        output += coefficients.b[i + 1u] * state->x[i];
    for (i = 0u; i < coefficients.order; ++i)
        output -= coefficients.a[i + 1u] * state->y[i];
    output /= coefficients.a[0];
    for (i = 2u; i != 0u; --i)
    {
        state->x[i] = state->x[i - 1u];
        state->y[i] = state->y[i - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static lstar_cl_coefficients_t tone_coefficients(float bass, float middle,
                                                 float treble)
{
    return (lstar_cl_coefficients_t){
        {bass * .004269454f + middle * .0002243567f + treble * .001419534f +
             bass * .1581859f * middle + bass * treble,
         bass * -.00339205f - middle * .0001728065f - treble * .001417346f -
             bass * .4381859f * middle - bass * 3.f * treble,
         bass * .4018141f * middle - middle * .0002243567f -
             treble * .001419534f - bass * .004269454f + bass * 3.f * treble,
         bass * .00339205f + middle * .0001728065f + treble * .001417346f -
             bass * .1218141f * middle - bass * treble},
        {bass * 1.185675f + middle * .0002243567f + bass * .1581859f * middle +
             .001677046f,
         bass * -3.184798f - middle * .0001728065f - bass * .4381859f * middle -
             .001159437f,
         bass * 2.814325f - middle * .0002243567f + bass * .4018141f * middle -
             .001676252f,
         middle * .0001728065f - bass * .8152021f - bass * .1218141f * middle +
             .001160231f},
        3u};
}

static lstar_cl_coefficients_t gain_coefficients(float gain)
{
    float square = gain * gain;
    return (lstar_cl_coefficients_t){
        {square - gain * 1.176482f, gain * .9875178f - square, 0.f, 0.f},
        {square - gain - .1764822f, gain - square - .01248224f, 0.f, 0.f},
        1u};
}

static lstar_cl_coefficients_t master_coefficients(float master, float presence)
{
    float square = master * master;
    return (lstar_cl_coefficients_t){
        {master * -.0101231f - master * presence, master * -.02024619f,
         master * presence - master * .0101231f, 0.f},
        {square * 9.45f * presence - presence - master * 9.45f * presence -
             master * .8928571f + square * .8928571f - .0101231f,
         presence * -18.9f * square + presence * 18.9f * master - .02024619f,
         presence + master * .8928571f - master * 9.45f * presence +
             square * 9.45f * presence - square * .8928571f - .0101231f,
         0.f},
        2u};
}

static float shaped_sample(float input, float drive, float amplitude)
{
    return copysignf(amplitude * (1.f - expf(-drive * fabsf(input))), input);
}

static float nonlinear_stage(gp100_lstar_cl_amplifier_t *amplifier,
                             float input, float envelope)
{
    float amplitude = .2f - amplifier->gain * .1f;
    float drive = amplifier->gain * 5.f + 5.f + envelope * 15.f;
    float normalization = fmaxf(1.f / (drive * 3.f + 1.f), .4f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float points[4];
    float output = 0.f;
    size_t i;

    points[0] = amplifier->previous_nonlinear_input;
    points[1] = points[0] + step;
    points[2] = points[0] + step * 2.f;
    points[3] = points[0] + step * 3.f;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 40u; i != 0u; --i)
        output += reconstruction_fir[i - 1u] * amplifier->fir_history[i - 1u];
    amplifier->fir_history[0] =
        shaped_sample(points[3], drive, amplitude) * normalization;
    amplifier->fir_history[1] =
        shaped_sample(points[2], drive, amplitude) * normalization;
    amplifier->fir_history[2] =
        shaped_sample(points[1], drive, amplitude) * normalization;
    amplifier->fir_history[3] =
        shaped_sample(points[0], drive, amplitude) * normalization;
    amplifier->previous_nonlinear_input = input;
    return output;
}

bool gp100_lstar_cl_amplifier_reset(
    gp100_lstar_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble)
{
    if (amplifier == NULL || gain > 99u || presence > 99u || master > 99u ||
        bass > 99u || middle > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = shifted_curve(gain, 3.f) * .7f;
    amplifier->presence = amplifier->presence_target =
        shifted_curve(presence, 3.f);
    amplifier->master = amplifier->master_target = shifted_curve(master, 2.5f);
    amplifier->bass = bass_curve(bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = shifted_curve(treble, 3.f);
    amplifier->configured = true;
    return true;
}

bool gp100_lstar_cl_amplifier_process(
    gp100_lstar_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble, float *audio,
    size_t frames, bool enabled)
{
    static const lstar_cl_coefficients_t input = {
        {61.1926689f, -114.803398f, 53.621151f, 0.f},
        {1.f, -1.87485111f, .875176907f, 0.f},
        2u};
    static const lstar_cl_coefficients_t envelope_coefficients = {
        {GP100_ENVELOPE_50_MS_B0, 0.f, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_50_MS_A1, GP100_ENVELOPE_50_MS_A2, 0.f},
        2u};
    size_t frame;

    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        presence > 99u || master > 99u || bass > 99u || middle > 99u ||
        treble > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = shifted_curve(gain, 3.f) * .7f;
    amplifier->presence_target = shifted_curve(presence, 3.f);
    amplifier->master_target = shifted_curve(master, 2.5f);
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = shifted_curve(treble, 3.f);

    for (frame = 0u; frame < frames; ++frame)
    {
        float signal;
        float level;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->presence =
            glide(amplifier->presence, amplifier->presence_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->bass = bass_curve(bass);
        signal = filter(&amplifier->input_filter, input, audio[frame * 2u]);
        signal = filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->bass, amplifier->middle,
                                          amplifier->treble),
                        signal);
        signal = filter(&amplifier->gain_filter,
                        gain_coefficients(amplifier->gain), signal) *
                 15.f;
        signal =
            filter(&amplifier->master_filter,
                   master_coefficients(amplifier->master, amplifier->presence),
                   signal);
        level = filter(&amplifier->envelope_filter, envelope_coefficients,
                       fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level) * 9.425052f;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
