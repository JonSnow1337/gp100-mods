#include "phaser.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define PHASER_SAMPLE_RATE 44100.0f
#define PHASER_CONTROL_GLIDE 0x1.db8cdap-13f
#define PHASER_TWO_PI 6.2831855f
#define PHASER_SWEEP_SCALE 0x1.5eebb4p-3f
#define PHASER_SWEEP_OFFSET 0x1.6f587ep-4f
#define PHASER_COEFFICIENT_OFFSET 0x1.51beaap-7f
#define PHASER_FEEDBACK 0x1.c28f5cp-2f

static bool parameters_valid(float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return rate_valid && (!tempo_sync || tempo_period_ms != 0u);
}

static float rate_step(float rate,
                       bool tempo_sync,
                       uint16_t tempo_period_ms)
{
    if (tempo_sync) {
        return 1.0f / ((float)tempo_period_ms * 44.1f *
                       gp100_effect_tempo_beats_for_rate(rate));
    }
    return rate / PHASER_SAMPLE_RATE;
}

static float all_pass(gp100_phaser_stage_t *stage,
                      float input,
                      float sweep)
{
    float b0 = sweep - PHASER_COEFFICIENT_OFFSET;
    float b1 = -sweep - PHASER_COEFFICIENT_OFFSET;
    float a0 = sweep + PHASER_COEFFICIENT_OFFSET;
    float a1 = PHASER_COEFFICIENT_OFFSET - sweep;
    float output = (input * b0 + stage->previous_input * b1 -
                    stage->previous_output * a1) / a0;

    stage->previous_input = input;
    stage->previous_output = output;
    return output;
}

static float process_path(gp100_phaser_t *phaser,
                          size_t channel,
                          float input,
                          float sweep)
{
    float output = all_pass(&phaser->stage[channel][0], input, sweep) -
        phaser->stage[channel][GP100_PHASER_STAGES - 1u].previous_output *
        PHASER_FEEDBACK;
    size_t index;

    for (index = 1u; index < GP100_PHASER_STAGES; ++index) {
        output = all_pass(&phaser->stage[channel][index], output, sweep);
    }
    return input + output;
}

static void process_sample(gp100_phaser_t *phaser,
                           float input,
                           float *left,
                           float *right)
{
    float sine = sinf(phaser->current_rate_step * PHASER_TWO_PI *
                      (float)phaser->lfo_sample);
    float sweep[2] = {
        (sine + 1.0f) * 0.5f * PHASER_SWEEP_SCALE + PHASER_SWEEP_OFFSET,
        (1.0f - sine) * 0.5f * PHASER_SWEEP_SCALE + PHASER_SWEEP_OFFSET
    };

    phaser->lfo_sample++;
    if (phaser->current_rate_step > 0.0f &&
        (float)phaser->lfo_sample >= 1.0f / phaser->current_rate_step) {
        phaser->lfo_sample = 0u;
    }
    *left = process_path(phaser, 0u, input, sweep[0]);
    *right = process_path(phaser, 1u, input, sweep[1]);
}

bool gp100_phaser_reset(gp100_phaser_t *phaser,
                             float rate,
                             bool tempo_sync)
{
    if (phaser == NULL || !parameters_valid(rate, tempo_sync, 1u)) {
        return false;
    }
    memset(phaser, 0, sizeof(*phaser));
    phaser->rate_selector = (uint8_t)rate;
    phaser->tempo_sync = tempo_sync;
    phaser->configured = true;
    return true;
}

bool gp100_phaser_process(gp100_phaser_t *phaser,
                               float rate,
                               bool tempo_sync,
                               uint16_t tempo_period_ms,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled)
{
    float rate_delta;
    size_t frame;

    if (phaser == NULL || !phaser->configured ||
        !parameters_valid(rate, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    phaser->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    phaser->rate_selector = (uint8_t)rate;
    phaser->tempo_sync = tempo_sync;
    rate_delta = (phaser->target_rate_step - phaser->current_rate_step) *
                 PHASER_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        phaser->current_rate_step += rate_delta;
        process_sample(phaser, interleaved_stereo[frame * 2u],
                       &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
