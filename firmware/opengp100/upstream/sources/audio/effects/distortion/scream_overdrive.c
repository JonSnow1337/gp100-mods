#include "scream_overdrive.h"

#include <math.h>
#include <string.h>

typedef struct scream_coefficients {
    float b0, b1, b2, a0, a1, a2;
} scream_coefficients_t;

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

static const scream_coefficients_t air_coefficients = {
    1.11969995f, -1.07459998f, 0.300300002f, 1.0f,
    -1.07459998f, 0.4199f
};

static const scream_coefficients_t post_coefficients[2] = {
    {0.541398287f, 0.226820305f, 0.0463384986f, 1.0f,
     0.808575511f, 0.388842314f},
    {3.63473392f, 1.87498105f, 0.212439194f, 1.0f,
     0.964523911f, 0.1237716f}
};

static bool parameters_valid(uint8_t gain, uint8_t tone, uint8_t volume)
{
    return gain <= 99u && tone <= 99u && volume <= 99u;
}

static float gain_curve(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    float active;

    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    active = (float)(index - 2u) / 251.0f;
    return floorf(powf(active, 2.5f) * 10000.0f + 0.5f) * 0.0001f;
}

static float filter_sample(gp100_scream_filter_t *state,
                           scream_coefficients_t c,
                           float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                    state->y1 * c.a1 - state->y2 * c.a2) / c.a0;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static scream_coefficients_t gain_coefficients(float gain, bool fat)
{
    scream_coefficients_t c;
    if (!fat) {
        c.b0 = gain * 45.99104f + 5.135709f;
        c.b1 = gain * -1.90236f - 0.1506277f;
        c.b2 = gain * -44.08868f - 4.898255f;
        c.a1 = c.b1;
        c.a2 = gain * 0.9023599f - 0.3091686f;
    } else {
        c.b0 = gain * 47.18609f + 5.257604f;
        c.b1 = gain * -1.95289f - 0.1782487f;
        c.b2 = gain * -45.2332f - 5.037463f;
        c.a1 = c.b1;
        c.a2 = gain * 0.9528902f - 0.3264814f;
    }
    c.a0 = gain + 0.5466223f;
    return c;
}

static scream_coefficients_t tone_coefficients(float tone)
{
    float tone2 = tone * tone;
    scream_coefficients_t c;
    c.b0 = tone2 * 0.04877097f - tone * 0.05120952f - 0.0006621532f;
    c.b1 = -0.0002513449f;
    c.b2 = tone2 * -0.04877097f + tone * 0.05120952f + 0.0004108082f;
    c.a0 = tone2 - tone * 0.9975615f - 0.01601534f;
    c.a1 = tone2 * -1.892704f + tone * 1.892704f + 0.02054326f;
    c.a2 = tone2 * 0.8927039f - tone * 0.8951424f - 0.005080885f;
    return c;
}

static scream_coefficients_t final_low_pass(void)
{
    float omega = 6.2831855f * 13000.0f / 44100.0f;
    float sine = sinf(omega);
    float cosine = cosf(omega);
    return (scream_coefficients_t){
        (1.0f - cosine) * 0.5f, 1.0f - cosine,
        (1.0f - cosine) * 0.5f, sine / 1.414f + 1.0f,
        -2.0f * cosine, 1.0f - sine / 1.414f
    };
}

static float clipped_sample(gp100_scream_overdrive_t *effect,
                            float input)
{
    float samples[4];
    float step = (input - effect->previous_clip_input) / 3.0f;
    float drive = effect->gain * 0.5f + 2.5f;
    float level = 0.42f - effect->gain * 0.13f;
    float output = 0.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index) {
        float x = effect->previous_clip_input + step * (float)index;
        float shaped = (1.0f - expf(-fabsf(x) * drive)) * level;
        if (x < 0.0f) shaped = -shaped;
        samples[index] = shaped * 0.988f + x * 0.012000024f;
    }
    memmove(&effect->oversample_history[4], &effect->oversample_history[0],
            36u * sizeof(float));
    for (index = 0u; index < 4u; ++index)
        effect->oversample_history[index] = samples[3u - index];
    for (index = 0u; index < 40u; ++index)
        output += reconstruction_fir[index] * effect->oversample_history[index];
    effect->previous_clip_input = input;
    return output;
}

bool gp100_scream_overdrive_reset(gp100_scream_overdrive_t *effect,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume, bool fat, bool air)
{
    if (effect == NULL || !parameters_valid(gain, tone, volume)) return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = gain_curve(gain);
    effect->tone = (float)tone * 0.01f;
    effect->volume = (float)volume * 0.01f;
    effect->volume_target = effect->volume;
    effect->fat = fat;
    effect->air = air;
    effect->configured = true;
    return true;
}

bool gp100_scream_overdrive_process(
    gp100_scream_overdrive_t *effect, uint8_t gain, uint8_t tone,
    uint8_t volume, bool fat, bool air, float *audio, size_t frames,
    bool enabled)
{
    scream_coefficients_t gain_filter;
    scream_coefficients_t tone_filter;
    scream_coefficients_t low_pass = final_low_pass();
    size_t frame;
    if (effect == NULL || !effect->configured ||
        !parameters_valid(gain, tone, volume) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) return false;
    effect->gain = gain_curve(gain);
    effect->tone = (float)tone * 0.01f;
    effect->volume_target = (float)volume * 0.01f;
    effect->fat = fat;
    effect->air = air;
    if (effect->volume < effect->volume_target) {
        effect->volume += 0.005f;
        if (effect->volume > effect->volume_target) effect->volume = effect->volume_target;
    } else if (effect->volume > effect->volume_target) {
        effect->volume -= 0.005f;
        if (effect->volume < effect->volume_target) effect->volume = effect->volume_target;
    }
    gain_filter = gain_coefficients(effect->gain, fat);
    tone_filter = tone_coefficients(effect->tone);
    for (frame = 0u; frame < frames; ++frame) {
        float input, output;
        if (!enabled) continue;
        input = audio[frame * 2u];
        output = filter_sample(&effect->gain_filter, gain_filter, input);
        output = clipped_sample(effect, output) + input * 0.5f;
        output = filter_sample(&effect->tone_filter, tone_filter, output);
        if (air) output = filter_sample(&effect->air_filter, air_coefficients, output);
        output *= 1.65f * effect->volume;
        output = filter_sample(&effect->post_filter[0], post_coefficients[0], output);
        output = filter_sample(&effect->post_filter[1], post_coefficients[1], output);
        output = filter_sample(&effect->post_filter[2], low_pass, output);
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
