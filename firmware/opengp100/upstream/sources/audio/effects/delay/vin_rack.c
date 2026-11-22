#include "vin_rack.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define VIN_RACK_CONTROL_GLIDE 0x1.db8cdap-13f
#define VIN_RACK_MOD_PHASE_STEP 0x1.7c6f8cp-14f

static bool parameters_valid(uint8_t mix,
                             uint8_t feedback,
                             float time,
                             uint8_t modulation,
                             uint8_t tone,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool time_valid = isfinite(time) &&
        (tempo_sync ? time >= 0.0f && time <= 10.0f
                    : time >= 20.0f && time <= 4000.0f);

    return mix <= 99u && feedback <= 99u && modulation <= 99u &&
           tone <= 99u && time_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static void update_tone(gp100_vin_rack_t *echo)
{
    float cutoff_hz = echo->current_tone * 4000.0f + 1000.0f;
    float tangent = tanf(cutoff_hz * 3.1415927f / 44100.0f);
    float squared = tangent * tangent;
    float norm = 1.0f / (tangent + squared * 0.707f + 0.707f);

    echo->tone_b[0] = squared * 0.707f * norm;
    echo->tone_b[1] = echo->tone_b[0] * 2.0f;
    echo->tone_b[2] = echo->tone_b[0];
    echo->tone_a[0] = (squared - 1.0f) * 1.414f * norm;
    echo->tone_a[1] = ((squared * 0.707f - tangent) + 0.707f) * norm;
}

static float modulate(gp100_vin_rack_t *echo,
                      size_t channel,
                      float input)
{
    size_t write = echo->mod_write_index[channel];
    float tap;
    float fraction;
    size_t first;
    size_t second;

    echo->mod_phase[channel] += echo->mod_direction[channel] *
                                VIN_RACK_MOD_PHASE_STEP;
    if (echo->mod_phase[channel] >= 1.0f) {
        echo->mod_direction[channel] = -1.0f;
        echo->mod_phase[channel] = 1.0f;
    } else if (echo->mod_phase[channel] <= 0.0f) {
        echo->mod_direction[channel] = 1.0f;
        echo->mod_phase[channel] = 0.0f;
    }

    echo->mod_line[channel][write] = input;
    tap = echo->mod_phase[channel] * 91.0f * echo->current_mod + 10.0f;
    first = (size_t)tap;
    fraction = tap - (float)first;
    first = (write + GP100_VIN_RACK_MOD_SAMPLES - 144u - first) %
            GP100_VIN_RACK_MOD_SAMPLES;
    second = first == 0u ? GP100_VIN_RACK_MOD_SAMPLES - 1u
                         : first - 1u;
    echo->mod_write_index[channel] =
        write + 1u == GP100_VIN_RACK_MOD_SAMPLES ? 0u : write + 1u;
    return echo->mod_line[channel][first] * (1.0f - fraction) +
           echo->mod_line[channel][second] * fraction;
}

static float main_delay(gp100_vin_rack_t *echo,
                        size_t channel,
                        float input)
{
    if (input > 2.0f) {
        input = 2.0f;
    } else if (input < -2.0f) {
        input = -2.0f;
    }
    gp100_delay_write(echo->delay_line[channel], GP100_VIN_RACK_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    return gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_VIN_RACK_SAMPLES,
        echo->write_index[channel], echo->current_delay_samples, echo->delay_full[channel]);
}

static float filter_tone(gp100_vin_rack_t *echo,
                         size_t channel,
                         float input)
{
    gp100_vin_rack_filter_t *state = &echo->tone_filter[channel];
    float output = input * echo->tone_b[0] +
                   state->x[0] * echo->tone_b[1] +
                   state->x[1] * echo->tone_b[2] -
                   state->y[0] * echo->tone_a[0] -
                   state->y[1] * echo->tone_a[1];

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float quantize(float input)
{
    float magnitude = floorf(fabsf(input) * 4095.0f + 0.5f) / 4095.0f;
    return copysignf(magnitude, input) * 0.8f;
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

static void reset_modulation(gp100_vin_rack_t *echo)
{
    echo->mod_phase[0] = 0.0f;
    echo->mod_phase[1] = 1.0f;
    echo->mod_direction[0] = 1.0f;
    echo->mod_direction[1] = -1.0f;
}

bool gp100_vin_rack_reset(gp100_vin_rack_t *echo,
                               uint8_t mix,
                               uint8_t feedback,
                               float time,
                               uint8_t modulation,
                               uint8_t tone,
                               bool tempo_sync,
                               bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, feedback, time, modulation, tone,
                          tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_vin_rack_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    /* The package initializer loads Time and FdBk in the opposite order
     * from its process callback. The first blocks glide from these seeds. */
    echo->current_feedback = time * 0.01f;
    echo->current_mod = (float)modulation * 0.01f;
    echo->target_tone = echo->current_tone = (float)tone * 0.01f;
    echo->current_delay_samples = gp100_effect_delay_samples(feedback, tempo_sync, 1u);
    echo->target_delay_samples = gp100_effect_delay_samples((uint32_t)time, tempo_sync, 1u);
    reset_modulation(echo);
    update_tone(echo);
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_vin_rack_process(gp100_vin_rack_t *echo,
                                 uint8_t mix,
                                 uint8_t feedback,
                                 float time,
                                 uint8_t modulation,
                                 uint8_t tone,
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
    float tone_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, feedback, time, modulation, tone,
                          tempo_sync, tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, tempo_sync, tempo_period_ms);
    echo->current_mod = (float)modulation * 0.01f;
    echo->target_tone = (float)tone * 0.01f;
    if (tempo_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                VIN_RACK_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     VIN_RACK_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 VIN_RACK_CONTROL_GLIDE;
    tone_delta = (echo->target_tone - echo->current_tone) *
                 VIN_RACK_CONTROL_GLIDE;

    for (frame = 0u; frame < frames; ++frame) {
        float input[2] = {audio[frame * 2u], audio[frame * 2u + 1u]};
        float filtered[2];
        size_t channel;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (echo->current_tone != echo->target_tone) {
            echo->current_tone += tone_delta;
            update_tone(echo);
        }
        if (!enabled && !trail) {
            if (echo->rings_active) {
                memset(echo->delay_full, 0, sizeof(echo->delay_full));
                memset(echo->mod_line, 0, sizeof(echo->mod_line));
                memset(echo->write_index, 0, sizeof(echo->write_index));
                memset(echo->mod_write_index, 0, sizeof(echo->mod_write_index));
                memset(echo->feedback_sample, 0, sizeof(echo->feedback_sample));
                reset_modulation(echo);
                echo->rings_active = false;
            }
            continue;
        }
        echo->rings_active = true;
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->feedback_sample[channel] *
                                echo->current_feedback;

            if (enabled) {
                write_value += modulate(echo, channel, input[channel]);
            }
            echo->feedback_sample[channel] =
                main_delay(echo, channel, write_value);
            filtered[channel] = filter_tone(
                echo, channel, echo->feedback_sample[channel]);
        }
        if (echo->hold_counter >= 2u) {
            echo->hold_counter = 0u;
            echo->held_sample[0] = filtered[0];
            echo->held_sample[1] = filtered[1];
        }
        echo->hold_counter++;
        for (channel = 0u; channel < 2u; ++channel) {
            float wet = quantize(echo->held_sample[channel]);
            audio[frame * 2u + channel] =
                mix_sample(input[channel], wet, echo->current_mix,
                           enabled, trail);
        }
    }
    return true;
}
