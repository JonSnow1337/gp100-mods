#include "juice_r100_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4];
    float a[4];
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

static const uint16_t bass_curve[100] = {
    0,    0,    9,    15,   25,   31,   41,   47,   57,   63,   73,   82,
    89,   98,   105,  114,  121,  130,  137,  146,  156,  162,  172,  178,
    188,  194,  204,  211,  221,  227,  237,  247,  254,  265,  272,  282,
    290,  301,  309,  321,  333,  341,  355,  364,  379,  389,  405,  416,
    434,  447,  468,  490,  506,  531,  550,  579,  600,  635,  659,  699,
    743,  775,  826,  863,  924,  967,  1038, 1089, 1171, 1231, 1327, 1433,
    1509, 1633, 1721, 1865, 1968, 2134, 2253, 2445, 2655, 2805, 3046, 3219,
    3496, 3694, 4012, 4239, 4602, 4861, 5276, 5723, 6042, 6550, 6911, 7487,
    7896, 8547, 9008, 9743};

static const float half_fir[20] = {
    -.0000417f, -.0005088f, -.0011711f, -.0021369f, -.0034084f,
    -.0048301f, -.006067f,  -.0066209f, -.0058887f, -.0032559f,
    .0017899f,  .0095427f,  .0199836f,  .0327264f,  .0470175f,
    .0617957f,  .0758057f,  .0877509f,  .0964619f,  .1010551f};

static const float interpolation_phases[8][5] = {
    {-.0000417f, -.0058887f, .0758057f, .0617957f, -.0066209f},
    {-.0011711f, .0017899f, .0964619f, .0327264f, -.0048301f},
    {-.0005088f, -.0032559f, .0877509f, .0470175f, -.006067f},
    {-.0021369f, .0095427f, .1010551f, .0199836f, -.0034084f},
    {-.0034084f, .0199836f, .1010551f, .0095427f, -.0021369f},
    {-.0048301f, .0327264f, .0964619f, .0017899f, -.0011711f},
    {-.006067f, .0470175f, .0877509f, -.0032559f, -.0005088f},
    {-.0066209f, .0617957f, .0758057f, -.0058887f, -.0000417f}};

