#include "foxy30tb_amplifier.h"

#include "power_envelope_internal.h"

#include <math.h>
#include <string.h>

typedef struct foxy30tb_coefficients
{
    float b[4];
    float a[4];
    size_t order;
} foxy30tb_coefficients_t;

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float control_curve(uint8_t raw, float exponent)
{
    float index = floorf((float)raw * 2.55f) - 2.0f;
    if (index < 0.0f)
        index = 0.0f;
    if (index > 251.0f)
        index = 251.0f;
    return floorf(powf(index / 251.0f, exponent) * 10000.0f) * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_foxy30tb_filter_t *state,
                    foxy30tb_coefficients_t coefficients, float input)
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

static foxy30tb_coefficients_t volume_coefficients(float volume)
{
    float squared = volume * volume;
    return (foxy30tb_coefficients_t){
        {squared - volume * 1.377929f, squared * -2.0f + volume * 2.0f,
         squared - volume * .6220711f, 0.0f},
        {squared - volume * .9517538f - .4444089f,
         squared * -2.0f + volume * 2.0f - .03646731f,
         squared - volume * 1.048246f + .4079415f, 0.0f},
        2u};
}

static foxy30tb_coefficients_t character_tone_coefficients(float bass,
                                                           float treble)
{
    float squared = bass * bass;
    return (foxy30tb_coefficients_t){
        {squared * .9911131f * treble - treble * .00103443f - bass * treble -
             bass * .140283f + squared * .1403464f - .0001370405f,
         bass * .3558829f + treble * .001029112f + bass * 3.002054f * treble -
             squared * 2.973339f * treble - squared * .3559217f + .00008770166f,
         treble * .00103443f - bass * .2958068f - bass * 3.004097f * treble +
             squared * 2.973339f * treble + squared * .2957434f + .0001370405f,
         bass * .08020683f - treble * .001029112f + bass * 1.002043f * treble -
             squared * .9911131f * treble - squared * .08016807f -
             .00008770166f},
        {squared * 1.370547f - bass * 1.381514f - .001421007f,
         squared * -3.568348f + bass * 3.599663f + .0008659945f,
         squared * 3.029995f - bass * 3.05867f + .001418441f,
         squared * -.8321937f + bass * .8405258f - .0008685599f},
        3u};
}

static foxy30tb_coefficients_t cut_contour_coefficients(float cut)
{
    float scale = powf(10.0f, (10.0f - cut * 40.0f) / 20.0f);
    float tangent = tanf(.284951717f);
    float forward = 2.0f / (scale + 1.0f);
    float reverse = 2.0f / (1.0f / scale + 1.0f);
    float denominator = (tangent + forward) * (tangent + 1.0f);
    return (foxy30tb_coefficients_t){
        {(tangent + reverse) * (tangent - 1.0f) / denominator,
         ((tangent + reverse) * (tangent + 1.0f) +
          (tangent - reverse) * (tangent - 1.0f)) /
             denominator,
         (tangent - reverse) * (tangent + 1.0f) / denominator, 0.0f},
        {1.0f,
         ((tangent + forward) * (tangent - 1.0f) +
          (tangent - forward) * (tangent + 1.0f)) /
             denominator,
         (tangent - forward) * (tangent - 1.0f) / denominator, 0.0f},
        2u};
}

static foxy30tb_coefficients_t cut_shelf_coefficients(float cut)
{
    const float angle = 1.42475855f;
    float scale = powf(10.0f, cut * -12.0f / 40.0f);
    float q = sinf(angle) / 1.414f;
    float shared = -2.0f * cosf(angle);
    return (foxy30tb_coefficients_t){
        {q * scale + 1.0f, shared, 1.0f - q * scale, 0.0f},
        {q / scale + 1.0f, shared, 1.0f - q / scale, 0.0f},
        2u};
}

