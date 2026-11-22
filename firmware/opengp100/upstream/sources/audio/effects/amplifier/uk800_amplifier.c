#include "uk800_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct uk800_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} uk800_coefficients_t;

static const uint16_t control_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};

static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float curve(uint8_t value)
{
    return (float)control_curve[value] / 10000.0f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_uk800_filter_t *state,
                    uk800_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0; i < coefficients.order; ++i)
        output += coefficients.b[i + 1] * state->x[i];
    for (i = 0; i < coefficients.order; ++i)
        output -= coefficients.a[i + 1] * state->y[i];
    output /= coefficients.a[0];
    for (i = coefficients.order; i > 1; --i)
    {
        state->x[i - 1] = state->x[i - 2];
        state->y[i - 1] = state->y[i - 2];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static uk800_coefficients_t gain_voicing(float gain)
{
    float gain_squared = gain * gain;
    uk800_coefficients_t result = {
        {gain_squared - gain * 1.011338f,
         gain_squared * -2.90236f + gain * 2.912591f,
         gain_squared * 2.80472f - gain * 2.793382f,
         gain_squared * -.9023599f + gain * .892129f},
        {gain_squared - gain * .9765392f - .03506465f,
         gain_squared * -2.90236f + gain * 2.878949f + .03337474f,
         gain_squared * 2.80472f - gain * 2.828181f + .03506351f,
         gain_squared * -.9023599f + gain * .9257703f - .03337588f},
        3};
    return result;
}

static uk800_coefficients_t tone(float bass, float middle, float treble)
{
    float treble_squared = treble * treble;
    float middle_squared = middle * middle;
    float bass_middle = bass * middle;
    float bass_treble = bass * treble;
    float middle_treble = middle * treble;
    float bass_middle_scale = bass * .1466667f * middle;
    float bass_middle_scale_three = bass * .44f * middle;
    uk800_coefficients_t result = {
        {bass * .02205095f + middle * .01889048f + treble * .02303876f +
             bass_middle * .8413287f + bass_treble - middle_treble * .022f -
             middle_squared * .01850923f + .000485121f,
         middle_treble * .066f - middle * .05254846f - treble * .06702267f -
             bass_middle * 2.374662f - bass_treble * 3.f - bass * .01855298f +
             middle_squared * .05224257f - .0004081656f,
         middle * .04857619f - bass * .02205095f + treble * .06496124f +
             bass_middle * 2.225338f + bass_treble * 3.f -
             middle_treble * .066f - middle_squared * .04895743f - .000485121f,
         bass * .01855298f - middle * .0149182f - treble * .02097733f -
             bass_middle * .6920046f - bass_treble + middle_treble * .022f +
             middle_squared * .0152241f + .0004081656f},
        {bass * 1.132854f - middle * .004976377f + treble * .005685316f +
             bass_middle * .8661223f + bass_treble * .2468968f -
             middle_treble * .002197921f - bass * treble_squared * .2234357f +
             middle * treble_squared * .001537748f -
             middle_squared * treble * .003088914f -
             middle_squared * .01905469f - treble_squared * .005144114f +
             middle_squared * treble_squared * .003226667f +
             bass_middle * treble * .1404052f -
             bass_middle_scale * treble_squared + .02607546f,
         middle * .01435205f - bass * 3.129111f - treble * .01529505f -
             bass_middle * 2.44338f - bass_treble * .6861594f +
             middle_treble * .006409164f + bass * treble_squared * .6634357f -
             middle * treble_squared * .004764414f +
             middle_squared * treble * .008574247f +
             middle_squared * .05375437f + treble_squared * .01482057f -
             middle_squared * treble_squared * .00968f -
             bass_middle * treble * .3897385f +
             bass_middle_scale_three * treble_squared - .06974743f,
         bass * 2.867149f - middle * .01362096f + treble * .01367546f +
             bass_middle * 2.288544f + bass_treble * .6331032f -
             middle_treble * .006191412f - bass * treble_squared * .6565643f +
             middle * treble_squared * .004915586f -
             middle_squared * treble * .007881753f -
             middle_squared * .05034798f - treble_squared * .01421589f +
             middle_squared * treble_squared * .00968f +
             bass_middle * treble * .3582615f -
             bass_middle_scale_three * treble_squared + .06192815f,
         middle * .004245286f - bass * .8708859f - treble * .004064175f -
             bass_middle * .7112864f - bass_treble * .1938406f +
             middle_treble * .00198017f + bass * treble_squared * .2165643f -
             middle * treble_squared * .001688919f +
             middle_squared * treble * .002396419f +
             middle_squared * .0156483f + treble_squared * .004539427f -
             middle_squared * treble_squared * .003226667f -
             bass_middle * treble * .1089282f +
             bass_middle_scale * treble_squared - .01824896f},
        3};
    return result;
}

static float asymmetric_shape(float input)
{
    float magnitude = fabsf(input);
    float power;
    float limit = input < 0.f ? 3.f : 2.5f;
    unsigned i;
    if (magnitude >= limit)
        return copysignf(limit, input);
    power = 1.f - magnitude * 15.f / 1024.f;
    for (i = 0; i < 10; ++i)
        power *= power;
    return copysignf((1.f - power) * limit, input);
}

static float power_stage(gp100_uk800_amplifier_t *amplifier, float input)
{
    float phase[4];
    float delta = (input - amplifier->previous_nonlinear_input) / 3.f;
    float output = 0.f;
    size_t i;
    phase[0] = amplifier->previous_nonlinear_input;
    phase[1] = phase[0] + delta;
    phase[2] = phase[0] + delta * 2.f;
    phase[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36 * sizeof(float));
    for (i = 0; i < 4; ++i)
    {
        amplifier->fir_history[i] = asymmetric_shape(phase[3 - i]);
    }
    for (i = 0; i < 40; ++i)
        output += amplifier->fir_history[i] * fir[i];
    amplifier->previous_nonlinear_input = input;
    return output * .65f;
}

bool gp100_uk800_amplifier_reset(gp100_uk800_amplifier_t *amplifier,
                                      uint8_t gain, uint8_t presence,
                                      uint8_t master, uint8_t bass,
                                      uint8_t middle, uint8_t treble)
{
    if (!amplifier || gain > 99 || presence > 99 || master > 99 || bass > 99 ||
        middle > 99 || treble > 99)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = curve(gain) * .99f;
    amplifier->presence = curve(presence);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bass = curve(bass);
    amplifier->middle = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_uk800_amplifier_process(gp100_uk800_amplifier_t *amplifier,
                                        uint8_t gain, uint8_t presence,
                                        uint8_t master, uint8_t bass,
                                        uint8_t middle, uint8_t treble,
                                        float *audio, size_t frame_count,
                                        bool enabled)
{
    static const uk800_coefficients_t input_filters[2] = {
        {{4.2770586f, -8.406497f, 4.1307211f, 0.f},
         {1.f, -1.92004335f, .921685696f, 0.f},
         2},
        {{8.406497f, 8.3868275f, 1.36023068f, 0.f},
         {1.f, .928139329f, .110984199f, 0.f},
         2}};
    static const uk800_coefficients_t post_gain = {
        {1.009f, -1.9998f, .991f, 0.f}, {1.0051f, -1.9998f, .9949f, 0.f}, 2};
    size_t frame;
    size_t i;
    if (!amplifier || !amplifier->configured || gain > 99 || presence > 99 ||
        master > 99 || bass > 99 || middle > 99 || treble > 99 ||
        (frame_count && !audio) || frame_count > SIZE_MAX / 2)
        return false;
    amplifier->gain_target = curve(gain) * .99f;
    amplifier->presence = curve(presence);
    amplifier->master_target = (float)master * .01f;
    amplifier->bass = curve(bass);
    amplifier->middle = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    for (frame = 0; frame < frame_count; ++frame)
    {
        float sample;
        float angle;
        float scale;
        float q;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        sample = audio[frame * 2];
        for (i = 0; i < 2; ++i)
        {
            sample =
                filter(&amplifier->input_filter[i], input_filters[i], sample);
        }
        sample = filter(&amplifier->gain_filter, gain_voicing(amplifier->gain),
                        sample);
        sample = filter(&amplifier->post_gain_filter, post_gain, sample);
        sample = power_stage(amplifier, sample);
        sample =
            filter(&amplifier->tone_filter,
                   tone(amplifier->bass, amplifier->middle, amplifier->treble),
                   sample);
        sample *= amplifier->master;
        angle = 1.42475855f;
        scale = powf(10.f, amplifier->presence * 3.f / 40.f);
        q = sinf(angle) / .6f;
        sample = filter(
            &amplifier->presence_filter,
            (uk800_coefficients_t){
                {q * scale + 1.f, -2.f * cosf(angle), 1.f - q * scale, 0.f},
                {q / scale + 1.f, -2.f * cosf(angle), 1.f - q / scale, 0.f},
                2},
            sample);
        sample *= .7225f;
        audio[frame * 2] = sample;
        audio[frame * 2 + 1] = sample;
    }
    return true;
}
