#include "lstar_od_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct lstar_od_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} lstar_od_coefficients_t;

static const uint16_t gain_curve[100] = {
    0,    0,    13,   28,   56,   79,   117,  146,  192,  224,  277,  333,
    372,  434,  476,  543,  589,  660,  709,  784,  862,  915,  997,  1053,
    1139, 1198, 1287, 1348, 1441, 1504, 1600, 1699, 1765, 1867, 1936, 2040,
    2111, 2219, 2291, 2402, 2514, 2590, 2705, 2783, 2901, 2980, 3101, 3182,
    3305, 3388, 3514, 3641, 3727, 3857, 3944, 4076, 4165, 4300, 4390, 4527,
    4666, 4759, 4899, 4994, 5137, 5233, 5378, 5475, 5623, 5722, 5871, 6022,
    6123, 6276, 6379, 6534, 6638, 6795, 6900, 7059, 7219, 7327, 7489, 7598,
    7762, 7872, 8038, 8150, 8318, 8430, 8600, 8771, 8886, 9059, 9175, 9349,
    9467, 9643, 9761, 9940};

static const uint16_t master_curve[100] = {
    0,    0,    0,    0,    1,    3,    6,    8,    13,   17,   25,   34,
    41,   53,   62,   77,   89,   107,  121,  143,  168,  186,  214,  235,
    267,  291,  328,  354,  396,  425,  471,  521,  555,  610,  647,  707,
    748,  813,  858,  928,  1001, 1052, 1131, 1186, 1271, 1330, 1420, 1483,
    1580, 1647, 1750, 1857, 1930, 2043, 2121, 2241, 2323, 2449, 2536, 2669,
    2807, 2901, 3045, 3143, 3295, 3398, 3557, 3665, 3830, 3943, 4117, 4294,
    4415, 4601, 4727, 4920, 5051, 5251, 5388, 5596, 5810, 5955, 6176, 6326,
    6556, 6712, 6949, 7110, 7357, 7523, 7778, 8037, 8213, 8481, 8663, 8940,
    9127, 9413, 9606, 9900};

static const uint16_t presence_treble_curve[100] = {
    0,    0,    0,    0,    0,    0,    1,    2,    3,    5,    7,    11,
    13,   18,   22,   29,   34,   43,   50,   61,   74,   83,   99,   111,
    129,  143,  165,  181,  207,  226,  256,  288,  311,  348,  374,  416,
    445,  492,  525,  577,  632,  671,  732,  774,  841,  888,  961,  1012,
    1092, 1148, 1235, 1326, 1389, 1487, 1555, 1661, 1735, 1849, 1927, 2050,
    2177, 2264, 2400, 2494, 2639, 2738, 2892, 2998, 3162, 3274, 3447, 3626,
    3749, 3939, 4069, 4269, 4406, 4617, 4761, 4983, 5212, 5368, 5609, 5773,
    6025, 6197, 6461, 6642, 6918, 7107, 7397, 7694, 7896, 8207, 8418, 8742,
    8962, 9299, 9529, 9881};

static const uint16_t bass_curve[100] = {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    1,    1,    1,    2,    3,    4,    5,    7,    8,    10,   12,
    16,   18,   22,   26,   31,   35,   43,   51,   57,   67,   75,   87,
    96,   112,  123,  141,  161,  176,  200,  218,  246,  267,  300,  324,
    362,  389,  434,  482,  516,  571,  610,  673,  717,  788,  838,  918,
    1003, 1064, 1160, 1227, 1334, 1410, 1529, 1612, 1744, 1837, 1983, 2138,
    2247, 2417, 2536, 2724, 2854, 3060, 3203, 3427, 3663, 3828, 4085, 4264,
    4543, 4738, 5041, 5252, 5580, 5808, 6163, 6535, 6792, 7193, 7470, 7902,
    8200, 8663, 8983, 9481};

static const float decimator_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

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

static float filter(gp100_lstar_od_filter_t *state,
                    lstar_od_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0u; i < coefficients.order; ++i)
    {
        output += coefficients.b[i + 1u] * state->x[i] -
                  coefficients.a[i + 1u] * state->y[i];
    }
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

static lstar_od_coefficients_t tone(float bass, float middle, float treble)
{
    float bass_middle = bass * middle;
    float bass_treble = bass * treble;
    return (lstar_od_coefficients_t){
        {bass * .004269454f + middle * .0002243567f + treble * .001419534f +
             bass_middle * .1581859f + bass_treble,
         bass * -.00339205f - middle * .0001728065f - treble * .001417346f -
             bass_middle * .4381859f - bass_treble * 3.f,
         bass_middle * .4018141f - middle * .0002243567f -
             treble * .001419534f - bass * .004269454f + bass_treble * 3.f,
         bass * .00339205f + middle * .0001728065f + treble * .001417346f -
             bass_middle * .1218141f - bass_treble},
        {bass * 1.185675f + middle * .0002243567f + bass_middle * .1581859f +
             .001677046f,
         bass * -3.184798f - middle * .0001728065f - bass_middle * .4381859f -
             .001159437f,
         bass * 2.814325f - middle * .0002243567f + bass_middle * .4018141f -
             .001676252f,
         middle * .0001728065f - bass * .8152021f - bass_middle * .1218141f +
             .001160231f},
        3u};
}

