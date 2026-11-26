#include "match_cl_amplifier.h"
#include "match_family_internal.h"

#include <math.h>
#include <string.h>

typedef struct match_cl_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} match_cl_coefficients_t;

enum match_cl_curve
{
    MATCH_CL_GAIN,
    MATCH_CL_PRESENCE,
    MATCH_CL_BASS,
    MATCH_CL_MIDDLE,
    MATCH_CL_TREBLE
};

/* UI-reachable entries from the five 256-entry factory tapers, stored as
 * exact Q0.0001 integers. */
static const uint16_t factory_curve[5][100] = {
    {0,    0,    23,   39,   63,   79,   103,  119,  143,  159,  183,  207,
     223,  247,  262,  286,  302,  326,  342,  366,  390,  406,  430,  446,
     470,  486,  510,  526,  550,  566,  589,  614,  630,  654,  670,  694,
     710,  734,  750,  775,  799,  816,  840,  857,  882,  898,  924,  940,
     966,  983,  1009, 1036, 1054, 1081, 1099, 1127, 1146, 1174, 1194, 1224,
     1254, 1275, 1308, 1330, 1363, 1387, 1423, 1447, 1486, 1512, 1553, 1596,
     1626, 1672, 1704, 1755, 1790, 1845, 1883, 1943, 2006, 2050, 2120, 2169,
     2246, 2299, 2384, 2443, 2537, 2603, 2707, 2817, 2895, 3017, 3103, 3239,
     3334, 3484, 3590, 3757},
    {0,    0,    739,  1203, 1858, 2269, 2850, 3214, 3726, 4047, 4499, 4918,
     5180, 5547, 5777, 6099, 6300, 6581, 6757, 7002, 7228, 7368, 7564, 7686,
     7856, 7962, 8109, 8200, 8327, 8405, 8515, 8615, 8677, 8763, 8816, 8890,
     8935, 8999, 9038, 9092, 9142, 9173, 9216, 9243, 9280, 9303, 9335, 9355,
     9384, 9401, 9426, 9449, 9464, 9485, 9498, 9517, 9528, 9546, 9557, 9573,
     9588, 9598, 9612, 9621, 9635, 9644, 9657, 9666, 9679, 9687, 9700, 9712,
     9720, 9733, 9741, 9753, 9761, 9773, 9781, 9793, 9805, 9813, 9825, 9833,
     9845, 9853, 9865, 9873, 9885, 9893, 9905, 9917, 9925, 9937, 9945, 9957,
     9965, 9977, 9985, 9997},
    {10000, 10000, 9536, 9243, 8826, 8562, 8186, 7949, 7611, 7397, 7093, 6807,
     6626,  6369,  6206, 5974, 5827, 5618, 5485, 5295, 5117, 5003, 4841, 4738,
     4590,  4496,  4362, 4276, 4152, 4073, 3959, 3850, 3781, 3680, 3615, 3521,
     3460,  3371,  3314, 3231, 3150, 3098, 3021, 2970, 2897, 2848, 2777, 2731,
     2662,  2617,  2550, 2484, 2440, 2375, 2333, 2269, 2227, 2165, 2123, 2061,
     2000,  1959,  1897, 1857, 1796, 1756, 1695, 1655, 1595, 1555, 1495, 1435,
     1395,  1335,  1295, 1235, 1195, 1135, 1095, 1035, 976,  936,  876,  836,
     776,   737,   677,  637,  577,  537,  478,  418,  378,  318,  278,  219,
     179,   119,   79,   19},
    {0,    0,    59,   99,   159,  199,  258,  298,  358,  398,  458,  517,
     557,  617,  657,  717,  756,  816,  856,  916,  976,  1016, 1075, 1115,
     1175, 1215, 1275, 1315, 1375, 1415, 1475, 1535, 1575, 1635, 1675, 1736,
     1776, 1837, 1877, 1938, 2000, 2041, 2102, 2144, 2206, 2248, 2312, 2354,
     2418, 2462, 2528, 2594, 2639, 2708, 2754, 2824, 2872, 2946, 2995, 3072,
     3150, 3204, 3286, 3343, 3430, 3490, 3583, 3647, 3746, 3815, 3922, 4034,
     4112, 4234, 4318, 4451, 4543, 4688, 4789, 4948, 5117, 5235, 5420, 5550,
     5756, 5900, 6127, 6287, 6539, 6716, 6996, 7294, 7503, 7834, 8066, 8434,
     8692, 9101, 9388, 9842},
    {0,    0,    5,    9,    15,   19,   25,   29,   35,   39,   45,   51,
     55,   61,   65,   71,   75,   81,   85,   91,   97,   101,  107,  111,
     117,  121,  128,  132,  138,  142,  149,  155,  160,  167,  171,  179,
     183,  191,  197,  205,  214,  220,  230,  237,  248,  256,  269,  278,
     293,  303,  321,  340,  353,  376,  392,  419,  438,  469,  492,  530,
     571,  601,  651,  686,  745,  787,  856,  906,  987,  1046, 1142, 1247,
     1323, 1447, 1536, 1680, 1784, 1951, 2072, 2267, 2479, 2631, 2877, 3052,
     3335, 3537, 3861, 4093, 4464, 4729, 5153, 5611, 5937, 6458, 6829, 7420,
     7839, 8507, 8981, 9736}};

