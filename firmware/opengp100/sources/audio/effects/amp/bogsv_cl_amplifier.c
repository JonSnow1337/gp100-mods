#include "bogsv_cl_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct bogsv_coefficients
{
    float b[4], a[4];
    size_t order;
} bogsv_coefficients_t;

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float curve(uint8_t value, float exponent)
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

static float filter(gp100_bogsv_cl_filter_t *state, bogsv_coefficients_t c,
                    float input)
{
    float output = c.b[0] * input;
    size_t i;
    for (i = 0u; i < c.order; ++i)
        output += c.b[i + 1u] * state->x[i] - c.a[i + 1u] * state->y[i];
    output /= c.a[0];
    for (i = 2u; i != 0u; --i)
    {
        state->x[i] = state->x[i - 1u];
        state->y[i] = state->y[i - 1u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static bogsv_coefficients_t tone_coefficients(float bass, float treble)
{
    return (bogsv_coefficients_t){
        {bass * .1194639f + treble * .001613107f + bass * treble + .0001851421f,
         bass * -.3161494f - treble * .00161062f - bass * 3.f * treble -
             .0001337138f,
         bass * .2761725f - treble * .001613107f + bass * 3.f * treble -
             .0001851421f,
         treble * .00161062f - bass * .07948697f - bass * treble +
             .0001337138f},
        {bass * 1.353717f + .002176124f, bass * -3.550403f - .001365876f,
         bass * 3.041919f - .00217496f, .00136704f - bass * .8452335f},
        3u};
}

static bogsv_coefficients_t bright_coefficients(float gain)
{
    float square = gain * gain;
    return (bogsv_coefficients_t){
        {square - gain * 1.113379f, gain * .8866213f - square, 0.f, 0.f},
        {square - gain - .1133787f, gain - square - .1133787f, 0.f, 0.f},
        1u};
}

static bogsv_coefficients_t presence_coefficients(float presence)
{
    const float frequency = 1.42475855f;
    float scale = powf(10.f, presence * 13.f / 40.f);
    float k = sinf(frequency) / .46f;
    float shared = -2.f * cosf(frequency);
    return (bogsv_coefficients_t){
        {k * scale + 1.f, shared, 1.f - k * scale, 0.f},
        {k / scale + 1.f, shared, 1.f - k / scale, 0.f},
        2u};
}

static float nonlinear_stage(gp100_bogsv_cl_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope + 1.f;
    float normalization = fmaxf(1.f / (drive * 3.f + 1.f), .1f);
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    float point[4], output = 0.f;
    size_t i;
    point[0] = amplifier->previous_nonlinear_input;
    point[1] = point[0] + step;
    point[2] = point[0] + step * 2.f;
    point[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 40u; ++i)
        output += reconstruction_fir[i] * amplifier->fir_history[i];
    for (i = 0u; i < 4u; ++i)
    {
        float p = point[3u - i];
        amplifier->fir_history[i] =
            copysignf(.05f * (1.f - expf(-drive * fabsf(p))), p) *
            normalization;
    }
    amplifier->previous_nonlinear_input = input;
    return output;
}

bool gp100_bogsv_cl_amplifier_reset(
    gp100_bogsv_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t treble, bool bright)
{
    if (amplifier == NULL || gain > 99u || presence > 99u || master > 99u ||
        bass > 99u || treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->gain = amplifier->gain_target = curve(gain, 2.f);
    amplifier->presence = curve(presence, 3.f);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bass = amplifier->bass_target = curve(bass, 3.f);
    amplifier->treble = amplifier->treble_target = curve(treble, 3.f);
    amplifier->bright = bright;
    amplifier->configured = true;
    return true;
}

bool gp100_bogsv_cl_amplifier_process(
    gp100_bogsv_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t treble, bool bright, float *audio,
    size_t frames, bool enabled)
{
    static const bogsv_coefficients_t input[2] = {
        {{7.67387056f, -8.25576496f, .706500411f, 0.f},
         {1.f, -1.13062215f, .154391602f, 0.f},
         2u},
        {{8.25576496f, 4.35470963f, .0000098f, 0.f},
         {1.f, .471049488f, .000013f, 0.f},
         2u}};
    static const bogsv_coefficients_t voice[3] = {
        {{.849391282f, -1.15528131f, .305999607f, 0.f},
         {1.f, -1.28061581f, .286148012f, 0.f},
         2u},
        {{1.15528131f, -.474666893f, .072528899f, 0.f},
         {1.f, -.671873927f, .291236103f, 0.f},
         2u},
        {{1.15528131f, .936119795f, .020260001f, 0.f},
         {1.f, .939459085f, .141171902f, 0.f},
         2u}};
    static const bogsv_coefficients_t envelope = {
        {GP100_ENVELOPE_50_MS_B0, 0.f, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_50_MS_A1, GP100_ENVELOPE_50_MS_A2, 0.f},
        2u};
    size_t n;
    if (amplifier == NULL || !amplifier->configured || gain > 99u ||
        presence > 99u || master > 99u || bass > 99u || treble > 99u ||
        (frames && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->gain_target = curve(gain, 2.f);
    amplifier->master_target = (float)master * .01f;
    amplifier->bass_target = curve(bass, 3.f);
    amplifier->treble_target = curve(treble, 3.f);
    amplifier->bright = bright;
    for (n = 0u; n < frames; ++n)
    {
        float signal, level;
        size_t i;
        amplifier->gain = glide(amplifier->gain, amplifier->gain_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        amplifier->presence = curve(presence, 3.f);
        signal =
            filter(&amplifier->input_filter[0], input[0], audio[n * 2u] * 1.1f);
        signal = filter(&amplifier->input_filter[1], input[1], signal);
        signal = filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->bass, amplifier->treble),
                        signal);
        signal = amplifier->bright
                     ? filter(&amplifier->bright_filter,
                              bright_coefficients(amplifier->gain), signal)
                     : signal * amplifier->gain;
        signal *= 5.f;
        for (i = 0u; i < 3u; ++i)
            signal = filter(&amplifier->voicing_filter[i], voice[i], signal);
        signal *= amplifier->master;
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal = filter(&amplifier->presence_filter,
                        presence_coefficients(amplifier->presence), signal) *
                 59.76f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
