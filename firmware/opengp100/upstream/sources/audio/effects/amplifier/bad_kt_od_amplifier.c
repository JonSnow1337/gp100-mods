#include "bad_kt_od_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct bad_kt_od_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} bad_kt_od_coefficients_t;

static const uint16_t gain_curve[100] = {
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

static const uint16_t presence_edge_curve[100] = {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    1,    1,    2,    2,    3,    4,    5,
    7,    8,    10,   12,   15,   18,   22,   27,   30,   37,   42,   50,
    56,   66,   73,   86,   100,  110,  128,  140,  161,  176,  201,  220,
    249,  271,  306,  344,  372,  417,  450,  502,  539,  600,  643,  712,
    788,  841,  927,  988,  1085, 1155, 1265, 1343, 1467, 1555, 1695, 1844,
    1950, 2117, 2234, 2420, 2551, 2758, 2903, 3132, 3375, 3546, 3814, 4002,
    4298, 4505, 4829, 5056, 5412, 5660, 6050, 6460, 6746, 7194, 7505, 7992,
    8331, 8860, 9228, 9802};

static const float interpolation[8][5] = {
    {-.001148f, .0007915f, .0679637f, .0583497f, -.0017144f},
    {-.0014401f, .0048516f, .0759497f, .0478418f, -.0029456f},
    {-.0018844f, .0106244f, .0816701f, .0371915f, -.0032475f},
    {-.002442f, .0181034f, .0846549f, .0270901f, -.002975f},
    {-.002975f, .0270901f, .0846549f, .0181034f, -.002442f},
    {-.0032475f, .0371915f, .0816701f, .0106244f, -.0018844f},
    {-.0029456f, .0478418f, .0759497f, .0048516f, -.0014401f},
    {-.0017144f, .0583497f, .0679637f, .0007915f, -.001148f}};

static const float reconstruction[20] = {
    -.001148f,  -.0014401f, -.0018844f, -.002442f, -.002975f,
    -.0032475f, -.0029456f, -.0017144f, .0007915f, .0048516f,
    .0106244f,  .0181034f,  .0270901f,  .0371915f, .0478418f,
    .0583497f,  .0679637f,  .0759497f,  .0816701f, .0846549f};

static float curve(const uint16_t values[100], uint8_t control)
{
    return (float)values[control] * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_bad_kt_od_filter_t *state,
                    bad_kt_od_coefficients_t c, float input)
{
    float output = c.b[0] * input;
    size_t i;
    for (i = 0u; i < c.order; ++i)
        output += c.b[i + 1u] * state->x[i] - c.a[i + 1u] * state->y[i];
    output /= c.a[0];
    for (i = c.order; i > 1u; --i)
    {
        state->x[i - 1u] = state->x[i - 2u];
        state->y[i - 1u] = state->y[i - 2u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static bad_kt_od_coefficients_t edge_coefficients(float edge)
{
    return (bad_kt_od_coefficients_t){
        {edge * .05242032f + .00005943347f, .00005943347f - edge * .05242032f,
         edge * -.05242032f - .00005943347f, edge * .05242032f - .00005943347f},
        {edge + .00164861f, edge * -2.564223f - .0009312053f,
         edge * 2.129166f - .001647793f, .0009320221f - edge * .5649431f},
        3u};
}

static bad_kt_od_coefficients_t tone_coefficients(float bass, float treble)
{
    float b2 = bass * bass, t2 = treble * treble;
    return (bad_kt_od_coefficients_t){
        {b2 * .031647824f - bass * .1392085f - treble * .000689386f -
             bass * treble - .000020600715f + b2 * .9940578f * treble +
             b2 * .1392504f - bass * .0316383f - treble * .00015667865f -
             bass * .22727275f * treble + b2 * .2259222f * treble -
             .00009064314f,
         bass * .3555881f + .000013552305f + treble * .0006858423f +
             bass * 3.001373f * treble - b2 * .0808215f -
             b2 * 2.982173f * treble - b2 * .3556145f + bass * .0808155f +
             treble * .00015587325f + bass * .6821305f * treble -
             b2 * .6777665f * treble + .00005963014f,
         .000020600715f - bass * .2981769f + treble * .000689386f -
             bass * 3.002739f * treble + b2 * .06775795f +
             b2 * 2.982173f * treble + b2 * .298135f - bass * .06776745f +
             treble * .00015667865f - bass * .6824405f * treble +
             b2 * .6777665f * treble + .00009064314f,
         bass * .08179731f - .000013552305f - treble * .0006858423f +
             bass * 1.001366f * treble - b2 * .018584294f -
             b2 * .9940578f * treble - b2 * .0817709f + bass * .018590296f -
             treble * .00015587325f + bass * .2275832f * treble -
             b2 * .2259222f * treble - .00005963014f},
        {b2 * .30879956f - bass * 1.42741f - treble * .004193891f -
             bass * 6.108763f * treble - .00021280735f + bass * 5.02043f * t2 +
             b2 * 6.075818f * treble + b2 * 1.420162f + t2 * .003444054f -
             b2 * 4.993575f * t2 - bass * .3104468f - .0009745392f,
         bass * 3.73359f + .0001337149f + treble * .002612361f +
             bass * 16.01018f * treble - b2 * .8098175f -
             bass * 15.02127f * t2 - b2 * 15.91549f * treble - b2 * 3.712758f -
             t2 * .003427947f + b2 * 14.93415f * t2 + bass * .8145525f +
             .0006122189f,
         .0002124421f - bass * 3.193462f + treble * .004186586f -
             bass * 13.71214f * treble + b2 * .694295f + bass * 14.98122f * t2 +
             b2 * 13.6246f * treble + b2 * 3.174203f - t2 * .003444054f -
             b2 * 14.88758f * t2 - bass * .698672f + .0009728595f,
         bass * .8872629f - .00013408015f - treble * .002619666f +
             bass * 3.810734f * treble - b2 * .193277f - bass * 4.980378f * t2 -
             b2 * 3.784927f * treble - b2 * .8815853f + t2 * .003427947f +
             b2 * 4.947003f * t2 + bass * .19456735f - .0006138986f},
        3u};
}

static float nonlinear(float input)
{
    float magnitude = fabsf(input);
    return copysignf(magnitude * 53530.f /
                         (magnitude * magnitude + magnitude * 54300.f + 1731.f),
                     input);
}

static float nonlinear_stage(gp100_bad_kt_od_amplifier_t *amplifier,
                             float input)
{
    float sample[5] = {input, amplifier->interpolation_history[0],
                       amplifier->interpolation_history[1],
                       amplifier->interpolation_history[2],
                       amplifier->interpolation_history[3]};
    float phase[8], output = 0.f;
    size_t i, j;
    for (i = 0u; i < 8u; ++i)
    {
        phase[i] = 0.f;
        for (j = 0u; j < 5u; ++j)
            phase[i] += sample[j] * interpolation[i][j];
        phase[i] = nonlinear(phase[i] * 8.f);
    }
    for (i = 3u; i > 0u; --i)
        amplifier->interpolation_history[i] =
            amplifier->interpolation_history[i - 1u];
    amplifier->interpolation_history[0] = input;
    memmove(&amplifier->reconstruction_history[8],
            amplifier->reconstruction_history, 32u * sizeof(float));
    for (i = 0u; i < 8u; ++i)
        amplifier->reconstruction_history[i] = phase[7u - i];
    for (i = 0u; i < 20u; ++i)
        output += (amplifier->reconstruction_history[i] +
                   amplifier->reconstruction_history[39u - i]) *
                  reconstruction[i];
    return output;
}

static bool valid(uint8_t gain, uint8_t presence, uint8_t master, uint8_t bass,
                  uint8_t edge, uint8_t treble)
{
    return gain <= 99u && presence <= 99u && master <= 99u && bass <= 99u &&
           edge <= 99u && treble <= 99u;
}

bool gp100_bad_kt_od_amplifier_reset(gp100_bad_kt_od_amplifier_t *a,
                                          uint8_t gain, uint8_t presence,
                                          uint8_t master, uint8_t bass,
                                          uint8_t edge, uint8_t treble)
{
    if (a == NULL || !valid(gain, presence, master, bass, edge, treble))
        return false;
    memset(a, 0, sizeof(*a));
    a->gain = a->gain_target = curve(gain_curve, gain);
    a->presence = curve(presence_edge_curve, presence);
    a->master = a->master_target = (float)master * .01f;
    a->bass = curve(bass_curve, bass);
    a->edge = curve(presence_edge_curve, edge);
    a->treble = a->treble_target = (float)treble * .01f;
    a->configured = true;
    return true;
}

bool gp100_bad_kt_od_amplifier_process(gp100_bad_kt_od_amplifier_t *a,
                                            uint8_t gain, uint8_t presence,
                                            uint8_t master, uint8_t bass,
                                            uint8_t edge, uint8_t treble,
                                            float *audio, size_t frames,
                                            bool enabled)
{
    static const bad_kt_od_coefficients_t input[2] = {
        {{1.221362114f, -2.402855396f, 1.181667089f, 0.f},
         {1.f, -1.663800836f, .7239171863f, 0.f},
         2u},
        {{2.402855396f, -1.508110881f, .7592300177f, 0.f},
         {1.f, -1.880879045f, .8856981993f, 0.f},
         2u}};
    static const bad_kt_od_coefficients_t output[3] = {
        {{.9523707032f, -1.620108843f, .6695178747f, 0.f},
         {1.f, -.8802073002f, .02858229913f, 0.f},
         2u},
        {{.545322001f, -.2398532927f, .1956516057f, 0.f},
         {1.f, -.2353616059f, .4880050123f, 0.f},
         2u},
        {{1.077116489f, -1.620108843f, .6376054287f, 0.f},
         {1.f, -1.830863237f, .8330222964f, 0.f},
         2u}};
    size_t n, i;
    if (a == NULL || !a->configured ||
        !valid(gain, presence, master, bass, edge, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    a->gain_target = curve(gain_curve, gain);
    a->presence = curve(presence_edge_curve, presence);
    a->master_target = (float)master * .01f;
    a->bass = curve(bass_curve, bass);
    a->edge = curve(presence_edge_curve, edge);
    a->treble_target = (float)treble * .01f;
    for (n = 0u; n < frames; ++n)
    {
        float signal, k, b0, a1;
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        signal = filter(&a->input_filter[0], input[0], audio[n * 2u]);
        signal = filter(&a->input_filter[1], input[1], signal) * a->gain;
        signal = nonlinear_stage(a, signal);
        signal = filter(&a->edge_filter, edge_coefficients(a->edge), signal);
        signal = filter(&a->tone_filter, tone_coefficients(a->bass, a->treble),
                        signal * .5f);
        for (i = 0u; i < 3u; ++i)
            signal = filter(&a->output_filter[i], output[i], signal);
        signal *= 30.f * a->master;
        k = tanf(((a->presence * 8000.f + 850.f) * 3.1415927f) / 44100.f);
        b0 = k / (k + 1.f);
        a1 = (k - 1.f) / (k + 1.f);
        signal = filter(&a->presence_filter,
                        (bad_kt_od_coefficients_t){
                            {b0, b0, 0.f, 0.f}, {1.f, a1, 0.f, 0.f}, 1u},
                        signal);
        signal *= 15.f * .1841f * 7.f * .5f * 1.1f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
