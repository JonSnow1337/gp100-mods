#include "taichi_overdrive.h"

#include <math.h>
#include <string.h>

typedef struct taichi_coefficients {
    float b0, b1, b2, a0, a1, a2;
} taichi_coefficients_t;

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

static bool parameters_valid(uint8_t gain, uint8_t tone,
                             uint8_t volume, uint8_t voice)
{
    return gain <= 99u && tone <= 99u && volume <= 99u && voice <= 99u;
}

/* Both factory Voice tables are truncated to four decimal places. The init
 * table uses x^(3/2); the processing table uses sqrt(x). */
static float voice_curve(uint8_t raw, bool processing_curve)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    float active;
    float shaped;
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    active = (float)(index - 2u) / 251.0f;
    shaped = processing_curve ? sqrtf(active) : active * sqrtf(active);
    return floorf(shaped * 10000.0f) * 0.0001f;
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

static taichi_coefficients_t input_coefficients(float gain, float voice)
{
    float gain2 = gain * gain;
    float gv = gain * voice;
    float gain2_voice = gain2 * voice;
    taichi_coefficients_t c;
    c.b0 = voice * 0.01841169f - gain * 0.9797574f +
           gv * 0.0621118f - gain2_voice * 0.08032641f + gain2 -
           0.02228306f;
    c.b1 = gain * 0.1771311f - voice * 0.0003213056f -
           gv * 0.1606528f + gain2_voice * 0.1606528f -
           gain2 * 0.1767181f - 0.00006041945f;
    c.b2 = gain * 0.8034524f - voice * 0.01809038f +
           gv * 0.09854101f - gain2_voice * 0.08032641f -
           gain2 * 0.8232819f + 0.02151577f;
    c.a0 = voice * 0.01841169f - gain * 0.0690272f +
           gv * 0.0621118f - gain2_voice * 0.08032641f +
           gain2 * 0.08926978f - 0.0204616f;
    c.a1 = c.b1;
    c.a2 = gv * 0.09854101f - voice * 0.01809038f -
           gain * 0.1072779f - gain2_voice * 0.08032641f +
           gain2 * 0.08744832f + 0.01969431f;
    return c;
}

static float biquad(gp100_taichi_biquad_state_t *state,
                    taichi_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                    state->y1 * c.a1 - state->y2 * c.a2) / c.a0;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float asymmetric_shape(float input)
{
    if (input <= 0.0f)
        return -(1.0f - expf(input * 0.8f)) * 1.6f;
    return 1.0f - expf(-input);
}

static float oversampled_clip(gp100_taichi_overdrive_t *effect,
                              float input)
{
    float step = (input - effect->previous_clip_input) / 3.0f;
    float shaped[4];
    float output = 0.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index)
        shaped[index] = asymmetric_shape(effect->previous_clip_input +
                                         step * (float)index);
    memmove(&effect->oversample_history[4], effect->oversample_history,
            36u * sizeof(float));
    for (index = 0u; index < 4u; ++index)
        effect->oversample_history[index] = shaped[3u - index];
    for (index = 0u; index < 40u; ++index)
        output += reconstruction_fir[index] * effect->oversample_history[index];
    effect->previous_clip_input = input;
    return output;
}

static float output_filter(gp100_taichi_overdrive_t *effect,
                           float input)
{
    const float b = -0.0687143505f;
    float a0 = effect->tone - 1.268714f;
    float a1 = 1.131286f - effect->tone;
    float output = (input * b + effect->output_x1 * b -
                    effect->output_y1 * a1) / a0;
    effect->output_x1 = input;
    effect->output_y1 = output;
    return output;
}

bool gp100_taichi_overdrive_reset(gp100_taichi_overdrive_t *effect,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume, uint8_t voice)
{
    if (effect == NULL || !parameters_valid(gain, tone, volume, voice))
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = (float)gain * 0.01f;
    effect->tone = (float)tone * 0.01f;
    effect->volume = effect->volume_target = (float)volume * 0.01f;
    effect->voice = voice_curve(voice, false);
    effect->voice_target = effect->voice;
    effect->configured = true;
    return true;
}

bool gp100_taichi_overdrive_process(
    gp100_taichi_overdrive_t *effect, uint8_t gain, uint8_t tone,
    uint8_t volume, uint8_t voice, float *audio, size_t frames, bool enabled)
{
    size_t frame;
    if (effect == NULL || !effect->configured ||
        !parameters_valid(gain, tone, volume, voice) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    effect->volume_target = (float)volume * 0.01f;
    effect->voice_target = voice_curve(voice, true);
    effect->volume = glide(effect->volume, effect->volume_target);
    for (frame = 0u; frame < frames; ++frame) {
        taichi_coefficients_t input_filter;
        float input;
        float output;
        if (!enabled) continue;
        effect->gain = (float)gain * 0.01f;
        effect->tone = (float)tone * 0.01f;
        effect->voice = effect->voice_target;
        input_filter = input_coefficients(effect->gain, effect->voice);
        input = audio[frame * 2u];
        output = biquad(&effect->input_filter, input_filter, input);
        output = oversampled_clip(effect, output) + input * 1.5f;
        output = output_filter(effect, output);
        output *= effect->volume * 0.21f;
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
