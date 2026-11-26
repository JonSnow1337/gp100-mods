#include "classic_bass_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[5];
    float a[5];
    size_t order;
} coefficients_t;

static const uint16_t linear_curve[100] = {
    0,    0,    119,  199,  318,  398,  517,  597,  717,  796,  916,  1035,
    1115, 1235, 1314, 1434, 1513, 1633, 1713, 1832, 1952, 2031, 2151, 2231,
    2350, 2430, 2549, 2629, 2749, 2828, 2948, 3067, 3147, 3266, 3346, 3466,
    3545, 3665, 3745, 3864, 3984, 4063, 4183, 4262, 4382, 4462, 4581, 4661,
    4780, 4860, 4980, 5099, 5179, 5298, 5378, 5498, 5577, 5697, 5776, 5896,
    6015, 6095, 6215, 6294, 6414, 6494, 6613, 6693, 6812, 6892, 7011, 7131,
    7211, 7330, 7410, 7529, 7609, 7729, 7808, 7928, 8047, 8127, 8247, 8326,
    8446, 8525, 8645, 8725, 8844, 8924, 9043, 9163, 9243, 9362, 9442, 9561,
    9641, 9760, 9840, 9960};

static const uint16_t treble_curve[100] = {
    6000, 6031, 6078, 6109, 6156, 6188, 6235, 6266, 6313, 6345, 6392, 6439,
    6470, 6517, 6549, 6596, 6627, 6674, 6705, 6752, 6800, 6831, 6878, 6909,
    6956, 6988, 7035, 7066, 7113, 7145, 7192, 7239, 7270, 7317, 7349, 7396,
    7427, 7474, 7505, 7552, 7600, 7631, 7678, 7709, 7756, 7788, 7835, 7866,
    7913, 7945, 7992, 8039, 8070, 8117, 8149, 8196, 8227, 8274, 8305, 8352,
    8400, 8431, 8478, 8509, 8556, 8588, 8635, 8666, 8713, 8745, 8792, 8839,
    8870, 8917, 8949, 8996, 9027, 9074, 9105, 9152, 9200, 9231, 9278, 9309,
    9356, 9388, 9435, 9466, 9513, 9545, 9592, 9639, 9670, 9717, 9749, 9796,
    9827, 9874, 9905, 9952};

static const uint16_t middle_curve[100] = {
    2000, 2054, 2137, 2192, 2274, 2329, 2411, 2466, 2549, 2603, 2686, 2768,
    2823, 2905, 2960, 3043, 3098, 3180, 3235, 3317, 3400, 3454, 3537, 3592,
    3674, 3729, 3811, 3866, 3949, 4003, 4086, 4168, 4223, 4305, 4360, 4443,
    4498, 4580, 4635, 4717, 4800, 4854, 4937, 4992, 5074, 5129, 5211, 5266,
    5349, 5403, 5486, 5568, 5623, 5705, 5760, 5843, 5898, 5980, 6035, 6117,
    6200, 6254, 6337, 6392, 6474, 6529, 6611, 6666, 6749, 6803, 6886, 6968,
    7023, 7105, 7160, 7243, 7298, 7380, 7435, 7517, 7600, 7654, 7737, 7792,
    7874, 7929, 8011, 8066, 8149, 8203, 8286, 8368, 8423, 8505, 8560, 8643,
    8698, 8780, 8835, 8917};

static float curve(const uint16_t *table, uint8_t value)
{
    return (float)table[value] / 10000.0f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_classic_bass_filter_t *state,
                    coefficients_t coefficients, float input)
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

