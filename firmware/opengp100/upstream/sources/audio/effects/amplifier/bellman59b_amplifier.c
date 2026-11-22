#include "bellman59b_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct bellman59b_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} bellman59b_coefficients_t;

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float quantized_power(uint8_t raw, float offset, float divisor,
                             float exponent)
{
    float index = floorf((float)raw * 2.55f) - offset;
    if (index < 0.0f)
        index = 0.0f;
    if (index > divisor)
        index = divisor;
    return floorf(powf(index / divisor, exponent) * 10000.0f) * .0001f;
}

static float volume_curve(uint8_t raw)
{
    return quantized_power(raw, 2.0f, 251.0f, 2.0f);
}

static float presence_curve(uint8_t raw)
{
    return quantized_power(raw, 2.0f, 251.0f, 1.5f);
}

static float bass_curve(uint8_t raw)
{
    return quantized_power(raw, 0.0f, 255.0f, 3.5f);
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_bellman59b_filter_t *state,
                    bellman59b_coefficients_t coefficients, float input)
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

static bellman59b_coefficients_t volume_coefficients(float volume)
{
    float squared = volume * volume;
    return (bellman59b_coefficients_t){
        {squared - volume * 1.113379f, squared * -2.0f + volume * 2.0f,
         squared - volume * .8866213f, 0.0f},
        {squared - volume * .9994331f - .1140099f,
         squared * -2.0f + volume * 2.0f - .0001285473f,
         squared - volume * 1.000567f + .1138813f, 0.0f},
        2u};
}

static bellman59b_coefficients_t tone_coefficients(float bass, float middle,
                                                   float treble)
{
    float middle_squared = middle * middle;
    return (bellman59b_coefficients_t){
        {bass * .01651759f + middle * .01600963f + treble * .02613953f +
             bass * .6284257f * middle + bass * treble -
             middle * .025f * treble - middle_squared * .01571064f +
             .0004129397f,
         middle * .075f * treble - middle * .04323924f - treble * .07612805f -
             bass * 1.721283f * middle - bass * 3.f * treble -
             bass * .0127989f + middle_squared * .04303207f - .0003199725f,
         middle * .03863323f - bass * .01651759f + treble * .07386047f +
             bass * 1.557289f * middle + bass * 3.f * treble -
             middle * .075f * treble - middle_squared * .03893222f -
             .0004129397f,
         bass * .0127989f - middle * .01140362f - treble * .02387195f -
             bass * .4644315f * middle - bass * treble +
             middle * .025f * treble + middle_squared * .01161079f +
             .0003199725f},
        {bass * 1.197923f - middle * .01352552f + bass * .6284257f * middle -
             middle_squared * .01571064f + .03129433f,
         middle * .03629591f - bass * 3.194205f - bass * 1.721283f * middle +
             middle_squared * .04303207f - .08077437f,
         bass * 2.802077f - middle * .03183162f + bass * 1.557289f * middle -
             middle_squared * .03893222f + .06870983f,
         middle * .009061233f - bass * .8057952f - bass * .4644315f * middle +
             middle_squared * .01161079f - .01922147f},
        3u};
}

static bellman59b_coefficients_t presence_coefficients(float presence)
{
    const float angle = 2.66116405f;
    float scale = powf(10.0f, presence * 22.7f / 40.0f);
    float q = sinf(angle) / 1.2f;
    float shared = -2.0f * cosf(angle);
    return (bellman59b_coefficients_t){
        {q * scale + 1.0f, shared, 1.0f - q * scale, 0.0f},
        {q / scale + 1.0f, shared, 1.0f - q / scale, 0.0f},
        2u};
}

static float nonlinear_stage(gp100_bellman59b_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 2.0f + 1.4f;
    float normalization = fmaxf(1.0f / (drive * 3.0f + 1.0f), .1f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.0f;
    float point[4];
    float output = 0.0f;
    size_t i;
    point[0] = amplifier->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.0f;
    point[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 40u; ++i)
        output += amplifier->fir_history[i] * reconstruction_fir[i];
    for (i = 0u; i < 4u; ++i)
    {
        float sample = point[3u - i];
        amplifier->fir_history[i] =
            copysignf(.15f * (1.0f - expf(-drive * fabsf(sample))), sample) *
            normalization;
    }
    amplifier->previous_nonlinear_input = input;
    return output;
}

static bool controls_valid(uint8_t volume, uint8_t presence, uint8_t output,
                           uint8_t bass, uint8_t middle, uint8_t treble)
{
    return volume <= 99u && presence <= 99u && output <= 99u && bass <= 99u &&
           middle <= 99u && treble <= 99u;
}

bool gp100_bellman59b_amplifier_reset(
    gp100_bellman59b_amplifier_t *amplifier, uint8_t volume,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble)
{
    if (amplifier == NULL ||
        !controls_valid(volume, presence, output, bass, middle, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume = amplifier->volume_target = volume_curve(volume);
    amplifier->presence = presence_curve(presence);
    amplifier->output = amplifier->output_target = (float)output * .01f;
    amplifier->bass = bass_curve(bass);
    amplifier->middle = amplifier->middle_target = (float)middle * .01f;
    amplifier->treble = amplifier->treble_target = (float)treble * .01f;
    amplifier->configured = true;
    return true;
}

bool gp100_bellman59b_amplifier_process(
    gp100_bellman59b_amplifier_t *amplifier, uint8_t volume,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble, float *audio, size_t frames, bool enabled)
{
    static const bellman59b_coefficients_t input[2] = {
        {{6.68555689f, -8.51430607f, 1.83079243f, 0.f},
         {1.f, -1.30411935f, .30448091f, 0.f},
         2u},
        {{8.51430607f, 3.82515931f, .0000794f, 0.f},
         {1.f, .382216811f, .0000703f, 0.f},
         2u}};
    float requested_presence;
    float requested_bass;
    static const bellman59b_coefficients_t envelope = {
        {GP100_ENVELOPE_10_MS_B0, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_10_MS_A1, GP100_ENVELOPE_10_MS_A2},
        2u};
    size_t frame;
    size_t i;
    if (amplifier == NULL || !amplifier->configured ||
        !controls_valid(volume, presence, output, bass, middle, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->volume_target = volume_curve(volume);
    amplifier->output_target = (float)output * .01f;
    amplifier->middle_target = (float)middle * .01f;
    amplifier->treble_target = (float)treble * .01f;
    requested_presence = presence_curve(presence);
    requested_bass = bass_curve(bass);
    for (frame = 0u; frame < frames; ++frame)
    {
        float signal;
        float level;
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->output = glide(amplifier->output, amplifier->output_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->presence = requested_presence;
        amplifier->bass = requested_bass;
        signal = audio[frame * 2u] * 1.1f;
        for (i = 0u; i < 2u; ++i)
            signal = filter(&amplifier->input_filter[i], input[i], signal);
        signal = filter(&amplifier->volume_filter,
                        volume_coefficients(amplifier->volume), signal);
        signal = filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->bass, amplifier->middle,
                                          amplifier->treble),
                        signal);
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal = filter(&amplifier->presence_filter,
                        presence_coefficients(amplifier->presence), signal);
        signal *= 61.376f * amplifier->output;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
