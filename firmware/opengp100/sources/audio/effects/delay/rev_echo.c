#include "rev_echo.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define REV_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f

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

static float biquad(gp100_rev_echo_filter_t *state,
                    float input,
                    float b0,
                    float b1,
                    float b2,
                    float a1,
                    float a2)
{
    float output = input * b0 + state->x[0] * b1 + state->x[1] * b2 -
                   state->y[0] * a1 - state->y[1] * a2;

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float reverse_sample(gp100_rev_echo_t *echo,
                            size_t channel,
                            float input)
{
    size_t half = (size_t)(echo->current_delay_samples * 0.5f);
    float half_delay = echo->current_delay_samples * 0.5f;
    float fraction = half_delay - (float)half;
    size_t write = echo->input_write[channel];
    size_t reverse = echo->reverse_read[channel];
    int difference;
    int complement;
    int crossfade = -1;
    float current_reverse;
    float intermediate;
    float upper_output;
    float reverse_output;
    float previous;

    if (write < reverse) {
        difference = (int)(reverse - write);
        complement = (int)half - difference;
        if (difference >= 1 && difference <= 512) {
            crossfade = difference;
        } else if (complement >= 1 && complement <= 512) {
            crossfade = complement;
        }
    } else {
        difference = (int)(write - reverse);
        complement = (int)half - difference;
        if (complement >= 0 && complement <= 511) {
            crossfade = complement;
        } else if (difference <= 511) {
            crossfade = difference;
        }
    }
    if (crossfade >= 0) {
        echo->crossfade_q15[channel] = (uint16_t)crossfade << 6;
    }

    echo->delay_line[channel][write] = input;
    write++;
    if (write > echo->lower_written[channel]) {
        echo->lower_written[channel] = write;
    }
    if (write >= half) {
        write = 0u;
    }
    echo->input_write[channel] = write;
    reverse = half - write - 1u;
    echo->reverse_read[channel] = reverse;

    previous = reverse < echo->lower_written[channel]
        ? echo->delay_line[channel][reverse] : 0.0f;
    current_reverse = previous * (float)echo->crossfade_q15[channel] / 32768.0f;
    intermediate = current_reverse * (1.0f - fraction) +
                   echo->last_reverse_sample[channel] * fraction;
    echo->last_reverse_sample[channel] = current_reverse;

    write = echo->upper_write[channel];
    echo->delay_line[channel][write] = intermediate;
    write++;
    if (write > echo->upper_written_end[channel]) {
        echo->upper_written_end[channel] = write;
    }
    if (write >= GP100_REV_ECHO_SAMPLES) {
        write = GP100_REV_ECHO_UPPER_START;
    }
    echo->upper_write[channel] = write;
    if (write < GP100_REV_ECHO_UPPER_START + half) {
        echo->upper_read[channel] =
            write + GP100_REV_ECHO_UPPER_START - half;
    } else {
        echo->upper_read[channel] = write - half;
    }

    /* The package computes a fractional blend here but loads both operands
     * from this same address. Keep the equivalent single read explicit. */
    previous = echo->upper_read[channel] < echo->upper_written_end[channel]
        ? echo->delay_line[channel][echo->upper_read[channel]] : 0.0f;
    upper_output = biquad(&echo->upper_filter[channel], previous,
        0x1.dc1aep-5f, 0x1.dc1aep-4f, 0x1.dc1aep-5f,
        -0x1.3d0a58p+0f, 0x1.e236ccp-2f);
    reverse_output = biquad(&echo->reverse_filter[channel], intermediate,
        0x1.d490ep-5f, 0x1.d490ep-4f, 0x1.d490ep-5f,
        -0x1.380526p+0f, 0x1.ca5d04p-2f);
    echo->feedback_sample[channel] = upper_output;
    return (upper_output + reverse_output) * 0.5f;
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

bool gp100_rev_echo_reset(gp100_rev_echo_t *echo,
                               uint8_t mix,
                               float time,
                               uint8_t feedback,
                               bool tempo_sync,
                               bool trail)
{
    size_t channel;

    if (echo == NULL ||
        !parameters_valid(mix, time, feedback, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_rev_echo_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    for (channel = 0u; channel < 2u; ++channel) {
        echo->upper_write[channel] = GP100_REV_ECHO_UPPER_START;
        echo->upper_read[channel] = GP100_REV_ECHO_UPPER_START;
        echo->crossfade_q15[channel] = 1u;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_rev_echo_process(gp100_rev_echo_t *echo,
                                 uint8_t mix,
                                 float time,
                                 uint8_t feedback,
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
        !parameters_valid(mix, time, feedback, tempo_sync,
                          tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
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
                REV_ECHO_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     REV_ECHO_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 REV_ECHO_CONTROL_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->lower_written, 0, sizeof(echo->lower_written));
        memset(echo->upper_written_end, 0, sizeof(echo->upper_written_end));
        memset(echo->input_write, 0, sizeof(echo->input_write));
        memset(echo->reverse_read, 0, sizeof(echo->reverse_read));
        memset(echo->feedback_sample, 0, sizeof(echo->feedback_sample));
        memset(echo->last_reverse_sample, 0, sizeof(echo->last_reverse_sample));
        echo->upper_write[0] = GP100_REV_ECHO_UPPER_START;
        echo->upper_write[1] = GP100_REV_ECHO_UPPER_START;
        echo->upper_read[0] = GP100_REV_ECHO_UPPER_START;
        echo->upper_read[1] = GP100_REV_ECHO_UPPER_START;
        echo->crossfade_q15[0] = 1u;
        echo->crossfade_q15[1] = 1u;
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
            float write_value = echo->feedback_sample[channel] *
                                echo->current_feedback;
            float wet;

            if (enabled) {
                write_value += input[channel];
            }
            wet = reverse_sample(echo, channel, write_value);
            audio[frame * 2u + channel] =
                mix_sample(input[channel], wet, echo->current_mix,
                           enabled, trail);
        }
    }
    return true;
}
