#include "chief.h"
#include "chief_nonlinearity.h"

#include <math.h>
#include <string.h>

enum { CHIEF_GAIN, CHIEF_VOLUME, CHIEF_BASS, CHIEF_MIDDLE, CHIEF_TREBLE };

typedef struct chief_coefficients {
    float b0, b1, b2, b3, a0, a1, a2, a3;
} chief_coefficients_t;

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

static float table_coordinate(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    return (float)(index - 2u) / 251.0f;
}

static float gain_volume_curve(uint8_t raw)
{
    float x = table_coordinate(raw);
    float x2 = x * x;
    float x7 = x2 * x2 * x2 * x;
    return floorf((x + x7) * 500000.0f) * 0.000001f;
}

static float bass_curve(uint8_t raw)
{
    float x = table_coordinate(raw);
    return floorf(x * x * 10000.0f) * 0.0001f;
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.0001f, target);
    if (current > target) return fmaxf(current - 0.0001f, target);
    return target;
}

static float biquad(gp100_chief_filter_t *state,
                    chief_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                    state->y1 * c.a1 - state->y2 * c.a2) / c.a0;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static chief_coefficients_t gain_coefficients(float gain)
{
    return (chief_coefficients_t){
        gain * 41.841663f + 0.9448224f,
        0.092611484f - gain * 1.90198f,
        gain * -39.939686f - 0.8522109f,
        0.0f,
        gain + 0.9448224f,
        0.092611484f - gain * 1.90198f,
        gain * 0.90198f - 0.8522109f,
        0.0f
    };
}

static chief_coefficients_t pre_clip_coefficients(float gain)
{
    return (chief_coefficients_t){
        0.47905135f, 0.0f, -0.47905135f, 0.0f,
        gain - 1.1016492f,
        2.0447805f - gain * 1.8591025f,
        gain * 0.85910255f - 0.943596f,
        0.0f
    };
}

static float clipped_sample(gp100_chief_t *effect, float input)
{
    float samples[4];
    float step = (input - effect->previous_clip_input) / 3.0f;
    float output = 0.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index)
        samples[index] = chief_nonlinearity(effect->previous_clip_input +
                                            step * (float)index);
    memmove(&effect->oversample_history[4], effect->oversample_history,
            36u * sizeof(float));
    for (index = 0u; index < 4u; ++index)
        effect->oversample_history[index] = samples[3u - index];
    for (index = 0u; index < 40u; ++index)
        output += reconstruction_fir[index] * effect->oversample_history[index];
    effect->previous_clip_input = input;
    return output;
}

static chief_coefficients_t tone_coefficients(float bass, float middle,
                                               float treble)
{
    float t2 = treble * treble;
    float r2 = middle * middle;
    float b_r = bass * middle;
    float b_t = bass * treble;
    float r_t = middle * treble;
    float c_bm = bass * 0.1466667f * middle;
    float c_bm_3 = bass * 0.44f * middle;
    chief_coefficients_t c;
    c.b0 = bass * 0.02205095f + middle * 0.01889048f +
           treble * 0.02303876f + b_r * 0.8413287f + b_t -
           r_t * 0.022f - r2 * 0.01850923f + 0.000485121f;
    c.b1 = r_t * 0.066f - middle * 0.05254846f -
           treble * 0.06702267f - b_r * 2.374662f - b_t * 3.0f -
           bass * 0.01855298f + r2 * 0.05224257f - 0.0004081656f;
    c.b2 = middle * 0.04857619f - bass * 0.02205095f +
           treble * 0.06496124f + b_r * 2.225338f + b_t * 3.0f -
           r_t * 0.066f - r2 * 0.04895743f - 0.000485121f;
    c.b3 = bass * 0.01855298f - middle * 0.0149182f -
           treble * 0.02097733f - b_r * 0.6920046f - b_t +
           r_t * 0.022f + r2 * 0.0152241f + 0.0004081656f;
    c.a0 = bass * 1.132854f - middle * 0.004976377f +
           treble * 0.005685316f + b_r * 0.8661223f +
           b_t * 0.2468968f - r_t * 0.002197921f - bass * t2 * 0.2234357f +
           middle * t2 * 0.001537748f - r2 * treble * 0.003088914f -
           r2 * 0.01905469f - t2 * 0.005144114f + r2 * t2 * 0.003226667f +
           b_r * treble * 0.1404052f - c_bm * t2 + 0.02607546f;
    c.a1 = middle * 0.01435205f - bass * 3.129111f -
           treble * 0.01529505f - b_r * 2.44338f - b_t * 0.6861594f +
           r_t * 0.006409164f + bass * t2 * 0.6634357f -
           middle * t2 * 0.004764414f + r2 * treble * 0.008574247f +
           r2 * 0.05375437f + t2 * 0.01482057f - r2 * t2 * 0.00968f -
           b_r * treble * 0.3897385f + c_bm_3 * t2 - 0.06974743f;
    c.a2 = bass * 2.867149f - middle * 0.01362096f +
           treble * 0.01367546f + b_r * 2.288544f + b_t * 0.6331032f -
           r_t * 0.006191412f - bass * t2 * 0.6565643f +
           middle * t2 * 0.004915586f - r2 * treble * 0.007881753f -
           r2 * 0.05034798f - t2 * 0.01421589f + r2 * t2 * 0.00968f +
           b_r * treble * 0.3582615f - c_bm_3 * t2 + 0.06192815f;
    c.a3 = middle * 0.004245286f - bass * 0.8708859f -
           treble * 0.004064175f - b_r * 0.7112864f - b_t * 0.1938406f +
           r_t * 0.00198017f + bass * t2 * 0.2165643f -
           middle * t2 * 0.001688919f + r2 * treble * 0.002396419f +
           r2 * 0.0156483f + t2 * 0.004539427f - r2 * t2 * 0.003226667f -
           b_r * treble * 0.1089282f + c_bm * t2 - 0.01824896f;
    return c;
}

