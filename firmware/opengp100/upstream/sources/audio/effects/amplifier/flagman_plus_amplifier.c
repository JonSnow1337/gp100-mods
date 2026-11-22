#include "flagman_plus_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4];
    float a[4];
    size_t order;
} coefficients_t;

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

static const uint16_t presence_curve[100] = {
    0,    0,    13,   28,   56,   79,   117,  146,  192,  224,  277,  333,
    372,  434,  476,  543,  589,  660,  709,  784,  862,  915,  997,  1053,
    1139, 1198, 1287, 1348, 1441, 1504, 1600, 1699, 1765, 1867, 1936, 2040,
    2111, 2219, 2291, 2402, 2514, 2590, 2705, 2783, 2901, 2980, 3101, 3182,
    3305, 3388, 3514, 3641, 3727, 3857, 3944, 4076, 4165, 4300, 4390, 4527,
    4666, 4759, 4899, 4994, 5137, 5233, 5378, 5475, 5623, 5722, 5871, 6022,
    6123, 6276, 6379, 6534, 6638, 6795, 6900, 7059, 7219, 7327, 7489, 7598,
    7762, 7872, 8038, 8150, 8318, 8430, 8600, 8771, 8886, 9059, 9175, 9349,
    9467, 9643, 9761, 9940};

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

