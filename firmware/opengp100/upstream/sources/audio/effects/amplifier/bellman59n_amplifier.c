#include "bellman59n_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct bellman_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} bellman_coefficients_t;

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f,  0.0015f,  0.0032f,  0.0048f,  0.0053f,
    0.0034f,  -0.0016f, -0.0092f, -0.0175f, -0.0233f, -0.0227f, -0.0123f,
    0.0097f,  0.0421f,  0.0807f,  0.1193f,  0.1504f,  0.1678f,  0.1678f,
    0.1504f,  0.1193f,  0.0807f,  0.0421f,  0.0097f,  -0.0123f, -0.0227f,
    -0.0233f, -0.0175f, -0.0092f, -0.0016f, 0.0034f,  0.0053f,  0.0048f,
    0.0032f,  0.0015f,  0.0001f,  -0.0007f, -0.0011f};

static float quantized_power(uint8_t value, float offset, float divisor,
                             float exponent)
{
    float index = floorf((float)value * 2.55f) - offset;
    float result;
    if (index < 0.0f)
        index = 0.0f;
    if (index > divisor)
        index = divisor;
    result = powf(index / divisor, exponent);
    return roundf(result * 10000.0f) * 0.0001f;
}

static float gain_curve(uint8_t value)
{
    return quantized_power(value, 2.0f, 251.0f, 2.0f);
}

static float presence_curve(uint8_t value)
{
    return quantized_power(value, 2.0f, 251.0f, 1.5f);
}

static float bass_curve(uint8_t value)
{
    return quantized_power(value, 0.0f, 255.0f, 3.5f);
}

static float filter(gp100_bellman59n_filter_t *state,
                    bellman_coefficients_t c, float input)
{
    const float x[3] = {state->x1, state->x2, state->x3};
    const float y[3] = {state->y1, state->y2, state->y3};
    float output = c.b[0] * input;
    size_t i;
    for (i = 0u; i < c.order; ++i)
        output += c.b[i + 1u] * x[i] - c.a[i + 1u] * y[i];
    output /= c.a[0];
    state->x3 = state->x2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y3 = state->y2;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static bellman_coefficients_t tone_coefficients(float bass, float middle,
                                                float treble)
{
    float m2 = middle * middle;
    return (bellman_coefficients_t){
        {bass * .01651759f + middle * .01600963f + treble * .02613953f +
             bass * .6284257f * middle + bass * treble -
             middle * .025f * treble - m2 * .01571064f + .0004129397f,
         middle * .075f * treble - middle * .04323924f - treble * .07612805f -
             bass * 1.721283f * middle - bass * 3.f * treble -
             bass * .0127989f + m2 * .04303207f - .0003199725f,
         middle * .03863323f - bass * .01651759f + treble * .07386047f +
             bass * 1.557289f * middle + bass * 3.f * treble -
             middle * .075f * treble - m2 * .03893222f - .0004129397f,
         bass * .0127989f - middle * .01140362f - treble * .02387195f -
             bass * .4644315f * middle - bass * treble +
             middle * .025f * treble + m2 * .01161079f + .0003199725f},
        {bass * 1.197923f - middle * .01352552f + bass * .6284257f * middle -
             m2 * .01571064f + .03129433f,
         middle * .03629591f - bass * 3.194205f - bass * 1.721283f * middle +
             m2 * .04303207f - .08077437f,
         bass * 2.802077f - middle * .03183162f + bass * 1.557289f * middle -
             m2 * .03893222f + .06870983f,
         middle * .009061233f - bass * .8057952f - bass * .4644315f * middle +
             m2 * .01161079f - .01922147f},
        3u};
}

static bellman_coefficients_t presence_coefficients(float presence)
{
    const float frequency = 1.23640549f;
    float scale = powf(10.0f, presence * 22.7f / 40.0f);
    float k = sinf(frequency) / 1.2f;
    float shared = -2.0f * cosf(frequency);
    return (bellman_coefficients_t){
        {k * scale + 1.0f, shared, 1.0f - k * scale},
        {k / scale + 1.0f, shared, 1.0f - k / scale},
        2u};
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + 0.0001f, target);
    if (current > target)
        return fmaxf(current - 0.0001f, target);
    return target;
}

static float nonlinear_stage(gp100_bellman59n_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 2.0f + 1.4f;
    float normalization = fmaxf(1.0f / (drive * 3.0f + 1.0f), 0.1f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.0f;
    float output = 0.0f;
    size_t i;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
    {
        float point = amplifier->previous_nonlinear_input + step * (float)i;
        float shaped =
            copysignf(0.15f * (1.0f - expf(-drive * fabsf(point))), point) *
            normalization;
        amplifier->fir_history[3u - i] = shaped;
    }
    amplifier->previous_nonlinear_input = input;
    for (i = 0u; i < 40u; ++i)
        output += reconstruction_fir[i] * amplifier->fir_history[i];
    return output;
}

bool gp100_bellman59n_amplifier_reset(
    gp100_bellman59n_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t volume, uint8_t bass, uint8_t middle,
    uint8_t treble)
{
    if (amplifier == NULL || gain > 99u || presence > 99u || volume > 99u ||
        bass > 99u || middle > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = gain_curve(gain);
    amplifier->presence = presence_curve(presence);
    amplifier->volume = amplifier->volume_target = (float)volume * .01f;
    amplifier->bass = bass_curve(bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_bellman59n_amplifier_process(
    gp100_bellman59n_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t volume, uint8_t bass, uint8_t middle,
    uint8_t treble, float *audio, size_t frames, bool enabled)
{
    static const bellman_coefficients_t input[2] = {
        {{6.68555689f, -8.51430607f, 1.83079243f},
         {1.f, -1.30411935f, .30448091f},
         2u},
        {{8.51430607f, 3.82515931f, .0000794f},
         {1.f, .382216811f, .0000703f},
         2u}};
    static const bellman_coefficients_t envelope = {
        {GP100_ENVELOPE_10_MS_B0, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_10_MS_A1, GP100_ENVELOPE_10_MS_A2},
        2u};
    size_t n;
    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        presence > 99u || volume > 99u || bass > 99u || middle > 99u ||
        treble > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = gain_curve(gain);
    amplifier->volume_target = (float)volume * .01f;
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    for (n = 0u; n < frames; ++n)
    {
        bellman_coefficients_t gain_c, tone_c, presence_c;
        float signal, level;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->presence = presence_curve(presence);
        amplifier->bass = bass_curve(bass);
        gain_c = (bellman_coefficients_t){
            {amplifier->gain * .9994334f, amplifier->gain * -.9994334f, 0.f},
            {1.f, -.9988669f, 0.f},
            1u};
        tone_c = tone_coefficients(amplifier->bass, amplifier->middle,
                                   amplifier->treble);
        presence_c = presence_coefficients(amplifier->presence);
        signal =
            filter(&amplifier->input_filter[0], input[0], audio[n * 2u] * 1.1f);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        signal = filter(&amplifier->gain_filter, gain_c, signal);
        signal = filter(&amplifier->tone_filter, tone_c, signal);
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal = filter(&amplifier->presence_filter, presence_c, signal);
        signal *= 63.8352f * amplifier->volume;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