static coefficients_t bass_treble(float bass, float treble)
{
    float b2 = bass * bass;
    float t2 = treble * treble;
    float b2t = b2 * treble;
    float b2t2 = b2 * t2;
    float bt2 = bass * t2;
    float bt = bass * treble;
    return (coefficients_t){
        {b2t2 * .3576554f - b2t * .9901497f - b2 * .02409363f -
             bt2 * .3613248f + bt + bass * .02433339f - t2 * .0003900035f +
             treble * .001103502f + .00002684688f,
         -b2t2 * 1.395564f + b2t * 3.700135f + b2 * .04146739f +
             bt2 * 1.402506f - bt * 3.717679f - bass * .04141089f +
             t2 * .0007712624f - treble * .001957108f + .000006487288f,
         b2t2 * 2.04076f - b2t * 5.175523f + b2 * .00629268f - bt2 * 2.040363f +
             bt * 5.173539f - bass * .006711057f + t2 * .000008744472f -
             treble * .0002460999f - .0000471047f,
         -b2t2 * 1.32545f + b2t * 3.211241f - b2 * .04146739f +
             bt2 * 1.318508f - bt * 3.193696f + bass * .04142014f -
             t2 * .0007712624f + treble * .001957108f - .000006283743f,
         b2t2 * .322598f - b2t * .7457028f + b2 * .01780095f - bt2 * .3193261f +
             bt * .7378357f - bass * .01761308f + t2 * .000381259f -
             treble * .0008574026f + .00002046137f},
        {b2t2 * 3.93421f - b2t * 4.788359f - b2 * .8198744f - bt2 * 3.972387f +
             bt * 4.834825f + bass * .8278304f - t2 * .004488776f +
             treble * .005463326f + .0009300925f,
         -b2t2 * 15.35121f + b2t * 16.97578f + b2 * 2.613023f +
             bt2 * 15.42756f - bt * 17.05214f - bass * 2.623247f +
             t2 * .008483887f - treble * .008376708f - .00110217f,
         b2t2 * 22.44836f - b2t * 22.36464f - b2 * 3.073154f - bt2 * 22.44836f +
             bt * 22.34806f + bass * 3.068397f + t2 * .0004936652f -
             treble * .002335586f - .0005811207f,
         -b2t2 * 14.57994f + b2t * 12.95537f + b2 * 1.577338f +
             bt2 * 14.50359f - bt * 12.87902f - bass * 1.567114f -
             t2 * .008483887f + treble * .008376708f + .001113661f,
         b2t2 * 3.548578f - b2t * 2.778153f - b2 * .2973329f - bt2 * 3.510401f +
             bt * 2.748264f + bass * .2941341f + t2 * .003995111f -
             treble * .00312774f - .0003374807f},
        4};
}

static coefficients_t treble_stage(float treble)
{
    float t2 = treble * treble;
    float p = t2 * .9986679f - treble * 1.797602f;
    float n = -t2 * .9986679f + treble * 1.797602f;
    return (coefficients_t){
        {p - 2.077986f, n + 5.148375f, n - 4.968615f, p + 1.898225f, 0.f},
        {t2 + treble - 4.080757f,
         -t2 * .9973358f - treble * .9973358f + 7.144371f,
         -t2 - treble - 2.967052f,
         t2 * .9973358f + treble * .9973358f - .09897823f, 0.f},
        3};
}