static float nonlinear_stage(gp100_foxy30tb_amplifier_t *amplifier,
                             float input, float envelope)
{
    float drive = envelope * 5.0f + 6.0f;
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
        output += reconstruction_fir[i] * amplifier->fir_history[i];
    for (i = 0u; i < 4u; ++i)
    {
        float sample = point[3u - i];
        amplifier->fir_history[i] =
            copysignf(.1f * (1.0f - expf(-drive * fabsf(sample))), sample) *
            normalization;
    }
    amplifier->previous_nonlinear_input = input;
    return output;
}

static bool controls_valid(uint8_t volume, uint8_t cut, uint8_t master,
                           uint8_t bass, uint8_t treble)
{
    return volume <= 99u && cut <= 99u && master <= 99u && bass <= 99u &&
           treble <= 99u;
}

bool gp100_foxy30tb_amplifier_reset(
    gp100_foxy30tb_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, uint8_t bass, uint8_t treble, bool character)
{
    if (amplifier == NULL || !controls_valid(volume, cut, master, bass, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume = amplifier->volume_target = control_curve(volume, 2.0f);
    amplifier->cut = amplifier->cut_target = control_curve(cut, 3.0f);
    amplifier->master = amplifier->master_target = (float)master * .01f;
    amplifier->bass = amplifier->bass_target = control_curve(bass, 3.0f) * .9f;
    amplifier->treble = amplifier->treble_target = control_curve(treble, 3.0f);
    amplifier->character = character;
    amplifier->configured = true;
    return true;
}

bool gp100_foxy30tb_amplifier_process(
    gp100_foxy30tb_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, uint8_t bass, uint8_t treble, bool character, float *audio,
    size_t frames, bool enabled)
{
    static const foxy30tb_coefficients_t input[2] = {
        {{6.59388924f, -9.44703102f, 2.85672092f, 0.f},
         {1.f, -1.42653096f, .427327514f, 0.f},
         2u},
        {{9.44703102f, 8.1678772f, .8710531f, 0.f},
         {1.f, .742128074f, .0126189f, 0.f},
         2u}};
    static const foxy30tb_coefficients_t envelope = {
        {GP100_ENVELOPE_10_MS_B0, 0.f, 0.f, 0.f},
        {1.f, GP100_ENVELOPE_10_MS_A1, GP100_ENVELOPE_10_MS_A2, 0.f},
        2u};
    size_t frame;
    size_t i;
    if (amplifier == NULL || !amplifier->configured ||
        !controls_valid(volume, cut, master, bass, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->volume_target = control_curve(volume, 2.0f);
    amplifier->cut_target = control_curve(cut, 3.0f);
    amplifier->master_target = (float)master * .01f;
    amplifier->bass_target = control_curve(bass, 3.0f) * .9f;
    amplifier->treble_target = control_curve(treble, 3.0f);
    amplifier->character = character;
    for (frame = 0u; frame < frames; ++frame)
    {
        float signal;
        float level;
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->cut = glide(amplifier->cut, amplifier->cut_target);
        amplifier->master = glide(amplifier->master, amplifier->master_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        signal = audio[frame * 2u] * 1.1f;
        for (i = 0u; i < 2u; ++i)
            signal = filter(&amplifier->input_filter[i], input[i], signal);
        signal = filter(&amplifier->volume_filter,
                        volume_coefficients(amplifier->volume), signal);
        if (!amplifier->character)
            signal = filter(
                &amplifier->character_tone_filter,
                character_tone_coefficients(amplifier->bass, amplifier->treble),
                signal);
        signal *= 6.0f * amplifier->master;
        signal = filter(&amplifier->cut_contour_filter,
                        cut_contour_coefficients(amplifier->cut), signal);
        level = filter(&amplifier->envelope_filter, envelope, fabsf(signal));
        signal = nonlinear_stage(amplifier, signal, level);
        signal = filter(&amplifier->cut_shelf_filter,
                        cut_shelf_coefficients(amplifier->cut), signal);
        signal *= 66.584f;
        audio[frame * 2u] = signal;
        audio[frame * 2u + 1u] = signal;
    }
    return true;
}
