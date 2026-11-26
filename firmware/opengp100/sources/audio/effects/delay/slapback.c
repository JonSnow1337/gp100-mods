#include "slapback.h"

#include "audio/dsp/delay_line_internal.h"

#include <math.h>
#include <string.h>

#define SLAPBACK_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix, float time, uint8_t feedback)
{
    return mix <= 99u && feedback <= 99u && isfinite(time) &&
           time >= 20.0f && time <= 4000.0f;
}

static float delay_samples(float time)
{
    float samples = (float)(uint32_t)time * 44.1f;
    if (samples < 882.0f) {
        return 882.0f;
    }
    return samples > 13230.0f ? 13230.0f : samples;
}

static float delay_sample(gp100_slapback_t *echo, size_t channel,
                          float input)
{
    size_t pre_read;

    echo->pre_delay[channel][echo->pre_write[channel]] = input;
    echo->pre_write[channel]++;
    if (echo->pre_write[channel] == GP100_SLAPBACK_PRE_SAMPLES) {
        echo->pre_write[channel] = 0u;
    }
    pre_read = (echo->pre_write[channel] + 50u) %
               GP100_SLAPBACK_PRE_SAMPLES;
    input = echo->pre_delay[channel][pre_read];
    gp100_delay_write(echo->delay_line[channel], GP100_SLAPBACK_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_SLAPBACK_SAMPLES,
        echo->write_index[channel], echo->current_delay_samples, echo->delay_full[channel]);
}

static float color(gp100_slapback_filter_t *state, float input)
{
    float output = input * 0x1.138edcp-6f +
                   state->x[0] * 0x1.138edcp-5f +
                   state->x[1] * 0x1.138edcp-6f -
                   state->y[0] * -0x1.99de9p+0f -
                   state->y[1] * 0x1.562efcp-1f;
    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
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

bool gp100_slapback_reset(gp100_slapback_t *echo,
                               uint8_t mix, float time,
                               uint8_t feedback, bool trail)
{
    if (echo == NULL || !parameters_valid(mix, time, feedback)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_slapback_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_slapback_process(gp100_slapback_t *echo,
                                 uint8_t mix, float time,
                                 uint8_t feedback, bool trail,
                                 float *audio, size_t frames, bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, time, feedback) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples = delay_samples(time);
    if (echo->current_delay_samples == 0.0f) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) * SLAPBACK_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     SLAPBACK_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 SLAPBACK_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->delay_full, 0, sizeof(echo->delay_full));
        memset(echo->pre_delay, 0, sizeof(echo->pre_delay));
        memset(echo->write_index, 0, sizeof(echo->write_index));
        memset(echo->pre_write, 0, sizeof(echo->pre_write));
        memset(echo->last_wet, 0, sizeof(echo->last_wet));
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float input[2] = {audio[frame * 2u], audio[frame * 2u + 1u]};
        size_t channel;
        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        echo->current_delay_samples += time_delta;
        if (!enabled && !trail) {
            continue;
        }
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->last_wet[channel] *
                                echo->current_feedback;
            float wet;
            if (enabled) {
                write_value += input[channel];
            }
            echo->last_wet[channel] = delay_sample(echo, channel,
                                                   write_value);
            wet = color(&echo->color[channel], echo->last_wet[channel]);
            audio[frame * 2u + channel] =
                mix_sample(input[channel], wet, echo->current_mix,
                           enabled, trail);
        }
    }
    return true;
}
