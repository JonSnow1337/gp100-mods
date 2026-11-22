#include "knights_cl_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct knights_coefficients
{
    float b[4], a[4];
    size_t order;
} knights_coefficients_t;

static const uint16_t gain_bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};

static const uint16_t middle_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float curve(const uint16_t table[100], uint8_t value)
{
    return (float)table[value] * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_knights_cl_filter_t *state,
                    knights_coefficients_t c, float input)
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

static knights_coefficients_t gain_coefficients(float gain)
{
    float square = gain * gain;
    return (knights_coefficients_t){
        {square - gain * 1.188964f, square * -.1671534f - gain * .1791895f,
         square * -.8328466f + gain * .6754682f, 0.f},
        {square - gain * .9019677f - .3055214f,
         square * -.1671534f + gain * .151441f - .3647106f,
         square * -.8328466f + gain * .7505267f - .0906141f, 0.f},
        2u};
}

static knights_coefficients_t tone_coefficients(float bass, float middle,
                                                float treble)
{
    float t2 = treble * treble, m2 = middle * middle;
    float bm = bass * middle, bt = bass * treble, mt = middle * treble;
    return (knights_coefficients_t){
        {bass * .01405697f + middle * .009279359f + treble * .02103462f +
             bm * .4546138f + bt - mt * .02f - m2 * .009092276f + .0002811394f,
         mt * .06f - middle * .02460941f - treble * .06102681f -
             bm * 1.224667f - bt * 3.f - bass * .01047075f + m2 * .02449335f -
             .0002094149f,
         middle * .02152278f - bass * .01405697f + treble * .05896538f +
             bm * 1.085493f + bt * 3.f - mt * .06f - m2 * .02170986f -
             .0002811394f,
         bass * .01047075f - middle * .006192725f - treble * .01897319f -
             bm * .3154397f - bt + mt * .02f + m2 * .006308793f + .0002094149f},
        {bass * 3.92096f - middle * .04605681f + treble * .02605508f +
             bm * 1.538889f + bt * 1.238905f - mt * .01906963f -
             bass * t2 * 1.007579f + middle * t2 * .01396607f -
             m2 * treble * .005428353f - m2 * .03077779f - t2 * .0211862f +
             m2 * t2 * .005882353f + bm * treble * .2714177f -
             bass * middle * t2 * .2941176f + .08242429f,
         middle * .1198008f - bass * 10.18019f - treble * .06549041f -
             bm * 4.135553f - bt * 3.23539f + mt * .05101231f +
             bass * t2 * 3.007579f - middle * t2 * .04220137f +
             m2 * treble * .0135567f + m2 * .08271105f + t2 * .06117839f -
             m2 * t2 * .01764706f - bm * treble * .6778348f +
             bass * middle * t2 * .8823529f - .2060584f,
         bass * 8.624511f - middle * .1009728f + treble * .05394858f +
             bm * 3.655082f + bt * 2.761095f - mt * .04467368f -
             bass * t2 * 2.992421f + middle * t2 * .04250452f -
             m2 * treble * .01082833f - m2 * .07310165f - t2 * .0588138f +
             m2 * t2 * .01764706f + bm * treble * .5414166f -
             bass * middle * t2 * .8823529f + .1684966f,
         middle * .02722884f - bass * 2.365245f - treble * .01450593f -
             bm * 1.058419f - bt * .7646104f + mt * .01273101f +
             bass * t2 * .9924212f - middle * t2 * .01426922f +
             m2 * treble * .002699989f + m2 * .02116839f + t2 * .01882161f -
             m2 * t2 * .005882353f - bm * treble * .1349995f +
             bass * middle * t2 * .2941176f - .04483886f},
        3u};
}

static float nonlinear(float input)
{
    float magnitude = fabsf(input), power, scale;
    unsigned i;
    scale = input >= 0.f ? 3.f : 2.8f;
    if (magnitude >= scale)
        return copysignf(scale, input);
    power = 1.f - magnitude * 3.f / 1024.f;
    for (i = 0u; i < 10u; ++i)
        power *= power;
    return copysignf((1.f - power) * scale, input);
}

static float power_stage(gp100_knights_cl_amplifier_t *a, float input)
{
    float point[4], step = (input - a->previous_nonlinear_input) / 3.f,
                    output = 0.f;
    size_t i;
    point[0] = a->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.f;
    point[3] = input;
    memmove(&a->fir_history[4], a->fir_history, 36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
        a->fir_history[i] = nonlinear(point[3u - i]);
    for (i = 0u; i < 40u; ++i)
        output += a->fir_history[i] * reconstruction_fir[i];
    a->previous_nonlinear_input = input;
    return output;
}

bool gp100_knights_cl_amplifier_reset(gp100_knights_cl_amplifier_t *a,
                                           uint8_t gain, uint8_t volume,
                                           uint8_t bass, uint8_t middle,
                                           uint8_t treble)
{
    if (a == NULL || gain > 99u || volume > 99u || bass > 99u || middle > 99u ||
        treble > 99u)
        return false;
    memset(a, 0, sizeof(*a));
    a->gain = a->gain_target = curve(gain_bass_curve, gain) * .35f;
    a->volume = a->volume_target = (float)volume * .01f;
    a->bass = a->bass_target = curve(gain_bass_curve, bass);
    a->middle = a->middle_target = curve(middle_curve, middle);
    a->treble = a->treble_target = (float)treble * .01f;
    a->configured = true;
    return true;
}

bool gp100_knights_cl_amplifier_process(
    gp100_knights_cl_amplifier_t *a, uint8_t gain, uint8_t volume,
    uint8_t bass, uint8_t middle, uint8_t treble, float *audio, size_t frames,
    bool enabled)
{
    static const knights_coefficients_t voicing[3] = {
        {{5.01529455f, -8.98969746f, 3.97553039f, 0.f},
         {1.f, -1.79432595f, .795163929f, 0.f},
         2u},
        {{8.98969746f, 8.55008507f, 1.57158184f, 0.f},
         {1.f, .932865798f, .165073692f, 0.f},
         2u},
        {{1.f, -1.99989998f, 1.f, 0.f},
         {1.01900005f, -1.99989998f, .981000006f, 0.f},
         2u}};
    size_t n, i;
    if (a == NULL || !a->configured || gain > 99u || volume > 99u ||
        bass > 99u || middle > 99u || treble > 99u ||
        (frames && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    a->gain_target = curve(gain_bass_curve, gain) * .35f;
    a->volume_target = (float)volume * .01f;
    a->bass_target = curve(gain_bass_curve, bass);
    a->middle_target = curve(middle_curve, middle);
    a->treble_target = (float)treble * .01f;
    for (n = 0u; n < frames; ++n)
    {
        float signal;
        a->gain = glide(a->gain, a->gain_target);
        a->volume = glide(a->volume, a->volume_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        signal = audio[n * 2u];
        for (i = 0u; i < 3u; ++i)
            signal = filter(&a->voicing[i], voicing[i], signal);
        signal = filter(&a->gain_filter, gain_coefficients(a->gain), signal);
        signal = power_stage(a, signal);
        signal =
            filter(&a->tone_filter,
                   tone_coefficients(a->bass, a->middle, a->treble), signal);
        signal *= a->volume * 15.f * .1318f * 1.5f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
