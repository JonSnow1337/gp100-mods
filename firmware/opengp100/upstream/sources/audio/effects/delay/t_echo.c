#include "t_echo.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define T_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix, float time, uint8_t feedback,
                             bool sync, uint16_t tempo_ms)
{
    bool time_valid = isfinite(time) &&
        (sync ? time >= 0.0f && time <= 10.0f
              : time >= 20.0f && time <= 4000.0f);
    return mix <= 99u && feedback <= 99u && time_valid &&
           (!sync || tempo_ms != 0u);
}

static float delay_sample(gp100_t_echo_t *echo, size_t channel,
                          float input)
{

    if (input > 1.7f) {
        input = 1.7f;
    } else if (input < -1.7f) {
        input = -1.7f;
    }
    gp100_delay_write(echo->delay_line[channel], GP100_T_ECHO_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_T_ECHO_SAMPLES,
        echo->write_index[channel], echo->current_delay_samples, echo->delay_full[channel]);
}

static float biquad(gp100_t_echo_biquad_t *state, float input,
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

static float color_sample(gp100_t_echo_t *echo, size_t channel,
                          float input)
{
    float stage = biquad(&echo->color[channel][0], input,
                         0x1.244cacp-6f, 0x1.244cacp-5f,
                         0x1.244cacp-6f, -0x1.b2c558p+0f,
                         0x1.8a1446p-1f);
    return biquad(&echo->color[channel][1], stage,
                  0x1.40fa04p-5f, 0x1.40fa04p-4f,
                  0x1.40fa04p-5f, -0x1.95da4ep+0f,
                  0x1.7bf31ep-1f) * 0.5f;
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

bool gp100_t_echo_reset(gp100_t_echo_t *echo,
                             uint8_t mix, float time, uint8_t feedback,
                             bool tempo_sync, bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, time, feedback, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_t_echo_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_t_echo_process(gp100_t_echo_t *echo,
                               uint8_t mix, float time, uint8_t feedback,
                               bool tempo_sync, bool trail,
                               uint16_t tempo_period_ms,
                               float *audio, size_t frames, bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, time, feedback, tempo_sync, tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples = gp100_effect_delay_samples((uint32_t)time, tempo_sync,
                                               tempo_period_ms);
    if (echo->current_delay_samples == 0.0f || tempo_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                T_ECHO_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     T_ECHO_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 T_ECHO_CONTROL_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->delay_full, 0, sizeof(echo->delay_full));
        memset(echo->write_index, 0, sizeof(echo->write_index));
        memset(echo->raw_delay, 0, sizeof(echo->raw_delay));
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float input[2] = {audio[frame * 2u], audio[frame * 2u + 1u]};
        size_t channel;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            continue;
        }
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->raw_delay[channel] *
                                echo->current_feedback;
            float wet;
            if (enabled) {
                write_value += input[channel];
            }
            echo->raw_delay[channel] = delay_sample(echo, channel,
                                                    write_value);
            wet = color_sample(echo, channel, echo->raw_delay[channel]);
            audio[frame * 2u + channel] =
                mix_sample(input[channel], wet, echo->current_mix,
                           enabled, trail);
        }
    }
    return true;
}
