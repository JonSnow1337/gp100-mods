#include "dual_echo.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define DUAL_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f
#define DUAL_ECHO_TIME_GLIDE_DIVISOR 6615.0005f

static bool time_valid(float time, bool sync)
{
    return isfinite(time) &&
        (sync ? time >= 0.0f && time <= 10.0f
              : time >= 20.0f && time <= 4000.0f);
}

static bool parameters_valid(uint8_t mix_a,
                             uint8_t feedback_a,
                             float time_a,
                             uint8_t mix_b,
                             uint8_t feedback_b,
                             float time_b,
                             bool sync_a,
                             bool sync_b,
                             uint16_t tempo_period_ms)
{
    return mix_a <= 99u && feedback_a <= 99u &&
           mix_b <= 99u && feedback_b <= 99u &&
           time_valid(time_a, sync_a) && time_valid(time_b, sync_b) &&
           (!(sync_a || sync_b) || tempo_period_ms != 0u);
}

static float delay_sample(gp100_dual_echo_t *echo,
                          size_t channel,
                          float input)
{

    if (input > 1.7f) {
        input = 1.7f;
    } else if (input < -1.7f) {
        input = -1.7f;
    }
    gp100_delay_write(echo->delay_line[channel], GP100_DUAL_ECHO_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_DUAL_ECHO_SAMPLES,
        echo->write_index[channel], echo->current_delay_samples[channel], echo->delay_full[channel]);
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

bool gp100_dual_echo_reset(gp100_dual_echo_t *echo,
                                uint8_t mix_a,
                                uint8_t feedback_a,
                                float time_a,
                                uint8_t mix_b,
                                uint8_t feedback_b,
                                float time_b,
                                bool sync_a,
                                bool sync_b,
                                bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix_a, feedback_a, time_a,
                          mix_b, feedback_b, time_b,
                          sync_a, sync_b, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_dual_echo_t, delay_line));
    echo->target_mix[0] = echo->current_mix[0] = (float)mix_a * 0.01f;
    echo->target_mix[1] = echo->current_mix[1] = (float)mix_b * 0.01f;
    echo->target_feedback[0] = echo->current_feedback[0] =
        (float)feedback_a * 0.01f;
    echo->target_feedback[1] = echo->current_feedback[1] =
        (float)feedback_b * 0.01f;
    echo->current_delay_samples[0] = gp100_effect_delay_samples((uint32_t)time_a, sync_a, 1u);
    echo->current_delay_samples[1] = gp100_effect_delay_samples((uint32_t)time_b, sync_b, 1u);
    echo->target_delay_samples[0] = echo->current_delay_samples[0];
    echo->target_delay_samples[1] = echo->current_delay_samples[1];
    echo->tempo_sync[0] = sync_a;
    echo->tempo_sync[1] = sync_b;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_dual_echo_process(gp100_dual_echo_t *echo,
                                  uint8_t mix_a,
                                  uint8_t feedback_a,
                                  float time_a,
                                  uint8_t mix_b,
                                  uint8_t feedback_b,
                                  float time_b,
                                  bool sync_a,
                                  bool sync_b,
                                  bool trail,
                                  uint16_t tempo_period_ms,
                                  float *audio,
                                  size_t frames,
                                  bool enabled)
{
    float mix_delta[2];
    float feedback_delta[2];
    float time_delta[2];
    size_t channel;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix_a, feedback_a, time_a,
                          mix_b, feedback_b, time_b,
                          sync_a, sync_b, tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix[0] = (float)mix_a * 0.01f;
    echo->target_mix[1] = (float)mix_b * 0.01f;
    echo->target_feedback[0] = (float)feedback_a * 0.01f;
    echo->target_feedback[1] = (float)feedback_b * 0.01f;
    echo->target_delay_samples[0] =
        gp100_effect_delay_samples((uint32_t)time_a, sync_a, tempo_period_ms);
    echo->target_delay_samples[1] =
        gp100_effect_delay_samples((uint32_t)time_b, sync_b, tempo_period_ms);
    echo->tempo_sync[0] = sync_a;
    echo->tempo_sync[1] = sync_b;
    echo->trail = trail;
    for (channel = 0u; channel < 2u; ++channel) {
        if (echo->tempo_sync[channel]) {
            echo->current_delay_samples[channel] =
                echo->target_delay_samples[channel];
        }
        mix_delta[channel] =
            (echo->target_mix[channel] - echo->current_mix[channel]) *
            DUAL_ECHO_CONTROL_GLIDE;
        feedback_delta[channel] =
            (echo->target_feedback[channel] -
             echo->current_feedback[channel]) * DUAL_ECHO_CONTROL_GLIDE;
        time_delta[channel] =
            (echo->target_delay_samples[channel] -
             echo->current_delay_samples[channel]) /
            DUAL_ECHO_TIME_GLIDE_DIVISOR;
    }

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->delay_full, 0, sizeof(echo->delay_full));
        memset(echo->write_index, 0, sizeof(echo->write_index));
        memset(echo->last_wet, 0, sizeof(echo->last_wet));
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        for (channel = 0u; channel < 2u; ++channel) {
            float dry = audio[frame * 2u + channel];
            float write_value;

            echo->current_mix[channel] += mix_delta[channel];
            echo->current_feedback[channel] += feedback_delta[channel];
            if (!echo->tempo_sync[channel]) {
                echo->current_delay_samples[channel] += time_delta[channel];
            }
            if (!enabled && !trail) {
                continue;
            }
            write_value = echo->last_wet[channel] *
                          echo->current_feedback[channel];
            if (enabled) {
                write_value += dry;
            }
            echo->last_wet[channel] = delay_sample(echo, channel, write_value);
            audio[frame * 2u + channel] =
                mix_sample(dry, echo->last_wet[channel],
                           echo->current_mix[channel], enabled, trail);
        }
    }
    return true;
}
