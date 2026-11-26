#include "uk50jp_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct uk50jp_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} uk50jp_coefficients_t;

static const uint16_t linear_curve[100] = {
    0,    0,    1,    4,    10,   15,   26,   35,   51,   63,   84,   107,
    124,  152,  172,  205,  229,  266,  293,  335,  381,  412,  462,  497,
    552,  590,  650,  691,  755,  800,  869,  941,  990,  1067, 1120, 1201,
    1257, 1343, 1402, 1493, 1587, 1651, 1750, 1817, 1920, 1991, 2099, 2172,
    2285, 2362, 2480, 2600, 2682, 2807, 2892, 3022, 3111, 3245, 3337, 3476,
    3619, 3715, 3862, 3962, 4114, 4217, 4373, 4479, 4641, 4750, 4916, 5085,
    5200, 5373, 5491, 5669, 5790, 5973, 6097, 6285, 6476, 6605, 6801, 6933,
    7133, 7269, 7474, 7612, 7822, 7964, 8179, 8396, 8543, 8765, 8915, 9142,
    9295, 9527, 9683, 9920};

static const uint16_t volume_curve[100] = {
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

static float filter(gp100_uk50jp_filter_t *state,
                    uk50jp_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0; i < coefficients.order; ++i)
    {
        output += coefficients.b[i + 1] * state->x[i] -
                  coefficients.a[i + 1] * state->y[i];
    }
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

static uk50jp_coefficients_t tone(float bass, float middle, float treble)
{
    float middle_squared = middle * middle;
    float bass_middle = bass * middle;
    float bass_treble = bass * treble;
    float middle_treble = middle * treble;
    uk50jp_coefficients_t result = {
        {bass * .02127887f + middle * .02373404f + treble * .02603876f +
             bass_middle * .9322378f + bass_treble - middle_treble * .025f -
             middle_squared * .02330595f + .0005319717f,
         middle_treble * .075f - middle * .06653748f - treble * .07602267f -
             bass_middle * 2.647389f - bass_treble * 3.f - bass * .01820065f +
             middle_squared * .06618473f - .0004550163f,
         middle * .06202353f - bass * .02127887f + treble * .07396124f +
             bass_middle * 2.498065f + bass_treble * 3.f -
             middle_treble * .075f - middle_squared * .06245163f - .0005319717f,
         bass * .01820065f - middle * .01922009f - treble * .02397733f -
             bass_middle * .7829137f - bass_treble + middle_treble * .025f +
             middle_squared * .01957284f + .0004550163f},
        {bass * 1.117771f - middle * .003678272f + bass_middle * .9322378f -
             middle_squared * .02330595f + .02908328f,
         middle * .01087483f - bass * 3.114693f - bass_middle * 2.647389f +
             middle_squared * .06618473f - .07878821f,
         bass * 2.882229f - middle * .01056415f + bass_middle * 2.498065f -
             middle_squared * .06245163f + .07091983f,
         middle * .003367595f - bass * .8853068f - bass_middle * .7829137f +
             middle_squared * .01957284f - .02120868f},
        3};
    return result;
}

static float exponential_shape(float input, float slope, float limit)
{
    float magnitude = fabsf(input);
    if (magnitude >= limit)
        return copysignf(limit, input);
    return copysignf((1.f - expf(-magnitude * slope)) * limit, input);
}

static float power_shape(float input)
{
    float magnitude = fabsf(input);
    float power;
    unsigned i;
    if (magnitude >= 2.f)
        return copysignf(2.f, input);
    power = 1.f - magnitude * 25.f / 1024.f;
    for (i = 0; i < 10; ++i)
        power *= power;
    return copysignf((1.f - power) * 2.f, input);
}

static float nonlinear_stage(gp100_uk50jp_amplifier_t *amplifier,
                             size_t stage_index, float input, bool first_stage)
{
    float phase[4];
    float delta =
        (input - amplifier->previous_nonlinear_input[stage_index]) / 3.f;
    float output = 0.f;
    size_t i;
    phase[0] = amplifier->previous_nonlinear_input[stage_index];
    phase[1] = phase[0] + delta;
    phase[2] = phase[0] + delta * 2.f;
    phase[3] = input;
    memmove(&amplifier->fir_history[stage_index][4],
            amplifier->fir_history[stage_index], 36 * sizeof(float));
    for (i = 0; i < 4; ++i)
    {
        amplifier->fir_history[stage_index][i] =
            first_stage ? exponential_shape(phase[3 - i], .3f, 50.f)
                        : power_shape(phase[3 - i]);
    }
    for (i = 0; i < 40; ++i)
    {
        output += amplifier->fir_history[stage_index][i] * fir[i];
    }
    amplifier->previous_nonlinear_input[stage_index] = input;
    return first_stage ? output * .02f : output;
}

bool gp100_uk50jp_amplifier_reset(gp100_uk50jp_amplifier_t *amplifier,
                                       uint8_t volume_one, uint8_t presence,
                                       uint8_t output, uint8_t bass,
                                       uint8_t middle, uint8_t treble,
                                       uint8_t volume_two)
{
    if (!amplifier || volume_one > 99 || presence > 99 || output > 99 ||
        bass > 99 || middle > 99 || treble > 99 || volume_two > 99)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume_one = amplifier->volume_one_target =
        curve(volume_curve, volume_one) * .5f;
    amplifier->volume_two = amplifier->volume_two_target =
        curve(volume_curve, volume_two);
    amplifier->presence = curve(linear_curve, presence);
    amplifier->output = amplifier->output_target = (float)output * .01f;
    amplifier->bass = curve(linear_curve, bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_uk50jp_amplifier_process(
    gp100_uk50jp_amplifier_t *amplifier, uint8_t volume_one,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble, uint8_t volume_two, float *audio, size_t frame_count,
    bool enabled)
{
    static const uk50jp_coefficients_t channel_one[2] = {
        {{19.169096f, -35.669075f, 16.500563f, 0.f},
         {1.f, -1.86608565f, .866164684f, 0.f},
         2},
        {{35.669075f, 18.477316f, .00076570001f, 0.f},
         {1.f, -.099493898f, .0000005f, 0.f},
         2}};
    static const uk50jp_coefficients_t channel_two[2] = {
        {{18.518007f, -36.772133f, 18.255064f, 0.f},
         {1.f, -1.96361279f, .964070201f, 0.f},
         2},
        {{36.772133f, 19.013519f, .0151183f, 0.f},
         {1.f, -.105783299f, .0000192f, 0.f},
         2}};
    static const uk50jp_coefficients_t channel_two_voicing[2] = {
        {{4.8064f, .5554f, -2.8064f, 0.f}, {2.5154f, .5554f, -.5154f, 0.f}, 2},
        {{1.017f, -1.9992f, .983f, 0.f}, {1.0239f, -1.9992f, .9761f, 0.f}, 2}};
    size_t frame;
    size_t i;
    if (!amplifier || !amplifier->configured || volume_one > 99 ||
        presence > 99 || output > 99 || bass > 99 || middle > 99 ||
        treble > 99 || volume_two > 99 || (frame_count && !audio) ||
        frame_count > SIZE_MAX / 2)
        return false;
    amplifier->volume_one_target = curve(volume_curve, volume_one) * .5f;
    amplifier->volume_two_target = curve(volume_curve, volume_two);
    amplifier->presence = curve(linear_curve, presence);
    amplifier->output_target = (float)output * .01f;
    amplifier->bass = curve(linear_curve, bass);
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    for (frame = 0; frame < frame_count; ++frame)
    {
        float input;
        float first;
        float second;
        float angle;
        float q;
        float scale;
        amplifier->volume_one =
            glide(amplifier->volume_one, amplifier->volume_one_target);
        amplifier->volume_two =
            glide(amplifier->volume_two, amplifier->volume_two_target);
        amplifier->output = glide(amplifier->output, amplifier->output_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->output = amplifier->output_target;
        input = audio[frame * 2];
        first = input;
        second = input;
        for (i = 0; i < 2; ++i)
        {
            first =
                filter(&amplifier->channel_one_input[i], channel_one[i], first);
            second = filter(&amplifier->channel_two_input[i], channel_two[i],
                            second);
        }
        first = filter(&amplifier->volume_one_filter,
                       (uk50jp_coefficients_t){
                           {amplifier->volume_one * .9994849f,
                            amplifier->volume_one * -.9994849f, 0.f, 0.f},
                           {1.f, -.9989698f, 0.f, 0.f},
                           1},
                       first * .15f);
        /* The factory uses the same coefficients for both sides of the
         * post-VOL 1 biquad, cancelling its poles and zeros. */
        second = filter(
            &amplifier->volume_two_filter,
            (uk50jp_coefficients_t){{amplifier->volume_two * .9948728f,
                                     amplifier->volume_two * -1.892606f,
                                     amplifier->volume_two * .8977333f, 0.f},
                                    {1.f, -1.892106f, .8931068f, 0.f},
                                    2},
            second * .15f);
        for (i = 0; i < 2; ++i)
        {
            second = filter(&amplifier->channel_two_voicing[i],
                            channel_two_voicing[i], second);
        }
        first = nonlinear_stage(amplifier, 0, first + second, true);
        first = filter(
            &amplifier->tone_filter,
            tone(amplifier->bass, amplifier->middle, amplifier->treble), first);
        first = nonlinear_stage(amplifier, 1, first, false);
        angle = 1.8521862f;
        scale = powf(10.f, amplifier->presence * 6.29f / 40.f);
        q = sinf(angle) / .652f;
        first = filter(
            &amplifier->presence_filter,
            (uk50jp_coefficients_t){
                {q * scale + 1.f, -2.f * cosf(angle), 1.f - q * scale, 0.f},
                {q / scale + 1.f, -2.f * cosf(angle), 1.f - q / scale, 0.f},
                2},
            first);
        first *= amplifier->output * .4608f;
        audio[frame * 2] = first;
        audio[frame * 2 + 1] = first;
    }
    return true;
}