static float third_order(gp100_chief_filter_t *state,
                         chief_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 +
                    state->x3 * c.b3 - state->y1 * c.a1 -
                    state->y2 * c.a2 - state->y3 * c.a3) / c.a0;
    state->x3 = state->x2; state->x2 = state->x1; state->x1 = input;
    state->y3 = state->y2; state->y2 = state->y1; state->y1 = output;
    return output;
}

static bool valid(uint8_t gain, uint8_t volume, uint8_t bass,
                  uint8_t middle, uint8_t treble)
{
    return gain <= 99u && volume <= 99u && bass <= 99u &&
           middle <= 99u && treble <= 99u;
}

static void set_targets(gp100_chief_t *effect, uint8_t gain,
                        uint8_t volume, uint8_t bass, uint8_t middle,
                        uint8_t treble)
{
    effect->target[CHIEF_GAIN] = gain_volume_curve(gain);
    effect->target[CHIEF_VOLUME] = gain_volume_curve(volume);
    effect->target[CHIEF_BASS] = bass_curve(bass);
    effect->target[CHIEF_MIDDLE] = (float)middle * 0.01f;
    effect->target[CHIEF_TREBLE] = (float)treble * 0.01f;
}

bool gp100_chief_reset(gp100_chief_t *effect, uint8_t gain,
                            uint8_t volume, uint8_t bass, uint8_t middle,
                            uint8_t treble)
{
    if (effect == NULL || !valid(gain, volume, bass, middle, treble))
        return false;
    memset(effect, 0, sizeof(*effect));
    set_targets(effect, gain, volume, bass, middle, treble);
    memcpy(effect->control, effect->target, sizeof(effect->control));
    effect->configured = true;
    return true;
}

bool gp100_chief_process(gp100_chief_t *effect, uint8_t gain,
                              uint8_t volume, uint8_t bass, uint8_t middle,
                              uint8_t treble, float *audio, size_t frames,
                              bool enabled)
{
    size_t frame;
    if (effect == NULL || !effect->configured ||
        !valid(gain, volume, bass, middle, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    set_targets(effect, gain, volume, bass, middle, treble);
    for (frame = 0u; frame < frames; ++frame) {
        size_t control;
        chief_coefficients_t tone;
        float output;
        for (control = 0u; control < 5u; ++control)
            effect->control[control] = glide(effect->control[control],
                                             effect->target[control]);
        if (!enabled) continue;
        output = biquad(&effect->gain_filter,
                        gain_coefficients(effect->control[CHIEF_GAIN]),
                        audio[frame * 2u]);
        output = biquad(&effect->pre_clip_filter,
                        pre_clip_coefficients(effect->control[CHIEF_GAIN]),
                        output);
        output = clipped_sample(effect, output);
        tone = tone_coefficients(effect->control[CHIEF_BASS],
                                 effect->control[CHIEF_MIDDLE],
                                 effect->control[CHIEF_TREBLE]);
        output = third_order(&effect->tone_filter, tone, output);
        output = effect->output_x1 * 0.8894f + effect->output_y1 * 0.1106f;
        effect->output_x1 = effect->tone_filter.y1;
        effect->output_y1 = output;
        output *= effect->control[CHIEF_VOLUME] * 2.88f;
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