static const float interpolation[8][5] = {
    {-.0000417f, -.0058887f, .0758057f, .0617957f, -.0066209f},
    {-.0005088f, -.0032559f, .0877509f, .0470175f, -.0060670f},
    {-.0011711f, .0017899f, .0964619f, .0327264f, -.0048301f},
    {-.0021369f, .0095427f, .1010551f, .0199836f, -.0034084f},
    {-.0034084f, .0199836f, .1010551f, .0095427f, -.0021369f},
    {-.0048301f, .0327264f, .0964619f, .0017899f, -.0011711f},
    {-.0060670f, .0470175f, .0877509f, -.0032559f, -.0005088f},
    {-.0066209f, .0617957f, .0758057f, -.0058887f, -.0000417f}};

static const float reconstruction[20] = {
    -.0000417f, -.0005088f, -.0011711f, -.0021369f, -.0034084f,
    -.0048301f, -.0060670f, -.0066209f, -.0058887f, -.0032559f,
    .0017899f,  .0095427f,  .0199836f,  .0327264f,  .0470175f,
    .0617957f,  .0758057f,  .0877509f,  .0964619f,  .1010551f};

static float control(enum match_cl_curve curve, uint8_t value)
{
    return (float)factory_curve[curve][value] / 10000.f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_match_cl_filter_t *state,
                    match_cl_coefficients_t c, float input)
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

static match_cl_coefficients_t bass_coefficients(float bass)
{
    return (match_cl_coefficients_t){
        {bass * .9282434f + .012787461f, bass * -1.8564868f - .016877152f,
         bass * .9282434f + .004089691f, 0.f},
        {bass + .0130820125f, bass * -1.8564868f - .016809916f,
         bass * .8564868f + .0038623768f, 0.f},
        2u};
}

static match_cl_coefficients_t middle_coefficients(float middle)
{
    return (match_cl_coefficients_t){
        {middle + .28556892f, middle * -1.8735585f - .5614941f,
         middle * .8735586f + .27707207f, 0.f},
        {middle + .32350135f, middle * -1.8735585f - .5614941f,
         middle * .8735586f + .23913963f, 0.f},
        2u};
}

static match_cl_coefficients_t tone_coefficients(float master, float treble)
{
    float tm = treble * master;
    float tm2 = tm * master;
    float m2 = master * master;
    return (match_cl_coefficients_t){
        {master * .2695466f + tm * 261.51413f - tm2 * 179.91846f -
             m2 * .1854447f,
         tm2 * 539.7554f - tm * 621.351f - master * .1013428f + m2 * .1854447f,
         tm * 458.1597f - master * .2695466f - tm2 * 539.7554f + m2 * .1854447f,
         master * .1013428f - tm * 98.322784f + tm2 * 179.91846f -
             m2 * .1854447f},
        {treble * 93.64139f + master * 2.151926f + tm * 171.63115f -
             tm2 * 179.91846f - m2 * 2.1645477f + 1.f,
         tm2 * 539.7554f - master * 6.1108966f - tm * 515.63525f -
             treble * 109.1378f + m2 * 6.1227536f - .8192367f,
         master * 5.764486f - treble * 61.97574f + tm * 516.377f -
             tm2 * 539.7554f - m2 * 5.7518644f - .9993065f,
         treble * 77.47215f - master * 1.8055153f - tm * 172.37292f +
             tm2 * 179.91846f + m2 * 1.7936583f + .8199302f},
        3u};
}