static float filter(gp100_flagman_plus_filter_t *state,
                    coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;

    for (i = 0; i < coefficients.order; ++i)
        output += coefficients.b[i + 1] * state->x[i] -
                  coefficients.a[i + 1] * state->y[i];
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

static coefficients_t tone(float master, float bass, float middle, float treble)
{
    float t2 = treble * treble;
    float m2 = middle * middle;
    coefficients_t c = {{0}, {0}, 3};

    c.b[0] = master * (.0005297113f - .0231963f * m2 + .02118845f * bass +
                       .02362213f * middle + .02603876f * treble +
                       .9278518f * bass * middle + bass * treble -
                       .025f * middle * treble);
    c.b[1] = master * (-.0004572767f + .06607508f * m2 - .01829107f * bass -
                       .06643009f * middle - .07602267f * treble -
                       2.643003f * bass * middle - 3.f * bass * treble +
                       .075f * middle * treble);
    c.b[2] = master * (-.0005297113f - .06256128f * m2 - .02118845f * bass +
                       .06213545f * middle + .07396124f * treble +
                       2.502451f * bass * middle + 3.f * bass * treble -
                       .075f * middle * treble);
    c.b[3] = master * (.0004572767f + .01968249f * m2 + .01829107f * bass -
                       .01932748f * middle - .02397733f * treble -
                       .7872997f * bass * middle - bass * treble +
                       .025f * middle * treble);

    c.a[0] = .04171623f + 1.60339f * bass - .003864769f * middle +
             .01436126f * treble + 1.391493f * bass * middle +
             .5517906f * bass * treble - .004361764f * middle * treble -
             .5078084f * bass * t2 + .002835092f * middle * t2 -
             .009059105f * m2 * treble - .03478734f * m2 - .01321459f * t2 +
             .009469697f * m2 * t2 + .3623642f * bass * middle * treble -
             .3787879f * bass * middle * t2;
    c.a[1] = -.113305f - 4.478805f * bass + .01172484f * middle -
             .03922365f * treble - 3.96117f * bass * middle -
             1.550406f * bass * treble + .01295778f * middle * treble +
             1.507808f * bass * t2 - .008895698f * middle * t2 +
             .0254985f * m2 * treble + .09902925f * m2 + .03820654f * t2 -
             .02840909f * m2 * t2 - 1.01994f * bass * middle * treble +
             1.136364f * bass * middle * t2;
    c.a[2] = .1022881f + 4.156616f * bass - .01164432f * middle +
             .0356402f * treble + 3.748143f * bass * middle +
             1.448209f * bass * treble - .01275945f * middle * treble -
             1.492192f * bass * t2 + .00928612f * middle * t2 -
             .02381968f * m2 * treble - .09370357f * m2 - .03678541f * t2 +
             .02840909f * m2 * t2 + .9527873f * bass * middle * treble -
             1.136364f * bass * middle * t2;
    c.a[3] = -.03069065f - 1.281189f * bass + .003784248f * middle -
             .01077489f * treble - 1.178466f * bass * middle -
             .4495937f * bass * treble + .004163429f * middle * treble +
             .4921916f * bass * t2 - .003225514f * middle * t2 +
             .007380289f * m2 * treble + .02946166f * m2 + .01179346f * t2 -
             .009469697f * m2 * t2 - .2952115f * bass * middle * treble +
             .3787879f * bass * middle * t2;
    return c;
}

static coefficients_t presence(float value)
{
    float gain = powf(10.f, value * 6.25f / 40.f);
    float angle = ((4543.f - value * 1300.f) / 44100.f) * 6.2831855f;
    float resonance = sinf(angle) / .728f;
    return (coefficients_t){{resonance * gain + 1.f, -2.f * cosf(angle),
                             1.f - resonance * gain, 0.f},
                            {resonance / gain + 1.f, -2.f * cosf(angle),
                             1.f - resonance / gain, 0.f},
                            2};
}

static float shape(float input)
{
    float magnitude = fabsf(input);
    float p;
    float drive = input <= 0.f ? 2.8f : 3.f;
    unsigned i;

    if (magnitude >= .8f)
        return copysignf(.8f, input);
    p = 1.f - magnitude * drive / 1024.f;
    for (i = 0; i < 10; ++i)
        p *= p;
    return copysignf((1.f - p) * .8f, input);
}

static float nonlinear_stage(gp100_flagman_plus_amplifier_t *amplifier,
                             float input)
{
    float phase[4];
    float delta = (input - amplifier->previous_nonlinear_input) / 3.f;
    float output = 0.f;
    size_t i;

    phase[0] = amplifier->previous_nonlinear_input;
    phase[1] = phase[0] + delta;
    phase[2] = phase[0] + 2.f * delta;
    phase[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36 * sizeof(float));
    for (i = 0; i < 4; ++i)
        amplifier->fir_history[i] = shape(phase[3 - i]);
    for (i = 0; i < 40; ++i)
        output += amplifier->fir_history[i] * fir[i];
    amplifier->previous_nonlinear_input = input;
    return output;
}

static bool valid(uint8_t gain, uint8_t presence_value, uint8_t master,
                  uint8_t bass, uint8_t middle, uint8_t treble)
{
    return gain <= 99 && presence_value <= 99 && master <= 99 && bass <= 99 &&
           middle <= 99 && treble <= 99;
}

static void set_targets(gp100_flagman_plus_amplifier_t *amplifier,
                        uint8_t gain, uint8_t presence_value, uint8_t master,
                        uint8_t bass, uint8_t middle, uint8_t treble)
{
    amplifier->gain_target = curve(gain_curve, gain);
    amplifier->presence_target = curve(presence_curve, presence_value);
    amplifier->master_target = master * .01f;
    amplifier->bass_target = curve(bass_curve, bass);
    amplifier->middle_target = middle * .01f;
    amplifier->treble_target = treble * .01f;
}

bool gp100_flagman_plus_amplifier_reset(
    gp100_flagman_plus_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence_value, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble)
{
    if (!amplifier ||
        !valid(gain, presence_value, master, bass, middle, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    set_targets(amplifier, gain, presence_value, master, bass, middle, treble);
    amplifier->gain = amplifier->gain_target;
    amplifier->presence = amplifier->presence_target;
    amplifier->master = amplifier->master_target;
    amplifier->bass = amplifier->bass_target;
    amplifier->middle = amplifier->middle_target;
    amplifier->treble = amplifier->treble_target;
    amplifier->configured = true;
    return true;
}

bool gp100_flagman_plus_amplifier_process(
    gp100_flagman_plus_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence_value, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble, float *audio, size_t frames, bool enabled)
{
    static const coefficients_t input[4] = {
        {{8.151378632f, -16.12848091f, 7.978319168f, 0.f},
         {1.f, -1.918033361f, .9204468131f, 0.f},
         2},
        {{11.98622799f, -16.12848091f, 5.151795387f, 0.f},
         {1.f, -1.482053876f, .5490512848f, 0.f},
         2},
        {{16.12848091f, 14.57254314f, 3.341720581f, 0.f},
         {1.f, .6005560756f, .001375599997f, 0.f},
         2},
        {{.481744796f, -.9356322885f, .4538874924f, 0.f},
         {1.f, -1.869153023f, .8733757734f, 0.f},
         2}};
    static const coefficients_t anti_alias = {
        {.03138384071f, .06276768143f, .03138384071f, 0.f},
        {1.246609321f, -1.874464637f, .7533906794f, 0.f},
        2};
    size_t frame;
    size_t i;

    if (!amplifier || !amplifier->configured ||
        !valid(gain, presence_value, master, bass, middle, treble) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    set_targets(amplifier, gain, presence_value, master, bass, middle, treble);
    for (frame = 0; frame < frames; ++frame)
    {
        float sample;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;

        amplifier->presence = amplifier->presence_target;
        sample = audio[frame * 2];
        for (i = 0; i < 4; ++i)
            sample = filter(&amplifier->input_filter[i], input[i], sample);
        sample = filter(&amplifier->anti_alias_filter, anti_alias, sample);
        sample = nonlinear_stage(amplifier, sample * amplifier->gain);
        sample = filter(&amplifier->tone_filter,
                        tone(amplifier->master, amplifier->bass,
                             amplifier->middle, amplifier->treble),
                        sample);
        sample *= 5.f;
        sample = filter(&amplifier->presence_filter,
                        presence(amplifier->presence), sample);
        sample *= .2f * .8f * amplifier->master * 3.f;
        audio[frame * 2] = sample;
        audio[frame * 2 + 1] = sample;
    }
    return true;
}
