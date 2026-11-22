#include "p_echo.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define P_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix,
                             float time,
                             uint8_t feedback,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool time_valid = isfinite(time) &&
        (tempo_sync ? time >= 0.0f && time <= 10.0f
                    : time >= 20.0f && time <= 4000.0f);

    return mix <= 99u && feedback <= 99u && time_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static float delay_sample(gp100_p_echo_t *echo,
                          size_t channel,
                          float input,
                          float sample_delay)
{

    if (input > 2.0f) {
        input = 2.0f;
    } else if (input < -2.0f) {
        input = -2.0f;
    }
    gp100_delay_write(echo->delay_line[channel], GP100_P_ECHO_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_P_ECHO_SAMPLES,
        echo->write_index[channel], sample_delay, echo->delay_full[channel]);
}

static float mix_sample(float dry, float wet, float mix, bool enabled)
{
    if (mix < 0.5f) {
        return dry + wet * mix * 2.0f;
    }
    if (!enabled) {
        return dry + wet;
    }
    return wet + dry * 2.0f * (1.0f - mix);
}

bool gp100_p_echo_reset(gp100_p_echo_t *echo,
                             uint8_t mix,
                             float time,
                             uint8_t feedback,
                             bool tempo_sync,
                             bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, time, feedback, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_p_echo_t, delay_line));
    echo->target_mix = (float)mix * 0.01f;
    echo->current_mix = echo->target_mix;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->current_feedback = echo->target_feedback;
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_p_echo_process(gp100_p_echo_t *echo,
                               uint8_t mix,
                               float time,
                               uint8_t feedback,
                               bool tempo_sync,
                               bool trail,
                               uint16_t tempo_period_ms,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, time, feedback, tempo_sync,
                          tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, tempo_sync, tempo_period_ms);
    if (echo->current_delay_samples == 0.0f || tempo_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                P_ECHO_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     P_ECHO_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 P_ECHO_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float input[2];
        size_t channel;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            if (echo->rings_active) {
                memset(echo->delay_full, 0, sizeof(echo->delay_full));
                memset(echo->write_index, 0, sizeof(echo->write_index));
                memset(echo->last_wet, 0, sizeof(echo->last_wet));
                echo->rings_active = false;
            }
            continue;
        }
        echo->rings_active = true;
        input[0] = interleaved_stereo[frame * 2u];
        input[1] = interleaved_stereo[frame * 2u + 1u];
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->last_wet[channel] *
                                echo->current_feedback;

            if (enabled) {
                write_value += input[channel];
            }
            echo->last_wet[channel] =
                delay_sample(echo, channel, write_value,
                             echo->current_delay_samples);
            interleaved_stereo[frame * 2u + channel] =
                mix_sample(input[channel], echo->last_wet[channel],
                           echo->current_mix, enabled);
        }
    }
    return true;
}
