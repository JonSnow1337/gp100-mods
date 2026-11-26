#include "blues_overdrive.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct blues_coefficients {
    float b0, b1, b2, b3, a0, a1, a2, a3;
} blues_coefficients_t;

static const blues_coefficients_t intermediate_coefficients = {
    0.160844967f, -0.442408144f, 0.403395504f, -0.121832348f,
    1.0f, -2.7328186f, 2.46691561f, -0.734096706f
};

static const blues_coefficients_t output_coefficients = {
    1.00299072f, -1.99757326f, 0.994874597f, 0.0f,
    1.0f, -1.99757326f, 0.99786526f, 0.0f
};

static float coordinate(uint8_t raw)
{
    unsigned index = (unsigned)((float)raw * 2.55f);
    if (index <= 2u) return 0.0f;
    if (index >= 253u) return 1.0f;
    return (float)(index - 2u) / 251.0f;
}

static float gain_volume_curve(uint8_t raw)
{
    float x = coordinate(raw);
    float x2 = x * x;
    float x7 = x2 * x2 * x2 * x;
    return floorf((x + x7) * 5000.0f) * 0.0001f;
}

static float tone_curve(uint8_t raw)
{
    return floorf(coordinate(raw) * 10000.0f) * 0.0001f;
}

static float filter(gp100_blues_filter_t *s,
                    blues_coefficients_t c, float x)
{
    float y = (x * c.b0 + s->x1 * c.b1 + s->x2 * c.b2 + s->x3 * c.b3 -
               s->y1 * c.a1 - s->y2 * c.a2 - s->y3 * c.a3) / c.a0;
    s->x3 = s->x2; s->x2 = s->x1; s->x1 = x;
    s->y3 = s->y2; s->y2 = s->y1; s->y1 = y;
    return y;
}

static blues_coefficients_t gain_coefficients(float g, bool second)
{
    if (!second) return (blues_coefficients_t){
        g * 154.1058f + 14.52623f, g * -1.904054f - 0.07497578f,
        g * -152.2017f - 14.26609f, 0.0f, g + 1.052925f,
        g * -1.904054f - 0.07497578f, g * 0.9040537f - 0.7927873f, 0.0f};
    return (blues_coefficients_t){
        g * 52.27153f + 7.353357f, g * -1.989746f - 0.257996f,
        g * -50.28179f - 7.08606f, 0.0f, g + 0.5855147f,
        g * -1.989746f - 0.257996f, g * 0.9897457f - 0.3182178f, 0.0f};
}

static blues_coefficients_t tone_coefficients(float t)
{
    return (blues_coefficients_t){
        t * 0.5078752f + 0.03199012f,
        0.01467889f - t * 0.7827083f,
        t * 0.2748331f - 0.01731123f, 0.0f,
        1.0f, -1.397934f, 0.4566494f, 0.0f};
}

static float rational_shape(float x)
{
    const float magnitude = fabsf(x);
    const float square = magnitude * magnitude;
    const float cube = square * magnitude;
    const float output = (cube * 1.493f + square * 38.78f + magnitude * 13.61f) /
                         (cube + square * 36.04f + magnitude * 14.69f + 52.01f);

    return copysignf(output, x);
}

static float clip_stage(gp100_blues_overdrive_t *e, float input,
                        unsigned stage)
{
    float step = (input - e->previous_clip_input[stage]) / 3.0f;
    float samples[4];
    size_t i;
    for (i = 0u; i < 4u; ++i) {
        float x = e->previous_clip_input[stage] + step * (float)i;
        samples[i] = stage == 0u ? rational_shape(x)
                                 : fmaxf(-4.0f, fminf(4.0f, x));
    }
    e->previous_clip_input[stage] = input;
    return gp100_decimate_four(e->oversample_history[stage], samples);
}

bool gp100_blues_overdrive_reset(gp100_blues_overdrive_t *e,
                                      uint8_t gain, uint8_t tone,
                                      uint8_t volume)
{
    if (e == NULL || gain > 99u || tone > 99u || volume > 99u) return false;
    memset(e, 0, sizeof(*e));
    e->gain = gain_volume_curve(gain);
    e->tone = tone_curve(tone);
    e->volume = e->volume_target = gain_volume_curve(volume);
    e->configured = true;
    return true;
}

bool gp100_blues_overdrive_process(
    gp100_blues_overdrive_t *e, uint8_t gain, uint8_t tone,
    uint8_t volume, float *audio, size_t frames, bool enabled)
{
    size_t n;
    if (e == NULL || !e->configured || gain > 99u || tone > 99u ||
        volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u) return false;
    e->volume_target = gain_volume_curve(volume);
    if (e->volume < e->volume_target)
        e->volume = fminf(e->volume + 0.005f, e->volume_target);
    else if (e->volume > e->volume_target)
        e->volume = fmaxf(e->volume - 0.005f, e->volume_target);
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        e->gain = gain_volume_curve(gain);
        e->tone = tone_curve(tone);
        output = filter(&e->gain_filter[0], gain_coefficients(e->gain, false),
                        audio[n * 2u]);
        output = filter(&e->intermediate_filter, intermediate_coefficients,
                        output);
        output = clip_stage(e, output, 0u);
        output = filter(&e->gain_filter[1], gain_coefficients(e->gain, true),
                        output);
        output = clip_stage(e, output, 1u);
        output = filter(&e->tone_filter, tone_coefficients(e->tone), output);
        output = filter(&e->output_filter, output_coefficients, output);
        output *= e->volume * 0.6f;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
