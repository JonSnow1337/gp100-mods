#include "foxy30n_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct foxy30n_coefficients
{
    float b[3];
    float a[3];
    size_t order;
} foxy30n_coefficients_t;

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float control_curve(uint8_t value, float exponent)
{
    float index = floorf((float)value * 2.55f) - 2.f;
    if (index < 0.f)
        index = 0.f;
    if (index > 251.f)
        index = 251.f;
    return floorf(powf(index / 251.f, exponent) * 10000.f) * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_foxy30n_filter_t *state,
                    foxy30n_coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;
    for (i = 0u; i < coefficients.order; ++i)
        output += coefficients.b[i + 1u] * state->x[i] -
                  coefficients.a[i + 1u] * state->y[i];
    output /= coefficients.a[0];
    for (i = 2u; i != 0u; --i)
    {
        state->x[i] = state->x[i - 1u];
        state->y[i] = state->y[i - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static foxy30n_coefficients_t normal_volume_coefficients(float volume)
{
    return (foxy30n_coefficients_t){
        {volume * .9997588f, volume * -.9997588f, 0.f},
        {1.f, -.9995177f, 0.f},
        1u};
}

static foxy30n_coefficients_t bright_volume_coefficients(float volume)
{
    float square = volume * volume;
    return (foxy30n_coefficients_t){
        {square - volume * 1.022676f, square * -2.f + volume * 2.f,
         square - volume * .9773243f},
        {square - volume * .9997588f - .02292244f,
         square * -2.f + volume * 2.f - .00001094019f,
         square - volume * 1.000241f + .0229115f},
        2u};
}

static foxy30n_coefficients_t cut_filter_coefficients(float cut)
{
    float scale = powf(10.f, (15.f - cut * 50.f) / 20.f);
    float tangent = tanf(.284951717f);
    float forward = 2.f / (scale + 1.f);
    float reverse = 2.f / (1.f / scale + 1.f);
    float den = (tangent + forward) * (tangent + 1.f);
    return (foxy30n_coefficients_t){
        {(tangent + reverse) * (tangent - 1.f) / den,
         ((tangent + reverse) * (tangent + 1.f) +
          (tangent - reverse) * (tangent - 1.f)) /
             den,
         (tangent - reverse) * (tangent + 1.f) / den},
        {1.f,
         ((tangent + forward) * (tangent - 1.f) +
          (tangent - forward) * (tangent + 1.f)) /
             den,
         (tangent - forward) * (tangent - 1.f) / den},
        2u};
}

static foxy30n_coefficients_t output_shelf_coefficients(float cut)
{
    const float frequency = 1.42475855f;
    float scale = powf(10.f, cut * -13.f / 40.f);
    float k = sinf(frequency) / 1.414f;
    float shared = -2.f * cosf(frequency);
    return (foxy30n_coefficients_t){{k * scale + 1.f, shared, 1.f - k * scale},
                                    {k / scale + 1.f, shared, 1.f - k / scale},
                                    2u};
}

static float shaped_sample(float input, float drive)
{
    return copysignf(.1f * (1.f - expf(-drive * fabsf(input))), input);
}

static float nonlinear_stage(gp100_foxy30n_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 1.5f + .3f;
    float normalization = fmaxf(1.f / (drive * 3.f + 1.f), .17f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float point[4];
    float output = 0.f;
    size_t i;
    point[0] = amplifier->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.f;
    point[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 40u; ++i)
        output += reconstruction_fir[i] * amplifier->fir_history[i];
    amplifier->fir_history[0] = shaped_sample(point[3], drive) * normalization;
    amplifier->fir_history[1] = shaped_sample(point[2], drive) * normalization;
    amplifier->fir_history[2] = shaped_sample(point[1], drive) * normalization;
    amplifier->fir_history[3] = shaped_sample(point[0], drive) * normalization;
    amplifier->previous_nonlinear_input = input;
    return output;
}

bool gp100_foxy30n_amplifier_reset(
    gp100_foxy30n_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, bool bright)
{
    if (amplifier == NULL || volume > 99u || cut > 99u || master > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume = amplifier->volume_target = control_curve(volume, 2.f);
    amplifier->cut = amplifier->cut_target = control_curve(cut, 3.f);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bright = bright;
    amplifier->configured = true;
    return true;
}

bool gp100_foxy30n_amplifier_process(
    gp100_foxy30n_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, bool bright, float *audio, size_t frames, bool enabled)
{
    static const foxy30n_coefficients_t input[2] = {
        {{6.59388924f, -9.44703102f, 2.85672092f},
         {1.f, -1.42653096f, .427327514f},
         2u},
        {{9.44703102f, 8.1678772f, .8710531f},
         {1.f, .742128074f, .0126189f},
         2u}};
    static const foxy30n_coefficients_t envelope = {
        {GP100_ENVELOPE_10_MS_B0, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_10_MS_A1, GP100_ENVELOPE_10_MS_A2},
        2u};
    size_t frame;
    if (amplifier == NULL || !amplifier->configured || volume > 99u ||
        cut > 99u || master > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u)
        return false;
    amplifier->volume_target = control_curve(volume, 2.f);
    amplifier->cut_target = control_curve(cut, 3.f);
    amplifier->master_target = (float)master * .01f;
    amplifier->bright = bright;
    for (frame = 0u; frame < frames; ++frame)
    {
        float signal;
        float level;
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->cut = glide(amplifier->cut, amplifier->cut_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        if (!enabled)
            continue;
        signal = filter(&amplifier->input_filter[0], input[0],
                        audio[frame * 2u] * 1.1f);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        if (amplifier->bright)
            signal =
                filter(&amplifier->bright_volume_filter,
                       bright_volume_coefficients(amplifier->volume), signal);
        else
            signal =
                filter(&amplifier->normal_volume_filter,
                       normal_volume_coefficients(amplifier->volume), signal);
        signal *= amplifier->master;
        signal = filter(&amplifier->cut_filter,
                        cut_filter_coefficients(amplifier->cut), signal);
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal = filter(&amplifier->output_shelf,
                        output_shelf_coefficients(amplifier->cut), signal);
        signal *= 30.492f;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