static coefficients_t middle_stage(float middle, uint8_t range)
{
    float m2 = middle * middle;
    float p;
    float n;
    if (range == 0)
    {
        p = m2 * .9986679f - middle * 1.567909f;
        n = -m2 * .9986679f + middle * 1.567909f;
        return (coefficients_t){
            {p - 7.535523f, n + 22.5064f, n - 22.41253f, p + 7.441649f, 0.f},
            {m2 - middle * .53f - 8.015575f,
             -m2 * .9973358f + middle * .528588f + 22.98507f,
             middle * .53f - m2 - 21.93249f,
             m2 * .9973358f - middle * .528588f + 6.962975f, 0.f},
            3};
    }
    if (range <= 2)
    {
        p = m2 * .9986679f - middle * 1.587882f;
        n = -m2 * .9986679f + middle * 1.587882f;
        if (range == 1)
            return (coefficients_t){
                {p - 7.549914f, n + 22.50799f, n - 22.39814f, p + 7.44006f,
                 0.f},
                {m2 - middle * .49f - 8.069984f,
                 -m2 * .9973358f + middle * .4886946f + 23.02652f,
                 middle * .49f - m2 - 21.87811f,
                 m2 * .9973358f - middle * .4886946f + 6.921495f, 0.f},
                3};
        return (coefficients_t){
            {p - 7.568272f, n + 22.48963f, n - 22.37978f, p + 7.458419f, 0.f},
            {m2 - middle * .49f - 8.088367f,
             -m2 * .9973358f + middle * .4886946f + 23.00808f,
             middle * .49f - m2 - 21.85983f,
             m2 * .9973358f - middle * .4886946f + 6.939829f, 0.f},
            3};
    }
    p = m2 * .9986679f - middle * 1.797602f;
    n = -m2 * .9986679f + middle * 1.797602f;
    if (range == 3)
        return (coefficients_t){
            {p - 5.445598f, n + 15.87396f, n - 15.6942f, p + 5.265838f, 0.f},
            {m2 - 6.452862f, 16.87795f - m2 * .9973358f, -m2 - 14.68732f,
             m2 * .9973358f + 4.261478f, 0.f},
            3};
    return (coefficients_t){
        {p - 1.927015f, n + 5.299346f, n - 5.119585f, p + 1.747255f, 0.f},
        {m2 + middle - 3.929586f,
         -m2 * .9973358f - middle * .9973358f + 7.295945f,
         -m2 - middle - 3.117418f,
         m2 * .9973358f + middle * .9973358f - .2497471f, 0.f},
        3};
}

static bool valid(uint8_t gain, uint8_t bass, uint8_t middle,
                  uint8_t middle_range, uint8_t treble, uint8_t master)
{
    return gain <= 99 && bass <= 99 && middle <= 99 && middle_range <= 4 &&
           treble <= 99 && master <= 99;
}

bool gp100_classic_bass_amplifier_reset(
    gp100_classic_bass_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t middle_range, uint8_t treble, uint8_t master)
{
    (void)bass;
    (void)middle;
    (void)middle_range;
    (void)treble;
    if (!amplifier || !valid(gain, bass, middle, middle_range, treble, master))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = curve(linear_curve, gain);
    amplifier->master = amplifier->master_target = curve(linear_curve, master);
    amplifier->configured = true;
    return true;
}

bool gp100_classic_bass_amplifier_process(
    gp100_classic_bass_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t middle_range, uint8_t treble, uint8_t master,
    float *audio, size_t frames, bool enabled)
{
    static const coefficients_t input[2] = {
        {{.7322989106f, -1.171834350f, .4405593872f, 0, 0},
         {1, -1.814524055f, .8153988719f, 0, 0},
         2},
        {{1.132598758f, -1.171834350f, .2271219939f, 0, 0},
         {1, -.8842405081f, .1811192036f, 0, 0},
         2}};
    coefficients_t bt;
    coefficients_t ts;
    coefficients_t ms;
    size_t frame;
    if (!amplifier || !amplifier->configured ||
        !valid(gain, bass, middle, middle_range, treble, master) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    amplifier->gain_target = curve(linear_curve, gain);
    amplifier->master_target = curve(linear_curve, master);
    bt = bass_treble(curve(linear_curve, bass), curve(linear_curve, treble));
    ts = treble_stage(curve(treble_curve, treble));
    ms = middle_stage(curve(middle_curve, middle), middle_range);
    for (frame = 0; frame < frames; ++frame)
    {
        float sample;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        if (!enabled)
            continue;
        sample = audio[frame * 2] * 70.f * amplifier->gain;
        sample = filter(&amplifier->input_filter[0], input[0], sample);
        sample = filter(&amplifier->input_filter[1], input[1], sample);
        sample = filter(&amplifier->bass_treble_filter, bt, sample);
        sample = filter(&amplifier->treble_filter, ts, sample);
        sample = filter(&amplifier->middle_filter, ms, sample);
        sample *= amplifier->master * .3f * .35f * 3.f;
        audio[frame * 2] = sample;
        audio[frame * 2 + 1] = sample;
    }
    return true;
}