static lstar_od_coefficients_t gain_voicing(float gain)
{
    float square = gain * gain;
    return (lstar_od_coefficients_t){
        {square - gain * 1.176482f, gain * .9875178f - square, 0.f, 0.f},
        {(square - gain) - .1764822f, (gain - square) - .01248224f, 0.f, 0.f},
        1u};
}

static lstar_od_coefficients_t master_presence(float master, float presence)
{
    float square = master * master;
    return (lstar_od_coefficients_t){
        {master * -.0101231f - master * presence, master * -.02024619f,
         master * presence - master * .0101231f, 0.f},
        {square * 9.45f * presence - presence - master * 9.45f * presence -
             master * .8928571f + square * .8928571f - .0101231f,
         presence * -18.9f * square + presence * 18.9f * master - .02024619f,
         presence + master * .8928571f - master * 9.45f * presence +
             square * 9.45f * presence - square * .8928571f - .0101231f,
         0.f},
        2u};
}

static float shaped(float input, float drive)
{
    return copysignf((1.f - expf(-drive * fabsf(input))) * .1f, input);
}

static float nonlinear_stage(gp100_lstar_od_amplifier_t *amplifier,
                             float input, float envelope)
{
    float point[4];
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float drive = envelope * 17.f + 5.f;
    float scale = 1.f / (drive * 3.f + 1.f);
    float output = 0.f;
    size_t i;
    if (scale < .04f)
        scale = .04f;
    point[0] = amplifier->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.f;
    point[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
    {
        amplifier->fir_history[i] = shaped(point[3u - i], drive) * scale;
    }
    for (i = 0u; i < 40u; ++i)
        output += amplifier->fir_history[i] * decimator_fir[i];
    amplifier->previous_nonlinear_input = input;
    return output;
}

static bool valid(uint8_t gain, uint8_t presence, uint8_t master, uint8_t bass,
                  uint8_t middle, uint8_t treble)
{
    return gain <= 99u && presence <= 99u && master <= 99u && bass <= 99u &&
           middle <= 99u && treble <= 99u;
}

static void set_targets(gp100_lstar_od_amplifier_t *amplifier,
                        uint8_t gain, uint8_t presence, uint8_t master,
                        uint8_t bass, uint8_t middle, uint8_t treble)
{
    amplifier->gain_target = curve(gain_curve, gain);
    amplifier->presence_target = curve(presence_treble_curve, presence);
    amplifier->master_target = curve(master_curve, master);
    amplifier->bass_target = curve(bass_curve, bass);
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = curve(presence_treble_curve, treble);
}

bool gp100_lstar_od_amplifier_reset(
    gp100_lstar_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble)
{
    if (amplifier == NULL ||
        !valid(gain, presence, master, bass, middle, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    set_targets(amplifier, gain, presence, master, bass, middle, treble);
    amplifier->gain = amplifier->gain_target;
    amplifier->presence = amplifier->presence_target;
    amplifier->master = amplifier->master_target;
    amplifier->bass = amplifier->bass_target;
    amplifier->middle = amplifier->middle_target;
    amplifier->treble = amplifier->treble_target;
    amplifier->configured = true;
    return true;
}

bool gp100_lstar_od_amplifier_process(
    gp100_lstar_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble, float *audio,
    size_t frames, bool enabled)
{
    static const lstar_od_coefficients_t input = {
        {61.19266891f, -114.8033981f, 53.62115097f, 0.f},
        {1.f, -1.874851108f, .8751769066f, 0.f},
        2u};
    static const lstar_od_coefficients_t conditioning = {
        {.002586051356f, -.002586051356f, 0.f, 0.f},
        {.1962451041f, -.1916102469f, 0.f, 0.f},
        1u};
    static const float pole = .9990040064f;
    const lstar_od_coefficients_t envelope_coefficients = {
        {(1.f - pole) * (1.f - pole), 0.f, 0.f, 0.f},
        {1.f, -2.f * pole, pole * pole, 0.f},
        2u};
    size_t n;
    if (amplifier == NULL || !amplifier->configured ||
        !valid(gain, presence, master, bass, middle, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    set_targets(amplifier, gain, presence, master, bass, middle, treble);
    for (n = 0u; n < frames; ++n)
    {
        float signal;
        float envelope;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->presence =
            glide(amplifier->presence, amplifier->presence_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        signal = filter(&amplifier->input_filter, input, audio[n * 2u]);
        signal = filter(&amplifier->conditioning_filter, conditioning, signal) *
                 30.f;
        signal =
            filter(&amplifier->tone_filter,
                   tone(amplifier->bass, amplifier->middle, amplifier->treble),
                   signal);
        signal = filter(&amplifier->gain_filter, gain_voicing(amplifier->gain),
                        signal) *
                 25.f;
        signal = filter(&amplifier->master_presence_filter,
                        master_presence(amplifier->master, amplifier->presence),
                        signal);
        envelope = filter(&amplifier->envelope_filter, envelope_coefficients,
                          fabsf(signal));
        signal =
            nonlinear_stage(amplifier, signal, envelope) * 130.f * .1585f * 7.f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
