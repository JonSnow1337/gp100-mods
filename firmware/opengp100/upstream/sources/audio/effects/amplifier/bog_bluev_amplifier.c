#include "bog_bluev_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;

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

static const uint16_t shaping_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};

static const float fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float curve(const uint16_t *table, uint8_t value)
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

static float filter(gp100_bog_bluev_filter_t *state, coefficients_t c,
                    float input)
{
    float output = c.b[0] * input;
    size_t i;
    for (i = 0; i < c.order; ++i)
        output += c.b[i + 1] * state->x[i] - c.a[i + 1] * state->y[i];
    output /= c.a[0];
    for (i = c.order; i > 1; --i)
    {
        state->x[i - 1] = state->x[i - 2];
        state->y[i - 1] = state->y[i - 2];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static coefficients_t gain_filter(float gain)
{
    return (coefficients_t){{gain * .9951985f, -gain * .9951985f, 0, 0},
                            {1.f, -.9903971f, 0, 0},
                            1};
}

static coefficients_t anti_alias(void)
{
    float angle = .170971036f;
    float sine = sinf(angle);
    float cosine = cosf(angle);
    return (coefficients_t){
        {(1.f - cosine) * .5f, 1.f - cosine, (1.f - cosine) * .5f, 0},
        {sine / 1.414f + 1.f, -2.f * cosine, 1.f - sine / 1.414f, 0},
        2};
}

static coefficients_t tone(float b, float m, float t)
{
    float t2 = t * t, bm = b * m, bt = b * t, mt = m * t;
    return (coefficients_t){
        {b * .02127887f + m * .000960067f + t * .001038764f + bm * .9322378f +
             bt,
         -b * .01820065f - m * .000807766f - t * .001022667f - bm * 2.647389f -
             bt * 3.f,
         bm * 2.498065f - m * .000960067f - t * .001038764f - b * .02127887f +
             bt * 3.f,
         b * .01820065f + m * .000807766f + t * .001022667f - bm * .7829137f -
             bt},
        {b * 1.120049f + m * .001017017f + t * .0005694982f + bm * .9879186f +
             bt * .5547306f + mt * .0003761571f - b * .5078084f * t2 -
             m * .0003904225f * t2 - t2 * .0005193818f + bm * t * .3645572f -
             bm * .3787879f * t2 + .001138996f,
         b * 1.507808f * t2 - m * .00085381f - t * .0004604405f -
             bm * 2.802919f - bt * 1.553256f - mt * .0003016163f -
             b * 3.116535f + m * .0003904225f * t2 + t2 * .0005113335f -
             bm * t * 1.022133f + bm * 1.136364f * t2 - .0009208809f,
         b * 2.879957f - m * .001016861f - t * .0005679451f + bm * 2.642384f +
             bt * 1.445269f - mt * .0003761571f - b * 1.492192f * t2 +
             m * .0003904225f * t2 + t2 * .0005193818f + bm * t * .9505943f -
             bm * 1.136364f * t2 - .00113589f,
         m * .0008539653f - b * .8834589f + t * .0004619937f - bm * .8273836f -
             bt * .4467441f + mt * .0003016163f + b * .4921916f * t2 -
             m * .0003904225f * t2 - t2 * .0005113335f - bm * t * .2930185f +
             bm * .3787879f * t2 + .0009239873f},
        3};
}

static coefficients_t presence(float value)
{
    float hi, lo, kh, kl, hf, hr, lf, lr, p0, p1, q0, q1, r0, r1, d;
    if (value == 0.f)
        value = .01f;
    hi = powf(10.f, value * 5.f / 20.f);
    lo = powf(10.f, -value * 3.f / 20.f);
    kh = tanf(.06411413848f);
    kl = tanf(.009260931052f);
    hf = 2.f / (hi + 1.f);
    hr = 2.f / (1.f / hi + 1.f);
    lf = 2.f / (lo + 1.f);
    lr = 2.f / (1.f / lo + 1.f);
    p0 = kh + hf;
    p1 = kh - hf;
    q0 = kl * lr + 1.f;
    q1 = kl * lr - 1.f;
    r0 = kl * lf + 1.f;
    r1 = kl * lf - 1.f;
    d = p0 * r0;
    return (coefficients_t){{(kh + hr) * q1 / d,
                             ((kh + hr) * q0 + (kh - hr) * q1) / d,
                             (kh - hr) * q0 / d, 0},
                            {1.f, (p0 * r1 + p1 * r0) / d, p1 * r1 / d, 0},
                            2};
}

static float shape(float input)
{
    float magnitude = fabsf(input), p, drive = input <= 0.f ? 6.f : 4.f;
    unsigned i;
    if (magnitude >= 1.5f)
        return copysignf(1.5f, input);
    p = 1.f - magnitude * drive / 1024.f;
    for (i = 0; i < 10; ++i)
        p *= p;
    return copysignf((1.f - p) * 1.5f, input);
}

static float nonlinear_stage(gp100_bog_bluev_amplifier_t *a, float input)
{
    float point[4], step = (input - a->previous_nonlinear_input) / 3.f,
                    output = 0.f;
    size_t i;
    point[0] = a->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + 2.f * step;
    point[3] = input;
    memmove(&a->fir_history[4], a->fir_history, 36 * sizeof(float));
    for (i = 0; i < 4; ++i)
        a->fir_history[i] = shape(point[3 - i]);
    for (i = 0; i < 40; ++i)
        output += a->fir_history[i] * fir[i];
    a->previous_nonlinear_input = input;
    return output;
}

static bool valid(uint8_t g, uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && p <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}

static void targets(gp100_bog_bluev_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(shaping_curve, g);
    a->presence_target = curve(presence_curve, p);
    a->master_target = o * .01f;
    a->bass_target = curve(shaping_curve, b);
    a->middle_target = m * .01f;
    a->treble_target = curve(shaping_curve, t);
}

bool gp100_bog_bluev_amplifier_reset(gp100_bog_bluev_amplifier_t *a,
                                          uint8_t g, uint8_t p, uint8_t o,
                                          uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || !valid(g, p, o, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, p, o, b, m, t);
    a->gain = a->gain_target;
    a->presence = a->presence_target;
    a->master = a->master_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->configured = true;
    return true;
}

bool gp100_bog_bluev_amplifier_process(gp100_bog_bluev_amplifier_t *a,
                                            uint8_t g, uint8_t p, uint8_t o,
                                            uint8_t b, uint8_t m, uint8_t t,
                                            float *audio, size_t frames,
                                            bool enabled)
{
    static const coefficients_t input[2] = {
        {{18.12818718f, -35.05553436f, 16.94722939f, 0},
         {1.f, -1.766971231f, .7805444002f, 0},
         2},
        {{35.05553436f, 18.16394615f, .006391800009f, 0},
         {1.f, -.1057839021f, .000008300000445f, 0},
         2}};
    static const coefficients_t fixed_first = {
        {.3024907112f, -.274646014f, 0, 0}, {1.f, -.8050872087f, 0, 0}, 1};
    static const coefficients_t voicing = {
        {1.042600036f, -1.986999989f, .9574000239f, 0},
        {1.075799942f, -1.986999989f, .9241999984f, 0},
        2};
    size_t n, i;
    if (!a || !a->configured || !valid(g, p, o, b, m, t) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, p, o, b, m, t);
    for (n = 0; n < frames; ++n)
    {
        float x;
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        a->presence = a->presence_target;
        x = audio[n * 2];
        for (i = 0; i < 2; ++i)
            x = filter(&a->input_filter[i], input[i], x);
        x = filter(&a->gain_filter, gain_filter(a->gain), x);
        x = filter(&a->fixed_filter, fixed_first, x);
        x = filter(&a->voicing_filter, voicing, x);
        x = filter(&a->anti_alias_filter, anti_alias(), x);
        x = nonlinear_stage(a, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x);
        x *= a->master;
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= .2692f * 6.f * .9f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
