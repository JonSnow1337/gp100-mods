#include "solo100_ld_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct solo100_ld_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} solo100_ld_coefficients_t;

static const uint16_t gain_curve[100] = {
    0,    0,    0,    0,    1,    3,    6,    8,    13,   17,   25,   34,
    41,   53,   62,   77,   89,   107,  121,  143,  168,  186,  214,  235,
    267,  291,  328,  354,  396,  425,  471,  521,  555,  610,  647,  707,
    748,  813,  858,  928,  1001, 1052, 1131, 1186, 1271, 1330, 1420, 1483,
    1580, 1647, 1750, 1857, 1930, 2043, 2121, 2241, 2323, 2449, 2536, 2669,
    2807, 2901, 3045, 3143, 3295, 3398, 3557, 3665, 3830, 3943, 4117, 4294,
    4415, 4601, 4727, 4920, 5051, 5251, 5388, 5596, 5810, 5955, 6176, 6326,
    6556, 6712, 6949, 7110, 7357, 7523, 7778, 8037, 8213, 8481, 8663, 8940,
    9127, 9413, 9606, 9900};

static const uint16_t presence_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};

static const uint16_t bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};

static const float decimator_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float curve(const uint16_t values[100], uint8_t control)
{
    return (float)values[control] / 10000.0f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_solo100_ld_filter_t *state,
                    solo100_ld_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0u; i < coefficients.order; ++i)
        output += coefficients.b[i + 1u] * state->x[i];
    for (i = 0u; i < coefficients.order; ++i)
        output -= coefficients.a[i + 1u] * state->y[i];
    output /= coefficients.a[0];
    for (i = coefficients.order; i > 1u; --i)
    {
        state->x[i - 1u] = state->x[i - 2u];
        state->y[i - 1u] = state->y[i - 2u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static solo100_ld_coefficients_t tone_coefficients(float bass, float middle,
                                                   float treble)
{
    const float middle_squared = middle * middle;
    const float bass_middle = bass * middle;
    const float bass_treble = bass * treble;
    const float middle_treble = middle * treble;
    return (solo100_ld_coefficients_t){
        {bass * .01778791f + middle * .01744181f + treble * .02614062f +
             bass_middle * .6844468f + bass_treble - middle_treble * .025f -
             middle_squared * .01711117f + .0004446978f,
         middle_treble * .075f - middle * .04897936f - treble * .07612695f -
             bass_middle * 1.948277f - bass_treble * 3.f - bass * .01540551f +
             middle_squared * .04870692f - .0003851377f,
         middle * .04574968f - bass * .01778791f + treble * .07385938f +
             bass_middle * 1.843213f + bass_treble * 3.f -
             middle_treble * .075f - middle_squared * .04608032f - .0004446978f,
         bass * .01540551f - middle * .01421213f - treble * .02387305f -
             bass_middle * .5793829f - bass_treble + middle_treble * .025f +
             middle_squared * .01448457f + .0003851377f},
        {bass * 1.11428f - middle * .009970506f + bass_middle * .6844468f -
             middle_squared * .01711117f + .0291077f,
         middle * .02843295f - bass * 3.111898f - bass_middle * 1.948277f +
             middle_squared * .04870692f - .07881302f,
         bass * 2.88572f - middle * .026838f + bass_middle * 1.843213f -
             middle_squared * .04608032f + .07089494f,
         middle * .008375557f - bass * .888102f - bass_middle * .5793829f +
             middle_squared * .01448457f - .02118434f},
        3u};
}

static solo100_ld_coefficients_t presence_coefficients(float presence)
{
    float high_gain;
    float low_gain;
    float tangent;
    float high_forward;
    float high_reverse;
    float low_forward;
    float low_reverse;
    float p0;
    float p1;
    float q0;
    float q1;
    if (presence == 0.f)
        presence = .01f;
    high_gain = powf(10.f, presence * 3.2f / 20.f);
    low_gain = powf(10.f, presence * -2.2f / 20.f);
    tangent = tanf(.02365099266f);
    high_forward = 2.f / (high_gain + 1.f);
    high_reverse = 2.f / (1.f / high_gain + 1.f);
    low_forward = 2.f / (low_gain + 1.f);
    low_reverse = 2.f / (1.f / low_gain + 1.f);
    p0 = tangent + high_forward;
    p1 = tangent - high_forward;
    q0 = tangent * low_forward + 1.f;
    q1 = tangent * low_forward - 1.f;
    return (solo100_ld_coefficients_t){
        {(tangent + high_reverse) * (tangent * low_reverse - 1.f) / (p0 * q0),
         ((tangent + high_reverse) * (tangent * low_reverse + 1.f) +
          (tangent - high_reverse) * (tangent * low_reverse - 1.f)) /
             (p0 * q0),
         (tangent - high_reverse) * (tangent * low_reverse + 1.f) / (p0 * q0),
         0.f},
        {1.f, (p0 * q1 + p1 * q0) / (p0 * q0), (p1 * q1) / (p0 * q0), 0.f},
        2u};
}

static float nonlinear(float input)
{
    float magnitude = fabsf(input);
    float power;
    unsigned i;
    if (magnitude >= 1.5f)
        return copysignf(1.5f, input);
    power = 1.f - magnitude * 2.5f / 1024.f;
    for (i = 0u; i < 10u; ++i)
        power *= power;
    return copysignf((1.f - power) * 1.5f, input);
}

static solo100_ld_coefficients_t gain_coefficients(float gain)
{
    const float squared = gain * gain;
    return (solo100_ld_coefficients_t){
        {squared - gain * 1.022676f, squared * -2.976164f + gain * 2.9983f,
         squared * 2.952329f - gain * 2.929653f,
         squared * -.9761644f + gain * .9540291f},
        {squared - gain * .9876635f - .03529201f,
         squared * -2.976164f + gain * 2.963855f + .0341644f,
         squared * 2.952329f - gain * 2.964665f + .03529078f,
         squared * -.9761644f + gain * .9884739f - .03416563f},
        3u};
}

static float nonlinear_stage(gp100_solo100_ld_amplifier_t *amplifier,
                             float input)
{
    float point[4];
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float output = 0.f;
    size_t i;
    point[0] = amplifier->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.f;
    point[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
    {
        amplifier->fir_history[i] = nonlinear(point[3u - i]);
    }
    for (i = 0u; i < 40u; ++i)
    {
        output += amplifier->fir_history[i] * decimator_fir[i];
    }
    amplifier->previous_nonlinear_input = input;
    return output;
}

bool gp100_solo100_ld_amplifier_reset(
    gp100_solo100_ld_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble)
{
    if (amplifier == NULL || gain > 99u || presence > 99u || master > 99u ||
        bass > 99u || middle > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = curve(gain_curve, gain);
    amplifier->presence = amplifier->presence_target =
        curve(presence_curve, presence);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bass = amplifier->bass_target = curve(bass_curve, bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_solo100_ld_amplifier_process(
    gp100_solo100_ld_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble, float *audio, size_t frames, bool enabled)
{
    static const solo100_ld_coefficients_t input[2] = {
        {{23.52809715f, -46.66475296f, 23.13820076f, 0.f},
         {1.f, -1.931028843f, .9325397015f, 0.f},
         2u},
        {{46.66475296f, 27.02453041f, .961492002f, 0.f},
         {1.f, -.6675130129f, .154447794f, 0.f},
         2u}};
    static const solo100_ld_coefficients_t anti_alias = {
        {.01137500272f, .02275000544f, .01137500272f, 0.f},
        {1.149993395f, -1.954499989f, .8500066053f, 0.f},
        2u};
    size_t n;
    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        presence > 99u || master > 99u || bass > 99u || middle > 99u ||
        treble > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = curve(gain_curve, gain);
    amplifier->presence_target = curve(presence_curve, presence);
    amplifier->master_target = (float)master * .01f;
    amplifier->bass_target = curve(bass_curve, bass);
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    for (n = 0u; n < frames; ++n)
    {
        float signal;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->presence = amplifier->presence_target;
        amplifier->bass = amplifier->bass_target;
        signal = filter(&amplifier->input_filter[0], input[0], audio[n * 2u]);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        signal = filter(&amplifier->gain_filter,
                        gain_coefficients(amplifier->gain), signal);
        signal = filter(&amplifier->anti_alias_filter, anti_alias, signal);
        signal = nonlinear_stage(amplifier, signal);
        signal = filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->bass, amplifier->middle,
                                          amplifier->treble),
                        signal);
        signal *= amplifier->master;
        signal = filter(&amplifier->presence_filter,
                        presence_coefficients(amplifier->presence), signal);
        signal *= 1.5f * .0767f * 11.f * .5f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
