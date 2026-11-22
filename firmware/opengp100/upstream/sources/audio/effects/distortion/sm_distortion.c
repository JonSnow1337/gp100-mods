#include "sm_distortion.h"

#include <math.h>
#include <string.h>

typedef struct sm_coefficients { float b0, b1, b2, a0, a1, a2; }
    sm_coefficients_t;

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f, 0.0015f, 0.0032f,
    0.0048f, 0.0053f, 0.0034f, -0.0016f, -0.0092f,
    -0.0175f, -0.0233f, -0.0227f, -0.0123f, 0.0097f,
    0.0421f, 0.0807f, 0.1193f, 0.1504f, 0.1678f,
    0.1678f, 0.1504f, 0.1193f, 0.0807f, 0.0421f,
    0.0097f, -0.0123f, -0.0227f, -0.0233f, -0.0175f,
    -0.0092f, -0.0016f, 0.0034f, 0.0053f, 0.0048f,
    0.0032f, 0.0015f, 0.0001f, -0.0007f, -0.0011f
};

static float filter(gp100_sm_distortion_filter_t *state,
                    sm_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 +
                    state->x2 * c.b2 - state->y1 * c.a1 -
                    state->y2 * c.a2) / c.a0;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static void interpolate(float previous, float input, float samples[4])
{
    float step = (input - previous) / 3.0f;
    size_t i;
    for (i = 0u; i < 4u; ++i) samples[i] = previous + step * (float)i;
}

static float reconstruct(float history[40], const float samples[4])
{
    float output = 0.0f;
    size_t i;
    memmove(&history[4], history, 36u * sizeof(float));
    for (i = 0u; i < 4u; ++i) history[i] = samples[3u - i];
    for (i = 0u; i < 40u; ++i) output += reconstruction_fir[i] * history[i];
    return output;
}

static float hard_clip_stage(gp100_sm_distortion_t *effect,
                             unsigned stage, float input, float limit)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[stage], input, samples);
    for (i = 0u; i < 4u; ++i)
        samples[i] = fmaxf(-limit, fminf(limit, samples[i]));
    effect->previous_stage_input[stage] = input;
    return reconstruct(effect->oversample_history[stage], samples);
}

static float final_shape(float input)
{
    float magnitude = fabsf(input);
    float shaped;
    if (magnitude < 0.5753f)
        shaped = magnitude * 0.987f;
    else if (magnitude <= 1.5385f)
        shaped = magnitude * 1.9707f - 0.1377f -
                 magnitude * magnitude * 1.5287f +
                 magnitude * magnitude * magnitude * 0.4062f;
    else
        shaped = magnitude * 0.0323f + 0.7051f;
    return copysignf(shaped, input);
}

static float shaped_stage(gp100_sm_distortion_t *effect, float input)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[2], input, samples);
    for (i = 0u; i < 4u; ++i) samples[i] = final_shape(samples[i]);
    effect->previous_stage_input[2] = input;
    return reconstruct(effect->oversample_history[2], samples);
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

bool gp100_sm_distortion_reset(gp100_sm_distortion_t *effect,
                                    uint8_t gain, uint8_t tone,
                                    uint8_t volume)
{
    if (effect == NULL || gain > 99u || tone > 99u || volume > 99u)
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->volume = effect->volume_target = (float)volume * 0.01f;
    effect->configured = true;
    return true;
}

bool gp100_sm_distortion_process(gp100_sm_distortion_t *effect,
                                      uint8_t gain, uint8_t tone,
                                      uint8_t volume, float *audio,
                                      size_t frames, bool enabled)
{
    static const sm_coefficients_t input_c = {
        -58.4243011f, 58.4243011f, 0.0f,
        1.0f, -0.906000018f, 0.0f};
    static const sm_coefficients_t post_first_c = {
        0.998300016f, -0.998300016f, 0.0f,
        1.0f, -0.996699989f, 0.0f};
    static const sm_coefficients_t post_second_c = {
        0.282200009f, 0.037400000f, -0.244900003f,
        1.0f, -0.972999990f, 0.071800001f};
    float gain_value, tone_value, gain2;
    sm_coefficients_t gain_c, tone_c;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || tone > 99u ||
        volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u) return false;
    gain_value = (float)gain * 0.01f;
    tone_value = (float)tone * 0.0075f;
    gain2 = gain_value * gain_value;
    gain_c = (sm_coefficients_t){
        (gain2 - gain_value * 1.0472412f - 1.1873484f) * 1.5f,
        (-2.0f * gain2 + gain_value * 2.094f - 0.0005470096f) * 1.5f,
        (gain2 - gain_value * 1.0467588f + 1.1868013f) * 1.5f,
        gain2 + gain_value * 0.08654562f - 1.1873484f,
        -2.0f * gain2 + gain_value * 2.094f - 0.0005470096f,
        gain2 - gain_value * 2.1805456f + 1.1868013f};
    tone_c = (sm_coefficients_t){
        tone_value * 0.63326144f + 0.019930938f,
        0.0021589438f - tone_value * 1.276623f,
        tone_value * 0.6401393f - 0.017771995f,
        1.0f, -1.8250604f, 0.8304738f};
    effect->volume_target = (float)volume * 0.01f;
    effect->volume = glide(effect->volume, effect->volume_target);
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        output = filter(&effect->input_filter, input_c, audio[n * 2u]);
        output = hard_clip_stage(effect, 0u, output, 4.25f);
        output = filter(&effect->post_first_clip_filter, post_first_c, output);
        output = filter(&effect->gain_filter, gain_c, output);
        output = hard_clip_stage(effect, 1u, output, 2.5f);
        output = filter(&effect->post_second_clip_filter, post_second_c, output);
        output = shaped_stage(effect, output);
        output = filter(&effect->tone_filter, tone_c, output);
        output *= effect->volume * 1.5f;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
