#include "supdual_od_amplifier.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct supdual_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} supdual_coefficients_t;

static float normalized_index(uint8_t raw)
{
    float index = floorf((float)raw * 2.55f) - 2.0f;
    if (index < 0.0f)
        index = 0.0f;
    if (index > 251.0f)
        index = 251.0f;
    return index / 251.0f;
}

static float volume_curve(uint8_t raw)
{
    float x = normalized_index(raw);
    float x2 = x * x;
    float x4 = x2 * x2;
    float x7 = x4 * x2 * x;
    return floorf((.5f * x + .5f * x7) * 10000.0f) * .0001f;
}

static float linear_curve(uint8_t raw)
{
    return floorf(normalized_index(raw) * 10000.0f) * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_supdual_od_filter_t *state,
                    supdual_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0u; i < coefficients.order; ++i)
    {
        output += coefficients.b[i + 1u] * state->x[i];
        output -= coefficients.a[i + 1u] * state->y[i];
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

static supdual_coefficients_t tone_coefficients(float tone)
{
    float squared = tone * tone;
    return (supdual_coefficients_t){
        {squared * -.9054494f + tone * .960216f + .04297302f,
         squared * 2.716348f - tone * 2.871068f - .1222946f,
         squared * -2.716348f + tone * 2.861505f + .1156907f,
         squared * .9054494f - tone * .9506529f - .03636912f},
        {tone + .04300836f, tone * -2.983279f - .1223249f,
         tone * 2.966593f + .1156554f, tone * -.9833139f - .03633875f},
        3u};
}

static float nonlinear_sample(float input)
{
    const float magnitude = fabsf(input);
    const float denominator = magnitude * magnitude +
                              magnitude * 129200.0f + 92490.0f;

    return copysignf(magnitude * 100500.0f / denominator, input);
}

static float nonlinear_stage(gp100_supdual_od_channel_t *channel,
                             float input)
{
    const float previous = channel->previous_nonlinear_input;
    const float step = (input - previous) / 3.0f;
    const float shaped[4] = {
        nonlinear_sample(previous),
        nonlinear_sample(previous + step),
        nonlinear_sample(previous + step * 2.0f),
        nonlinear_sample(previous + step * 3.0f)
    };

    channel->previous_nonlinear_input = input;
    return gp100_decimate_four(channel->fir_history, shaped);
}

static float process_channel(gp100_supdual_od_channel_t *channel,
                             float input, float volume, float tone)
{
    static const supdual_coefficients_t conditioning[2] = {
        {{.992346168f, -.992346168f, 0.f, 0.f},
         {1.f, -.984692395f, 0.f, 0.f},
         1u},
        {{-5.90077782f, 5.86108923f, 0.f, 0.f},
         {1.f, -.960311174f, 0.f, 0.f},
         1u}};
    float signal = input * 5.0f * volume;
    size_t i;
    for (i = 0u; i < 2u; ++i)
        signal = filter(&channel->input_filter[i], conditioning[i], signal);
    signal = nonlinear_stage(channel, signal);
    return filter(&channel->tone_filter, tone_coefficients(tone), signal);
}

static bool controls_valid(uint8_t volume_1, uint8_t tone_1, uint8_t volume_2,
                           uint8_t tone_2, uint8_t output)
{
    return volume_1 <= 99u && tone_1 <= 99u && volume_2 <= 99u &&
           tone_2 <= 99u && output <= 99u;
}

bool gp100_supdual_od_amplifier_reset(
    gp100_supdual_od_amplifier_t *amplifier, uint8_t volume_1,
    uint8_t tone_1, uint8_t volume_2, uint8_t tone_2, uint8_t output)
{
    if (amplifier == NULL ||
        !controls_valid(volume_1, tone_1, volume_2, tone_2, output))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume_1 = amplifier->volume_1_target = volume_curve(volume_1);
    amplifier->tone_1 = amplifier->tone_1_target = 1.0f - volume_curve(tone_1);
    amplifier->volume_2 = amplifier->volume_2_target = volume_curve(volume_2);
    amplifier->tone_2 = amplifier->tone_2_target = 1.0f - linear_curve(tone_2);
    amplifier->output = amplifier->output_target = (float)output * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_supdual_od_amplifier_process(
    gp100_supdual_od_amplifier_t *amplifier, uint8_t volume_1,
    uint8_t tone_1, uint8_t volume_2, uint8_t tone_2, uint8_t output,
    float *audio, size_t frames, bool enabled)
{
    size_t frame;
    if (amplifier == NULL || !amplifier->configured ||
        !controls_valid(volume_1, tone_1, volume_2, tone_2, output) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->volume_1_target = volume_curve(volume_1);
    amplifier->tone_1_target = 1.0f - volume_curve(tone_1);
    amplifier->volume_2_target = volume_curve(volume_2);
    amplifier->tone_2_target = 1.0f - linear_curve(tone_2);
    amplifier->output_target = (float)output * .01f;
    for (frame = 0u; frame < frames; ++frame)
    {
        float signal;
        float adaptive_gain;
        amplifier->volume_1 =
            glide(amplifier->volume_1, amplifier->volume_1_target);
        amplifier->tone_1 = glide(amplifier->tone_1, amplifier->tone_1_target);
        amplifier->volume_2 =
            glide(amplifier->volume_2, amplifier->volume_2_target);
        amplifier->tone_2 = glide(amplifier->tone_2, amplifier->tone_2_target);
        amplifier->output = glide(amplifier->output, amplifier->output_target);
        if (!enabled)
            continue;
        signal = process_channel(&amplifier->channel[0], audio[frame * 2u],
                                 amplifier->volume_1, amplifier->tone_1);
        signal += process_channel(&amplifier->channel[1], audio[frame * 2u],
                                  amplifier->volume_2, amplifier->tone_2);
        adaptive_gain = (amplifier->volume_1 < .059100002f &&
                         amplifier->volume_2 < .059100002f)
                            ? 4.0f
                            : 2.8f;
        signal *= adaptive_gain * .5313672f * amplifier->output;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