static match_cl_coefficients_t presence_coefficients(float presence)
{
    return (match_cl_coefficients_t){
        {1.0356694f - presence * .9993705f, presence * 1.998741f - 2.0632164f,
         1.0275471f - presence * .9993705f, 0.f},
        {presence - 1.01483f, 2.0202277f - presence * 1.998741f,
         presence * .99874103f - 1.005408f, 0.f},
        2u};
}

static float nonlinear_stage(gp100_match_cl_amplifier_t *amplifier,
                             float input)
{
    float sample[5] = {input, amplifier->interpolation_history[0],
                       amplifier->interpolation_history[1],
                       amplifier->interpolation_history[2],
                       amplifier->interpolation_history[3]};
    float phase[8];
    float output = 0.f;
    size_t i, j;
    for (i = 0u; i < 8u; ++i)
    {
        phase[i] = 0.f;
        for (j = 0u; j < 5u; ++j)
            phase[i] += sample[j] * interpolation[i][j];
        phase[i] *= 8.f;
        phase[i] = phase[i] * .1677f / (fabsf(phase[i]) + .08215f);
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

float gp100_match_family_gain_control(uint8_t gain)
{
    return gain <= 99u ? control(MATCH_CL_GAIN, gain) : 0.0f;
}

bool gp100_match_family_reset(gp100_match_cl_amplifier_t *amplifier,
                                   float gain_value, uint8_t presence,
                                   uint8_t master, uint8_t bass, uint8_t middle,
                                   uint8_t treble)
{
    if (amplifier == NULL || presence > 99u || master > 99u || bass > 99u ||
        middle > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = gain_value;
    amplifier->presence = control(MATCH_CL_PRESENCE, presence);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bass = control(MATCH_CL_BASS, bass);
    amplifier->middle = control(MATCH_CL_MIDDLE, middle);
    amplifier->treble = control(MATCH_CL_TREBLE, treble);
    amplifier->configured = true;
    return true;
}

bool gp100_match_family_process(gp100_match_cl_amplifier_t *amplifier,
                                     float gain_target, uint8_t presence,
                                     uint8_t master, uint8_t bass,
                                     uint8_t middle, uint8_t treble,
                                     float input_drive_scale,
                                     float output_scale, float *audio,
                                     size_t frames, bool enabled)
{
    const match_cl_coefficients_t input_coefficients = {
        {1.04877102f, -.843932867f, 0.f, 0.f},
        {1.f, -.892703891f, 0.f, 0.f},
        1u};
    size_t n;
    if (amplifier == NULL || !amplifier->configured || presence > 99u ||
        master > 99u || bass > 99u || middle > 99u || treble > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = gain_target;
    amplifier->master_target = (float)master * .01f;
    amplifier->presence = control(MATCH_CL_PRESENCE, presence);
    amplifier->bass = control(MATCH_CL_BASS, bass);
    amplifier->middle = control(MATCH_CL_MIDDLE, middle);
    amplifier->treble = control(MATCH_CL_TREBLE, treble);
    for (n = 0u; n < frames; ++n)
    {
        float signal;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        if (!enabled)
            continue;
        signal = audio[n * 2u] * input_drive_scale * amplifier->gain *
                 amplifier->gain;
        signal = filter(&amplifier->input_filter, input_coefficients, signal);
        signal = filter(&amplifier->bass_filter,
                        bass_coefficients(amplifier->bass), signal);
        signal = filter(&amplifier->middle_filter,
                        middle_coefficients(amplifier->middle), signal);
        signal = nonlinear_stage(amplifier, signal);
        signal = filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->master, amplifier->treble),
                        signal);
        signal = filter(&amplifier->presence_filter,
                        presence_coefficients(amplifier->presence), signal);
        signal *= amplifier->master * output_scale;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}

bool gp100_match_cl_amplifier_reset(
    gp100_match_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble)
{
    if (gain > 99u)
        return false;
    return gp100_match_family_reset(
        amplifier, gp100_match_family_gain_control(gain), presence, master,
        bass, middle, treble);
}

bool gp100_match_cl_amplifier_process(
    gp100_match_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble, float *audio,
    size_t frames, bool enabled)
{
    if (gain > 99u)
        return false;
    return gp100_match_family_process(
        amplifier, gp100_match_family_gain_control(gain), presence, master,
        bass, middle, treble, 40.0f, 30.0f * .9441f * 1.06f, audio, frames,
        enabled);
}