static float curve(const uint16_t *table, uint8_t value)
{
    return (float)table[value] / 10000.f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_juice_r100_filter_t *state,
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

static coefficients_t tone(float bass, float middle, float treble)
{
    float bm = bass * middle;
    float bt = bass * treble;
    float mt = middle * treble;
    float m2 = middle * middle;
    return (coefficients_t){
        {bass * .01843456f + middle * .04042724f + treble * .05207505f +
             bm * .7942604f + bt - mt * .05f - m2 * .03971302f + .0009217279f,
         mt * .15f - middle * .1144228f - treble * .1520478f - bm * 2.276312f -
             bt * 3.f - bass * .01623976f + m2 * .1138156f - .0008119881f,
         middle * .1077779f - bass * .01843456f + treble * .1479249f +
             bm * 2.169842f + bt * 3.f - mt * .15f - m2 * .1084921f -
             .0009217279f,
         bass * .01623976f - middle * .03378234f - treble * .04795219f -
             bm * .6877909f - bt + mt * .05f + m2 * .03438954f + .0008119881f},
        {bass * 1.099419f - middle * .013622f + bm * .7942604f -
             m2 * .03971302f + .05721406f,
         middle * .03962645f - bass * 3.097225f - bm * 2.276312f +
             m2 * .1138156f - .1567388f,
         bass * 2.900581f - middle * .03817288f + bm * 2.169842f -
             m2 * .1084921f + .1427903f,
         middle * .01216842f - bass * .9027755f - bm * .6877909f +
             m2 * .03438954f - .0432568f},
        3};
}

static float saturate(float input)
{
    float magnitude = fabsf(input);
    float output = magnitude * .9842f / (magnitude + .06552f);
    return copysignf(output, input);
}

static void interpolate(gp100_juice_r100_amplifier_t *amplifier,
                        float input, float phase[8])
{
    size_t i;
    size_t j;

    memmove(&amplifier->input_history[1], amplifier->input_history,
            4 * sizeof(float));
    amplifier->input_history[0] = input;
    for (i = 0; i < 8; ++i)
    {
        phase[i] = 0.f;
        for (j = 0; j < 5; ++j)
            phase[i] +=
                amplifier->input_history[j] * interpolation_phases[i][j];
        phase[i] *= 8.f;
    }
}

static float delayed_decimator(gp100_juice_r100_amplifier_t *amplifier,
                               const float phase[8])
{
    float output = 0.f;
    size_t i;

    memmove(&amplifier->nonlinear_history[8], amplifier->nonlinear_history,
            32 * sizeof(float));
    for (i = 0; i < 20; ++i)
        output += (amplifier->nonlinear_history[i] +
                   amplifier->nonlinear_history[39 - i]) *
                  half_fir[i];
    for (i = 0; i < 8; ++i)
        amplifier->nonlinear_history[i] = saturate(phase[7 - i]);
    return output;
}

static bool valid(uint8_t gain, uint8_t master, uint8_t bass, uint8_t middle,
                  uint8_t treble)
{
    return gain <= 99 && master <= 99 && bass <= 99 && middle <= 99 &&
           treble <= 99;
}

static void set_targets(gp100_juice_r100_amplifier_t *amplifier,
                        uint8_t gain, uint8_t master, uint8_t bass,
                        uint8_t middle, uint8_t treble)
{
    amplifier->gain_target = curve(linear_curve, gain);
    amplifier->master_target = curve(linear_curve, master);
    amplifier->bass_target = curve(bass_curve, bass);
    amplifier->middle_target = curve(linear_curve, middle);
    amplifier->treble_target = curve(linear_curve, treble);
}

bool gp100_juice_r100_amplifier_reset(
    gp100_juice_r100_amplifier_t *amplifier, uint8_t gain, uint8_t master,
    uint8_t bass, uint8_t middle, uint8_t treble)
{
    if (!amplifier || !valid(gain, master, bass, middle, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    set_targets(amplifier, gain, master, bass, middle, treble);
    amplifier->gain = amplifier->gain_target;
    amplifier->master = amplifier->master_target;
    amplifier->bass = amplifier->bass_target;
    amplifier->middle = amplifier->middle_target;
    amplifier->treble = amplifier->treble_target;
    amplifier->configured = true;
    return true;
}

bool gp100_juice_r100_amplifier_process(
    gp100_juice_r100_amplifier_t *amplifier, uint8_t gain, uint8_t master,
    uint8_t bass, uint8_t middle, uint8_t treble, float *audio, size_t frames,
    bool enabled)
{
    static const coefficients_t input[3] = {
        {{.3892711997f, -.7767999768f, .3875307143f, 0.f},
         {1.f, -1.981898785f, .9820443988f, 0.f},
         2},
        {{.7767999768f, -.5849732757f, .03305919841f, 0.f},
         {1.f, -.4771570861f, .0186362993f, 0.f},
         2},
        {{.7767999768f, .4037587941f, .02768459916f, 0.f},
         {1.f, -.9290854931f, .1270388067f, 0.f},
         2}};
    size_t frame;
    size_t i;

    if (!amplifier || !amplifier->configured ||
        !valid(gain, master, bass, middle, treble) || (frames && !audio) ||
        frames > SIZE_MAX / 2)
        return false;
    set_targets(amplifier, gain, master, bass, middle, treble);
    for (frame = 0; frame < frames; ++frame)
    {
        float phase[8];
        float sample;

        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->bass = amplifier->bass_target;
        amplifier->middle = amplifier->middle_target;

        sample = audio[frame * 2] * 49.f * amplifier->gain;
        for (i = 0; i < 3; ++i)
            sample = filter(&amplifier->input_filter[i], input[i], sample);
        interpolate(amplifier, sample, phase);
        sample = delayed_decimator(amplifier, phase);
        sample =
            filter(&amplifier->tone_filter,
                   tone(amplifier->bass, amplifier->middle, amplifier->treble),
                   sample);
        sample *= amplifier->master * 1.353f * 1.16f;
        audio[frame * 2] = sample;
        audio[frame * 2 + 1] = sample;
    }
    return true;
}
