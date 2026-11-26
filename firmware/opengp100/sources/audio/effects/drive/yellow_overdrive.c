#include "yellow_overdrive.h"

#include <math.h>
#include <string.h>

typedef struct yellow_coefficients { float b0, b1, b2, a1, a2; }
    yellow_coefficients_t;

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

static float gain_curve(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    float x;
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    x = (float)(index - 2u) / 251.0f;
    return floorf(x * sqrtf(x) * 10000.0f) * 0.0001f;
}

static float biquad(gp100_yellow_filter_t *state,
                    yellow_coefficients_t c, float input)
{
    float output = input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                   state->y1 * c.a1 - state->y2 * c.a2;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static float clipped_sample(gp100_yellow_overdrive_t *effect,
                            float input)
{
    float samples[4];
    float step = (input - effect->previous_clip_input) / 3.0f;
    float positive = 1.4f - effect->gain * 0.3f;
    float negative = 0.65f - effect->gain * 0.15f;
    float output = 0.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index) {
        float x = effect->previous_clip_input + step * (float)index;
        float y = x <= 0.0f ? -(1.0f - expf(x * 2.0f)) * negative
                            : (1.0f - expf(-x)) * positive;
        samples[index] = y * 0.995f + x * 0.004999995f;
    }
    memmove(&effect->oversample_history[4], effect->oversample_history,
            36u * sizeof(float));
    for (index = 0u; index < 4u; ++index)
        effect->oversample_history[index] = samples[3u - index];
    for (index = 0u; index < 40u; ++index)
        output += reconstruction_fir[index] * effect->oversample_history[index];
    effect->previous_clip_input = input;
    return output;
}

bool gp100_yellow_overdrive_reset(gp100_yellow_overdrive_t *effect,
                                       uint8_t gain, uint8_t volume)
{
    if (effect == NULL || gain > 99u || volume > 99u) return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = gain_curve(gain);
    effect->volume = effect->volume_target = (float)volume * 0.01f;
    effect->configured = true;
    return true;
}

bool gp100_yellow_overdrive_process(
    gp100_yellow_overdrive_t *effect, uint8_t gain, uint8_t volume,
    float *audio, size_t frames, bool enabled)
{
    static const yellow_coefficients_t input_c = {
        0.952528f, -1.53268039f, 0.580153704f,
        -1.54756904f, 0.572446227f
    };
    static const yellow_coefficients_t output_c = {
        -0.059255749f, -0.059255749f, 0.0f, -0.881488502f, 0.0f
    };
    size_t frame;
    if (effect == NULL || !effect->configured || gain > 99u || volume > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) return false;
    effect->volume_target = (float)volume * 0.01f;
    if (effect->volume < effect->volume_target)
        effect->volume = fminf(effect->volume + 0.005f, effect->volume_target);
    else if (effect->volume > effect->volume_target)
        effect->volume = fmaxf(effect->volume - 0.005f, effect->volume_target);
    for (frame = 0u; frame < frames; ++frame) {
        yellow_coefficients_t drive;
        yellow_coefficients_t level;
        float conditioned, output;
        if (!enabled) continue;
        effect->gain = gain_curve(gain);
        conditioned = biquad(&effect->input_filter, input_c, audio[frame * 2u]);
        drive = (yellow_coefficients_t){
            effect->gain * 202.3787f + 7.678497f,
            effect->gain * -202.3787f - 7.580857f,
            0.0f, -0.9023599f, 0.0f
        };
        output = biquad(&effect->gain_filter, drive, conditioned) * 3.0f;
        output = clipped_sample(effect, output) + conditioned;
        output = biquad(&effect->output_filter[0], output_c, output);
        level = (yellow_coefficients_t){
            effect->volume * 0.6797478f,
            effect->volume * -0.6797478f,
            0.0f, -0.9984586f, 0.0f
        };
        output = biquad(&effect->output_filter[1], level, output);
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
