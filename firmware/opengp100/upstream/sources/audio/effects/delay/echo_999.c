#include "echo_999.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#include "echo_999_tables.inc"

#define ECHO_999_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix, float time, uint8_t feedback,
                             bool sync, uint16_t tempo_ms)
{
    bool time_valid = isfinite(time) &&
        (sync ? time >= 0.0f && time <= 10.0f
              : time >= 20.0f && time <= 4000.0f);
    return mix <= 99u && feedback <= 99u && time_valid &&
           (!sync || tempo_ms != 0u);
}

static float filter3(gp100_echo_999_filter_t *state, float input,
                     const float b[4], const float a[3])
{
    float output = input * b[0] + state->x[0] * b[1] +
                   state->x[1] * b[2] + state->x[2] * b[3] -
                   state->y[0] * a[0] - state->y[1] * a[1] -
                   state->y[2] * a[2];
    state->x[2] = state->x[1];
    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[2] = state->y[1];
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float biquad(gp100_echo_999_filter_t *state, float input,
                    float b0, float b1, float b2, float a1, float a2)
{
    float output = input * b0 + state->x[0] * b1 + state->x[1] * b2 -
                   state->y[0] * a1 - state->y[1] * a2;
    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float delay_path(gp100_echo_999_t *echo, float input)
{
    static const float pre_b[4] = {
        0x1.7e6902p-8f, 0x1.1ecec2p-6f,
        0x1.1ecec2p-6f, 0x1.7e6902p-8f
    };
    static const float pre_a[3] = {
        -0x1.2bf0bcp+1f, 0x1.fb9af2p+0f, -0x1.2f8c62p-1f
    };
    static const float post_b[4] = {
        0x1.be1392p-8f, 0x1.4e8eaep-6f,
        0x1.4e8eaep-6f, 0x1.be1392p-8f
    };
    static const float post_a[3] = {
        -0x1.07dabcp+1f, 0x1.5b6cf4p+0f, -0x1.ee36fap-3f
    };
    float delayed;
    float normalized;
    float a0;
    float a1;
    size_t stage;

    input = filter3(&echo->delay_input_filter, input, pre_b, pre_a);
    gp100_delay_write(echo->delay_line, GP100_ECHO_999_SAMPLES,
                           &echo->write_index, &echo->delay_full, input);
    delayed = gp100_delay_read_linear_valid(
        echo->delay_line, GP100_ECHO_999_SAMPLES,
        echo->write_index, echo->current_delay_samples, echo->delay_full);

    normalized = (echo->current_delay_samples - 882.0f) / 175518.0f;
    a0 = normalized + 0x1.eeb98ap-2f;
    a1 = -normalized - 0x1.114676p-2f;
    for (stage = 0u; stage < 3u; ++stage) {
        gp100_echo_999_filter_t *filter = &echo->time_color[stage];
        float output = (delayed * 0x1.bae626p-4f +
                        filter->x[0] * 0x1.bae626p-4f -
                        filter->y[0] * a1) / a0;
        filter->x[0] = delayed;
        filter->y[0] = output;
        delayed = output;
    }
    delayed = filter3(&echo->delay_output_filter, delayed, post_b, post_a);
    delayed = biquad(&echo->delay_limiter_filter, delayed,
                     0x1.5e0eb6p-4f, 0x1.5e0eb6p-3f,
                     0x1.5e0eb6p-4f, -0x1.84351ap+0f,
                     0x1.b7718ep-1f);
    if (delayed > 1.7f) {
        return 1.7f;
    }
    return delayed < -1.7f ? -1.7f : delayed;
}

bool gp100_echo_999_reset(gp100_echo_999_t *echo,
                               uint8_t mix, float time, uint8_t feedback,
                               bool sync, bool trail)
{
    if (echo == NULL || !parameters_valid(mix, time, feedback, sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_echo_999_t, delay_line));
    echo->target_mix = echo->current_mix = echo_999_mix[mix];
    echo->target_feedback = echo->current_feedback =
        echo_999_feedback[feedback];
    echo->tempo_sync = sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_echo_999_process(gp100_echo_999_t *echo,
                                 uint8_t mix, float time, uint8_t feedback,
                                 bool sync, bool trail, uint16_t tempo_ms,
                                 float *audio, size_t frames, bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, time, feedback, sync, tempo_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = echo_999_mix[mix];
    echo->target_feedback = echo_999_feedback[feedback];
    echo->target_delay_samples = gp100_effect_delay_samples((uint32_t)time, sync, tempo_ms);
    if (echo->current_delay_samples == 0.0f || sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) * ECHO_999_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     ECHO_999_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 ECHO_999_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        echo->delay_full = false;
        echo->write_index = 0u;
        echo->last_delay = 0.0f;
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float left = audio[frame * 2u];
        float right = audio[frame * 2u + 1u];
        float dry;
        float combined;
        float output;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            continue;
        }
        dry = biquad(&echo->input_filter, left * 0.5f,
                     -0x1.dc9a86p+1f, -0x1.a8819p-3f,
                     0x1.c2126ep+1f, -0x1.2bf068p-2f,
                     -0x1.2b8e08p-2f);
        echo->last_delay = delay_path(
            echo, (enabled ? dry : 0.0f) +
                  echo->last_delay * echo->current_feedback);
        dry *= 2.0f;
        if (echo->current_mix < 0.5f) {
            combined = dry + echo->last_delay * echo->current_mix * 2.0f;
        } else if (!enabled && trail) {
            combined = dry + echo->last_delay;
        } else {
            combined = echo->last_delay + dry * 2.0f *
                       (1.0f - echo->current_mix);
        }
        output = biquad(&echo->output_filter, combined,
                        -0x1.e09f22p-3f, -0x1.19d704p-4f,
                        0x1.53b3ap-3f, -0x1.2ab48cp-4f,
                        -0x1.a5d11ep-1f);
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
        (void)right;
    }
    return true;
}
