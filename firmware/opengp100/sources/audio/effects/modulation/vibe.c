#include "vibe.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define VIBE_SAMPLE_RATE 44100.0f
#define VIBE_CONTROL_GLIDE 0x1.db8cdap-13f
#define VIBE_TWO_PI 6.2831855f
#define VIBE_SWEEP_OFFSET 0x1.35b4eep-6f
#define VIBE_FEEDBACK 0x1.eb851ep-4f

static bool parameters_valid(uint8_t depth,
                             float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool rate_valid = isfinite(rate) &&
        (tempo_sync ? rate >= 0.0f && rate <= 10.0f
                    : rate >= 0.1f && rate <= 10.0f);

    return depth <= 99u && rate_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static float rate_step(float rate,
                       bool tempo_sync,
                       uint16_t tempo_period_ms)
{
    if (tempo_sync) {
        return 1.0f / ((float)tempo_period_ms * 44.1f *
                       gp100_effect_tempo_beats_for_rate(rate));
    }
    return rate / VIBE_SAMPLE_RATE;
}

static float input_biquad(gp100_vibe_biquad_t *state,
                          float input,
                          float sweep)
{
    const float c0 = 0x1.5c9976p-9f;
    const float c1 = 0x1.ece5dcp-3f;
    const float c2 = 0x1.2ce962p-10f;
    const float c3 = 0x1.84c68ap-1f;
    const float c4 = 0x1.5c9976p-10f;
    const float c5 = 0x1.8c4988p-10f;
    float b0 = sweep - c4;
    float b1 = -sweep * c1 - c0;
    float b2 = -sweep * c3 - c4;
    float a0 = sweep + c5;
    float a1 = c0 - sweep * c1;
    float a2 = c2 - sweep * c3;
    float output = (input * b0 + state->x[0] * b1 + state->x[1] * b2 -
                    state->y[0] * a1 - state->y[1] * a2) / a0;

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float all_pass(gp100_vibe_all_pass_t *state,
                      float input,
                      float sweep,
                      float offset)
{
    float b0 = sweep - offset;
    float b1 = -sweep - offset;
    float a0 = sweep + offset;
    float a1 = offset - sweep;
    float output = (input * b0 + state->previous_input * b1 -
                    state->previous_output * a1) / a0;

    state->previous_input = input;
    state->previous_output = output;
    return output;
}

static float process_path(gp100_vibe_t *vibe,
                          size_t channel,
                          float input,
                          float sweep)
{
    static const float offsets[GP100_VIBE_ALL_PASS_STAGES] = {
        0x1.b0502ep-14f, 0x1.8b3bb8p-5f, 0x1.3c2fc6p-8f
    };
    float output = input_biquad(&vibe->input_filter[channel],
                                input + vibe->last_wet[channel] *
                                    VIBE_FEEDBACK,
                                sweep);
    size_t index;

    for (index = 0u; index < GP100_VIBE_ALL_PASS_STAGES; ++index) {
        output = all_pass(&vibe->stage[channel][index], output,
                          sweep, offsets[index]);
    }
    vibe->last_wet[channel] = output;
    return input + output;
}

static void process_sample(gp100_vibe_t *vibe,
                           float input,
                           float *left,
                           float *right)
{
    float sine = sinf(vibe->current_rate_step * VIBE_TWO_PI *
                      (float)vibe->lfo_sample);
    float sweep[2] = {
        (sine + 1.0f) * 0.5f * vibe->current_depth + VIBE_SWEEP_OFFSET,
        (1.0f - sine) * 0.5f * vibe->current_depth + VIBE_SWEEP_OFFSET
    };

    vibe->lfo_sample++;
    if (vibe->current_rate_step > 0.0f &&
        (float)vibe->lfo_sample >= 1.0f / vibe->current_rate_step) {
        vibe->lfo_sample = 0u;
    }
    *left = process_path(vibe, 0u, input, sweep[0]);
    *right = process_path(vibe, 1u, input, sweep[1]);
}

bool gp100_vibe_reset(gp100_vibe_t *vibe,
                           uint8_t depth,
                           float rate,
                           bool tempo_sync)
{
    if (vibe == NULL ||
        !parameters_valid(depth, rate, tempo_sync, 1u)) {
        return false;
    }
    memset(vibe, 0, sizeof(*vibe));
    vibe->target_depth = (float)depth * 0.01f;
    vibe->current_depth = vibe->target_depth;
    vibe->rate_selector = (uint8_t)rate;
    vibe->tempo_sync = tempo_sync;
    vibe->configured = true;
    return true;
}

bool gp100_vibe_process(gp100_vibe_t *vibe,
                             uint8_t depth,
                             float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms,
                             float *interleaved_stereo,
                             size_t frame_count,
                             bool enabled)
{
    float depth_delta;
    float rate_delta;
    size_t frame;

    if (vibe == NULL || !vibe->configured ||
        !parameters_valid(depth, rate, tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    vibe->target_depth = (float)depth * 0.01f;
    vibe->target_rate_step = rate_step(rate, tempo_sync, tempo_period_ms);
    vibe->rate_selector = (uint8_t)rate;
    vibe->tempo_sync = tempo_sync;
    depth_delta = (vibe->target_depth - vibe->current_depth) *
                  VIBE_CONTROL_GLIDE;
    rate_delta = (vibe->target_rate_step - vibe->current_rate_step) *
                 VIBE_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float left;
        float right;

        if (!enabled) {
            continue;
        }
        vibe->current_depth += depth_delta;
        vibe->current_rate_step += rate_delta;
        process_sample(vibe, interleaved_stereo[frame * 2u], &left, &right);
        interleaved_stereo[frame * 2u] = left;
        interleaved_stereo[frame * 2u + 1u] = right;
    }
    return true;
}
