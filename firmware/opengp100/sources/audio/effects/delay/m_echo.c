#include "m_echo.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define M_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f
#define M_ECHO_MOD_RATE 4.27427572e-5f

static bool parameters_valid(uint8_t mix, float time, uint8_t feedback,
                             bool sync, uint16_t tempo_ms)
{
    bool time_valid = isfinite(time) &&
        (sync ? time >= 0.0f && time <= 10.0f
              : time >= 20.0f && time <= 4000.0f);

    return mix <= 99u && feedback <= 99u && time_valid &&
           (!sync || tempo_ms != 0u);
}

static float modulation_delay(gp100_m_echo_t *echo,
                              size_t channel, float input)
{
    float sample_delay = 352.0f + fabsf(sinf(echo->modulation_phase[channel] *
                                             M_ECHO_MOD_RATE)) * 265.0f;
    float delayed;

    echo->modulation_line[channel][echo->modulation_write[channel]] = input;
    /* The short tap uses the current write position as its origin. */
    delayed = gp100_delay_read_linear(
        echo->modulation_line[channel], GP100_M_ECHO_MOD_SAMPLES,
        echo->modulation_write[channel], sample_delay);
    echo->modulation_write[channel]++;
    if (echo->modulation_write[channel] == GP100_M_ECHO_MOD_SAMPLES) {
        echo->modulation_write[channel] = 0u;
    }
    echo->modulation_phase[channel] += 1.0f;
    if (echo->modulation_phase[channel] >= 73500.0f) {
        echo->modulation_phase[channel] = 1.0f;
    }
    return (input + delayed) * 0.5f;
}

static float main_delay(gp100_m_echo_t *echo, size_t channel,
                        float input)
{
    gp100_m_echo_biquad_t *filter = &echo->delay_filter[channel];
    float delayed;
    float filtered;
    float feedback;

    gp100_delay_write(echo->delay_line[channel], GP100_M_ECHO_SAMPLES,
                           &echo->delay_write[channel], &echo->delay_full[channel], input);
    delayed = gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_M_ECHO_SAMPLES,
        echo->delay_write[channel], echo->current_delay_samples, echo->delay_full[channel]);
    filtered = delayed * 0x1.1b8fa4p-6f +
               filter->x[0] * 0x1.1b8fa4p-5f +
               filter->x[1] * 0x1.1b8fa4p-6f -
               filter->y[0] * -0x1.a5c5f2p+0f -
               filter->y[1] * 0x1.6efddap-1f;
    filter->x[1] = filter->x[0];
    filter->x[0] = delayed;
    filter->y[1] = filter->y[0];
    filter->y[0] = filtered;
    feedback = filtered * echo->current_feedback * 0x1.fc60dcp-1f +
               echo->high_pass_x[channel] * -0x1.fc60dcp-1f -
               echo->high_pass_y[channel] * -0x1.f8c1b8p-1f;
    echo->high_pass_x[channel] = filtered * echo->current_feedback;
    echo->high_pass_y[channel] = feedback;
    return feedback;
}

static float mix_sample(float dry, float wet, float mix, bool enabled)
{
    if (mix < 0.5f) {
        return dry + wet * mix * 2.0f;
    }
    return enabled ? wet + dry * 2.0f * (1.0f - mix) : dry + wet;
}

bool gp100_m_echo_reset(gp100_m_echo_t *echo,
                             uint8_t mix, float time, uint8_t feedback,
                             bool tempo_sync, bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, time, feedback, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_m_echo_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_m_echo_process(gp100_m_echo_t *echo,
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
    mix_delta = (echo->target_mix - echo->current_mix) * M_ECHO_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     M_ECHO_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 M_ECHO_CONTROL_GLIDE;

    for (frame = 0u; frame < frames; ++frame) {
        float input[2] = {audio[frame * 2u], audio[frame * 2u + 1u]};
        size_t channel;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            if (echo->rings_active) {
                memset(echo->delay_full, 0, sizeof(echo->delay_full));
                memset(echo->modulation_line, 0, sizeof(echo->modulation_line));
                memset(echo->delay_write, 0, sizeof(echo->delay_write));
                memset(echo->modulation_write, 0, sizeof(echo->modulation_write));
                memset(echo->modulation_phase, 0, sizeof(echo->modulation_phase));
                memset(echo->last_wet, 0, sizeof(echo->last_wet));
                echo->rings_active = false;
            }
            continue;
        }
        echo->rings_active = true;
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->last_wet[channel];
            if (enabled) {
                write_value += modulation_delay(echo, channel, input[channel]);
            }
            echo->last_wet[channel] = main_delay(echo, channel, write_value);
            audio[frame * 2u + channel] =
                mix_sample(input[channel], echo->last_wet[channel],
                           echo->current_mix, enabled);
        }
    }
    return true;
}
