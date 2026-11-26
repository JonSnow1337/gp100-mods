#include "red_haze.h"

#include <math.h>
#include <string.h>

typedef struct haze_coefficients { float b0, b1, b2, a1, a2; }
    haze_coefficients_t;

static float control_curve(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    return floorf(((float)(index - 2u) / 251.0f) * 10000.0f) * 0.0001f;
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

static float filter(gp100_red_haze_filter_t *state,
                    haze_coefficients_t c, float input)
{
    float output = input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                   state->y1 * c.a1 - state->y2 * c.a2;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static float moving_average(float history[4], float input)
{
    float output;
    memmove(&history[1], &history[0], 3u * sizeof(float));
    history[0] = input;
    output = history[0] + history[1] + history[2] + history[3];
    return output * 0.25f;
}

static float fuzzy_stage(gp100_red_haze_t *effect, float input)
{
    float midpoint = (effect->previous_preclip + input) * 0.5f;
    float phase[2] = {effect->previous_preclip, midpoint};
    float selected = 0.0f;
    size_t i;
    for (i = 0u; i < 2u; ++i) {
        float clipped = fmaxf(-1.0f, fminf(1.0f, phase[i]));
        float first = moving_average(effect->moving_average[0], clipped);
        float second = moving_average(effect->moving_average[1], first);
        if (i == 0u) selected = second;
    }
    effect->previous_preclip = input;
    return selected;
}

bool gp100_red_haze_reset(gp100_red_haze_t *effect,
                               uint8_t gain, uint8_t volume)
{
    if (effect == NULL || gain > 99u || volume > 99u) return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = effect->gain_target = control_curve(gain);
    effect->volume = effect->volume_target = control_curve(volume);
    effect->configured = true;
    return true;
}

bool gp100_red_haze_process(gp100_red_haze_t *effect,
                                 uint8_t gain, uint8_t volume, float *audio,
                                 size_t frames, bool enabled)
{
    static const haze_coefficients_t input_c = {
        0.033203125f, 0.033203125f, 0.0f, -0.93359375f, 0.0f};
    static const haze_coefficients_t preclip_c = {
        0.44921875f, 0.899414063f, 0.44921875f,
        0.568359375f, 0.229492188f};
    static const haze_coefficients_t output_c = {
        0.076416016f, -0.076416016f, 0.0f, -0.997314453f, 0.0f};
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || volume > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) return false;
    effect->gain_target = control_curve(gain);
    effect->volume_target = control_curve(volume);
    effect->gain = glide(effect->gain, effect->gain_target);
    effect->volume = glide(effect->volume, effect->volume_target);
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        output = filter(&effect->input_filter, input_c, audio[n * 2u] * 400.0f);
        output = filter(&effect->preclip_filter, preclip_c,
                        output * effect->gain);
        output = fuzzy_stage(effect, output);
        output = filter(&effect->output_filter, output_c, output);
        output *= effect->volume * 4.0f;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
