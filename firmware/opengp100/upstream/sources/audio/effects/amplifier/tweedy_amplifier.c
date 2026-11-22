#include "tweedy_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct tweedy_coefficients
{
    float b0, b1, b2;
    float a0, a1, a2;
} tweedy_coefficients_t;

static const float factory_gain_tone[100] = {
    0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0001f, 0.0003f, 0.0006f, 0.0008f,
    0.0013f, 0.0017f, 0.0025f, 0.0034f, 0.0041f, 0.0053f, 0.0062f, 0.0077f,
    0.0089f, 0.0107f, 0.0121f, 0.0143f, 0.0168f, 0.0186f, 0.0214f, 0.0235f,
    0.0267f, 0.0291f, 0.0328f, 0.0354f, 0.0396f, 0.0425f, 0.0471f, 0.0521f,
    0.0555f, 0.0610f, 0.0647f, 0.0707f, 0.0748f, 0.0813f, 0.0858f, 0.0928f,
    0.1001f, 0.1052f, 0.1131f, 0.1186f, 0.1271f, 0.1330f, 0.1420f, 0.1483f,
    0.1580f, 0.1647f, 0.1750f, 0.1857f, 0.1930f, 0.2043f, 0.2121f, 0.2241f,
    0.2323f, 0.2449f, 0.2536f, 0.2669f, 0.2807f, 0.2901f, 0.3045f, 0.3143f,
    0.3295f, 0.3398f, 0.3557f, 0.3665f, 0.3830f, 0.3943f, 0.4117f, 0.4294f,
    0.4415f, 0.4601f, 0.4727f, 0.4920f, 0.5051f, 0.5251f, 0.5388f, 0.5596f,
    0.5810f, 0.5955f, 0.6176f, 0.6326f, 0.6556f, 0.6712f, 0.6949f, 0.7110f,
    0.7357f, 0.7523f, 0.7778f, 0.8037f, 0.8213f, 0.8481f, 0.8663f, 0.8940f,
    0.9127f, 0.9413f, 0.9606f, 0.9900f};

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f,  0.0015f,  0.0032f,  0.0048f,  0.0053f,
    0.0034f,  -0.0016f, -0.0092f, -0.0175f, -0.0233f, -0.0227f, -0.0123f,
    0.0097f,  0.0421f,  0.0807f,  0.1193f,  0.1504f,  0.1678f,  0.1678f,
    0.1504f,  0.1193f,  0.0807f,  0.0421f,  0.0097f,  -0.0123f, -0.0227f,
    -0.0233f, -0.0175f, -0.0092f, -0.0016f, 0.0034f,  0.0053f,  0.0048f,
    0.0032f,  0.0015f,  0.0001f,  -0.0007f, -0.0011f};

static float filter(gp100_tweedy_filter_t *state, tweedy_coefficients_t c,
                    float input)
{
    float output = (c.b0 * input + c.b1 * state->x1 + c.b2 * state->x2 -
                    c.a1 * state->y1 - c.a2 * state->y2) /
                   c.a0;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static tweedy_coefficients_t tone_coefficients(float tone)
{
    float square = tone * tone;
    return (tweedy_coefficients_t){
        tone - square * 0.494985f + 0.002415193f,
        square * 0.9899699f - tone * 1.97994f + 0.00005415231f,
        tone * 0.9799398f - square * 0.494985f - 0.002361041f,
        square * -0.494985f + tone * 0.505015f + 0.5086243f,
        square * 0.9899699f - tone * 0.9899699f - 0.9899158f,
        square * -0.494985f + tone * 0.4849549f + 0.4813998f};
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + 0.0001f, target);
    if (current > target)
        return fmaxf(current - 0.0001f, target);
    return target;
}

static float shaped_sample(float input, float drive, float amplitude)
{
    float magnitude = 1.0f - expf(-drive * fabsf(input));
    return copysignf(amplitude * magnitude, input);
}

static float nonlinear_stage(gp100_tweedy_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 6.0f + 5.0f;
    float normalization = fmaxf(1.0f / (drive * 3.0f + 1.0f), 0.04f);
    float amplitude = 0.25f - amplifier->gain * 0.1f;
    float step = (input - amplifier->previous_nonlinear_input) / 3.0f;
    float samples[4];
    float output = 0.0f;
    size_t i;
    for (i = 0u; i < 4u; ++i)
    {
        float point = amplifier->previous_nonlinear_input + step * (float)i;
        samples[i] = shaped_sample(point, drive, amplitude) * normalization;
    }
    amplifier->previous_nonlinear_input = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
        amplifier->fir_history[i] = samples[3u - i];
    for (i = 0u; i < 40u; ++i)
        output += reconstruction_fir[i] * amplifier->fir_history[i];
    return output;
}

bool gp100_tweedy_amplifier_reset(gp100_tweedy_amplifier_t *amplifier,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume)
{
    if (amplifier == NULL || gain > 99u || tone > 99u || volume > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = factory_gain_tone[gain];
    amplifier->tone = amplifier->tone_target = factory_gain_tone[tone];
    amplifier->volume = amplifier->volume_target = (float)volume * 0.01f;
    amplifier->configured = true;
    return true;
}

bool gp100_tweedy_amplifier_process(
    gp100_tweedy_amplifier_t *amplifier, uint8_t gain, uint8_t tone,
    uint8_t volume, float *audio, size_t frames, bool enabled)
{
    static const tweedy_coefficients_t input[2] = {
        {7.44692469f, -7.59236145f, 0.157064199f, 1.0f, -1.07992887f,
         0.0821245983f},
        {7.59236145f, 4.41363668f, 0.00303920009f, 1.0f, 0.545566678f,
         0.000249400007f}};
    static const tweedy_coefficients_t drive = {0.933403492f, -1.70208585f,
                                                0.769104898f, 1.0f,
                                                -1.82079685f, 0.822817683f};
    static const tweedy_coefficients_t output = {-1.59978998f,  4.03873348f,
                                                 -2.03598166f,  1.0f,
                                                 -0.657572925f, 0.060534738f};
    static const tweedy_coefficients_t envelope = {
        GP100_ENVELOPE_50_MS_B0, 0.0f, 0.0f, 1.0f,
        GP100_ENVELOPE_50_MS_A1, GP100_ENVELOPE_50_MS_A2};
    size_t n;
    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        tone > 99u || volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = factory_gain_tone[gain];
    amplifier->tone_target = factory_gain_tone[tone];
    amplifier->volume_target = (float)volume * 0.01f;
    for (n = 0u; n < frames; ++n)
    {
        tweedy_coefficients_t tone_c;
        float signal, level;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->tone = glide(amplifier->tone, amplifier->tone_target);
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        if (!enabled)
            continue;
        tone_c = tone_coefficients(amplifier->tone);
        signal =
            filter(&amplifier->input_filter[0], input[0], audio[n * 2u] * 1.1f);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        signal = filter(&amplifier->tone_filter, tone_c, signal);
        signal *= amplifier->gain;
        signal = filter(&amplifier->drive_filter, drive, signal);
        signal = filter(&amplifier->output_filter, output, signal * 0.5f);
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal *= 110.0f * amplifier->volume;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
