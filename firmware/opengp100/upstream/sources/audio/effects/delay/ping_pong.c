#include "ping_pong.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define PING_PONG_CONTROL_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix,
                             uint8_t feedback,
                             float time,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool time_valid = isfinite(time) &&
        (tempo_sync ? time >= 0.0f && time <= 10.0f
                    : time >= 20.0f && time <= 4000.0f);

    return mix <= 99u && feedback <= 99u && time_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static void initialize_color(gp100_ping_pong_t *echo)
{
    float angle = 0x1.40f83ap+0f;
    float sine = sinf(angle);
    float cosine = cosf(angle);
    gp100_ping_pong_coefficients_t *low =
        &echo->color_coefficients[0];
    gp100_ping_pong_coefficients_t *high =
        &echo->color_coefficients[1];

    low->b[0] = (1.0f - cosine) * 0.5f;
    low->b[1] = 1.0f - cosine;
    low->b[2] = low->b[0];
    low->a[0] = sine * 0.5f + 1.0f;
    low->a[1] = cosine * -2.0f;
    low->a[2] = 1.0f - sine * 0.5f;

    angle = 0x1.d2dd6ap-5f;
    sine = sinf(angle);
    cosine = cosf(angle);
    high->b[0] = (cosine + 1.0f) * 0.5f;
    high->b[1] = -1.0f - cosine;
    high->b[2] = high->b[0];
    high->a[0] = sine / 1.6f + 1.0f;
    high->a[1] = cosine * -2.0f;
    high->a[2] = 1.0f - sine / 1.6f;
}

static float delay_sample(gp100_ping_pong_t *echo,
                          size_t path,
                          float input)
{

    if (input > 2.0f) {
        input = 2.0f;
    } else if (input < -2.0f) {
        input = -2.0f;
    }
    gp100_delay_write(echo->delay_line[path], GP100_PING_PONG_SAMPLES,
                           &echo->write_index[path], &echo->delay_full[path], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[path], GP100_PING_PONG_SAMPLES,
        echo->write_index[path], echo->current_delay_samples, echo->delay_full[path]);
}

static float filter_sample(gp100_ping_pong_filter_t *state,
                           const gp100_ping_pong_coefficients_t *c,
                           float input)
{
    float output = (input * c->b[0] + state->x[0] * c->b[1] +
                    state->x[1] * c->b[2] - state->y[0] * c->a[1] -
                    state->y[1] * c->a[2]) / c->a[0];

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float color_sample(gp100_ping_pong_t *echo,
                          size_t path,
                          float input)
{
    input = filter_sample(&echo->color[path][0],
                          &echo->color_coefficients[0], input);
    return filter_sample(&echo->color[path][1],
                         &echo->color_coefficients[1], input);
}

static float mix_sample(float dry, float wet, float mix,
                        bool enabled, bool trail)
{
    if (mix < 0.5f) {
        return dry + wet * mix * 2.0f;
    }
    if (!enabled && trail) {
        return dry + wet;
    }
    return wet + dry * 2.0f * (1.0f - mix);
}

bool gp100_ping_pong_reset(gp100_ping_pong_t *echo,
                                uint8_t mix,
                                uint8_t feedback,
                                float time,
                                bool tempo_sync,
                                bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, feedback, time, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_ping_pong_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->current_delay_samples = gp100_effect_delay_samples((uint32_t)time, tempo_sync, 1u);
    echo->target_delay_samples = echo->current_delay_samples;
    initialize_color(echo);
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_ping_pong_process(gp100_ping_pong_t *echo,
                                  uint8_t mix,
                                  uint8_t feedback,
                                  float time,
                                  bool tempo_sync,
                                  bool trail,
                                  uint16_t tempo_period_ms,
                                  float *audio,
                                  size_t frames,
                                  bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, feedback, time, tempo_sync,
                          tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, tempo_sync, tempo_period_ms);
    if (tempo_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                PING_PONG_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     PING_PONG_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 PING_PONG_CONTROL_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->delay_full, 0, sizeof(echo->delay_full));
        memset(echo->write_index, 0, sizeof(echo->write_index));
        memset(echo->loop_output, 0, sizeof(echo->loop_output));
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float dry_left = audio[frame * 2u];
        float dry_right = audio[frame * 2u + 1u];
        float path_input[2];

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }

        if (!enabled && !trail) {
            continue;
        }
        path_input[0] = echo->loop_output[1];
        path_input[1] = echo->loop_output[0] * echo->current_feedback;
        if (enabled) {
            path_input[1] += dry_left * 0.5f;
        }
        echo->loop_output[0] = color_sample(
            echo, 0u, delay_sample(echo, 0u, path_input[0]));
        echo->loop_output[1] = color_sample(
            echo, 1u, delay_sample(echo, 1u, path_input[1]));

        audio[frame * 2u] = mix_sample(
            dry_right, echo->loop_output[1], echo->current_mix,
            enabled, trail);
        audio[frame * 2u + 1u] = mix_sample(
            dry_left, echo->loop_output[0], echo->current_mix,
            enabled, trail);
    }
    return true;
}
