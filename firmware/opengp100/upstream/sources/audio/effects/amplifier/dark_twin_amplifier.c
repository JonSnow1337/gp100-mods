#include "dark_twin_amplifier.h"

#include "audio/dsp/decimation_internal.h"
#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct dark_twin_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} dark_twin_coefficients_t;

/* Reachable entries from the two factory tapers, stored in ten-thousandths. */
static const uint16_t gain_taper[100] = {
    0, 0, 13, 28, 56, 79, 117, 146, 192, 224,
    277, 333, 372, 434, 476, 543, 589, 660, 709, 784,
    862, 915, 997, 1053, 1139, 1198, 1287, 1348, 1441, 1504,
    1600, 1699, 1765, 1867, 1936, 2040, 2111, 2219, 2291, 2402,
    2514, 2590, 2705, 2783, 2901, 2980, 3101, 3182, 3305, 3388,
    3514, 3641, 3727, 3857, 3944, 4076, 4165, 4300, 4390, 4527,
    4666, 4759, 4899, 4994, 5137, 5233, 5378, 5475, 5623, 5722,
    5871, 6022, 6123, 6276, 6379, 6534, 6638, 6795, 6900, 7059,
    7219, 7327, 7489, 7598, 7762, 7872, 8038, 8150, 8318, 8430,
    8600, 8771, 8886, 9059, 9175, 9349, 9467, 9643, 9761, 9940};

static const uint16_t bass_taper[100] = {
    0, 0, 1, 4, 10, 15, 26, 35, 51, 63,
    84, 107, 124, 152, 172, 205, 229, 266, 293, 335,
    381, 412, 462, 497, 552, 590, 650, 691, 755, 800,
    869, 941, 990, 1067, 1120, 1201, 1257, 1343, 1402, 1493,
    1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172, 2285, 2362,
    2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750,
    4916, 5085, 5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285,
    6476, 6605, 6801, 6933, 7133, 7269, 7474, 7612, 7822, 7964,
    8179, 8396, 8543, 8765, 8915, 9142, 9295, 9527, 9683, 9920};

static float gain_curve(uint8_t value)
{
    return (float)gain_taper[value] / 10000.0f;
}

static float bass_curve(uint8_t value)
{
    return (float)bass_taper[value] / 10000.0f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + 0.0001f, target);
    if (current > target)
        return fmaxf(current - 0.0001f, target);
    return target;
}

static float filter(gp100_dark_twin_filter_t *state,
                    dark_twin_coefficients_t coefficients, float input)
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

static dark_twin_coefficients_t tone_coefficients(float bass, float middle,
                                                  float treble)
{
    return (dark_twin_coefficients_t){
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

static dark_twin_coefficients_t bright_coefficients(float gain)
{
    float square = gain * gain;
    return (dark_twin_coefficients_t){
        {square - gain * 1.094482f, gain * .9055178f - square, 0.f, 0.f},
        {square - gain - .09448224f, gain - square - .09448224f, 0.f, 0.f},
        1u};
}

static float shaped_sample(float input, float drive)
{
    return copysignf(.15f * (1.f - expf(-drive * fabsf(input))), input);
}

static float nonlinear_stage(gp100_dark_twin_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 3.5f + .5f;
    float normalization = fmaxf(1.f / (drive * 3.f + 1.f), .13f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float points[4] = {
        amplifier->previous_nonlinear_input,
        amplifier->previous_nonlinear_input + step,
        amplifier->previous_nonlinear_input + step * 2.f,
        amplifier->previous_nonlinear_input + step * 3.f};

    for (size_t i = 0u; i < 4u; ++i)
        points[i] = shaped_sample(points[i], drive) * normalization;
    amplifier->previous_nonlinear_input = input;
    return gp100_decimate_four(amplifier->fir_history, points);
}

bool gp100_dark_twin_amplifier_reset(
    gp100_dark_twin_amplifier_t *amplifier, uint8_t gain, uint8_t volume,
    uint8_t bass, uint8_t middle, uint8_t treble, bool bright)
{
    if (amplifier == NULL || gain > 99u || volume > 99u || bass > 99u ||
        middle > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = gain_curve(gain);
    amplifier->volume = amplifier->volume_target = (float)volume * .01f;
    amplifier->bass = amplifier->bass_target = bass_curve(bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->bright = bright;
    amplifier->configured = true;
    return true;
}

bool gp100_dark_twin_amplifier_process(
    gp100_dark_twin_amplifier_t *amplifier, uint8_t gain, uint8_t volume,
    uint8_t bass, uint8_t middle, uint8_t treble, bool bright, float *audio,
    size_t frames, bool enabled)
{
    static const dark_twin_coefficients_t input[3] = {
        {{2.53726482f, -4.77261734f, 2.23561168f, 0.f},
         {1.f, -1.88071895f, .880913794f, 0.f},
         2u},
        {{4.77261734f, .94700098f, .0023504f, 0.f},
         {1.f, .105832197f, .0003515f, 0.f},
         2u},
        {{4.77261734f, 3.81607556f, .0003141f, 0.f},
         {1.f, .791965723f, .0003815f, 0.f},
         2u}};
    static const dark_twin_coefficients_t voicing[2] = {
        {{.742847085f, -1.37555003f, .63563931f, 0.f},
         {1.f, -1.95859754f, .960272908f, 0.f},
         2u},
        {{1.37555003f, -1.33397675f, .0530951f, 0.f},
         {1.f, -.9222489f, .0898777023f, 0.f},
         2u}};
    static const dark_twin_coefficients_t envelope_coefficients = {
        {GP100_ENVELOPE_10_MS_B0, 0.f, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_10_MS_A1, GP100_ENVELOPE_10_MS_A2, 0.f},
        2u};
    size_t frame;

    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        volume > 99u || bass > 99u || middle > 99u || treble > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = gain_curve(gain);
    amplifier->volume_target = (float)volume * .01f;
    amplifier->bass_target = bass_curve(bass);
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    amplifier->bright = bright;

    for (frame = 0u; frame < frames; ++frame)
    {
        dark_twin_coefficients_t tone;
        float signal;
        float level;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;

        tone = tone_coefficients(amplifier->bass, amplifier->middle,
                                 amplifier->treble);
        signal = filter(&amplifier->input_filter[0], input[0],
                        audio[frame * 2u] * 1.1f);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        signal = filter(&amplifier->input_filter[2], input[2], signal);
        signal = filter(&amplifier->tone_filter, tone, signal);
        if (amplifier->bright)
            signal = filter(&amplifier->bright_filter,
                            bright_coefficients(amplifier->gain), signal);
        else
            signal *= amplifier->gain;
        signal = filter(&amplifier->voicing_filter[0], voicing[0], signal);
        signal = filter(&amplifier->voicing_filter[1], voicing[1], signal);
        level = filter(&amplifier->envelope_filter, envelope_coefficients,
                       fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal *= 55.0f;
        signal *= amplifier->volume;
        signal *= 0x1.71758ep-4f;
        signal *= 7.0f;
        signal *= 1.64f;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
