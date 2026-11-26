#include "m_echo2.h"

#include "audio/dsp/delay_line_internal.h"
#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define M_ECHO2_CONTROL_GLIDE 0x1.db8cdap-13f
#define M_ECHO2_MOD_PHASE_STEP 0x1.757dep-12f

static bool parameters_valid(uint8_t mix,
                             float time,
                             uint8_t feedback,
                             uint8_t tone,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    bool time_valid = isfinite(time) &&
        (tempo_sync ? time >= 0.0f && time <= 10.0f
                    : time >= 20.0f && time <= 4000.0f);

    return mix <= 99u && feedback <= 99u && tone <= 99u && time_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static void update_tone(gp100_m_echo2_t *echo)
{
    float cutoff = echo->tone * echo->tone * 4000.0f + 1000.0f;
    float tangent = tanf(cutoff * 3.1415927f / 44100.0f);
    float squared = tangent * tangent;
    float norm = 1.0f /
        (squared * 0.8071068f + tangent + 0.8071068f);

    echo->filter_b[0] = squared * 0.8071068f * norm;
    echo->filter_b[1] = echo->filter_b[0] * 2.0f;
    echo->filter_b[2] = echo->filter_b[0];
    echo->filter_a[0] = (squared - 1.0f) * 1.6142136f * norm;
    echo->filter_a[1] =
        (squared * 0.8071068f - tangent + 0.8071068f) * norm;
}

static float filter_sample(gp100_m_echo2_t *echo,
                           gp100_m_echo2_filter_t *state,
                           float input)
{
    float output = input * echo->filter_b[0] +
                   state->x[0] * echo->filter_b[1] +
                   state->x[1] * echo->filter_b[2] -
                   state->y[0] * echo->filter_a[0] -
                   state->y[1] * echo->filter_a[1];

    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static size_t wrapped_index(size_t write, size_t delay, size_t length)
{
    return write >= delay ? write - delay : write + length - delay;
}

static float process_graph(gp100_m_echo2_t *echo, float input)
{
    static const float fractions[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    float delay_fraction = echo->current_delay_samples -
                           floorf(echo->current_delay_samples);
    float taps[4];
    float fixed_delayed;
    float mod_delay;
    float mod_fraction;
    float modded;
    float main_delay;
    float main_input;
    size_t index;
    size_t previous;
    size_t tap;

    gp100_delay_write(echo->tap_line, GP100_M_ECHO2_SAMPLES,
                           &echo->tap_write, &echo->tap_full, input);
    for (tap = 0u; tap < 4u; ++tap) {
        size_t tap_delay =
            (size_t)(echo->current_delay_samples * fractions[tap]);
        float current;

        index = wrapped_index(echo->tap_write, tap_delay,
                              GP100_M_ECHO2_SAMPLES);
        current = echo->tap_full || index < echo->tap_write
            ? echo->tap_line[index] : 0.0f;
        taps[tap] = current * (1.0f - delay_fraction) +
                    echo->tap_previous[tap] * delay_fraction;
        echo->tap_previous[tap] = current;
        taps[tap] = filter_sample(echo, &echo->tap_filter[tap], taps[tap]);
    }
    echo->early_output = (taps[0] + taps[3]) * 0.5f;

    echo->early_line[echo->early_write] = echo->early_output;
    echo->early_write++;
    if (echo->early_write == GP100_M_ECHO2_EARLY_SAMPLES) {
        echo->early_write = 0u;
    }
    fixed_delayed = echo->early_line[echo->early_write];

    mod_delay = 56.5f *
                (sinf(echo->modulation_counter * M_ECHO2_MOD_PHASE_STEP) +
                 1.0f) * 0.5f + 10.0f;
    echo->modulation_counter += 1.0f;
    if (echo->modulation_counter > 17640.0f) {
        echo->modulation_counter = fmodf(echo->modulation_counter, 17640.0f);
    }
    echo->mod_line[echo->mod_write] = fixed_delayed;
    echo->mod_write++;
    if (echo->mod_write == GP100_M_ECHO2_MOD_SAMPLES) {
        echo->mod_write = 0u;
    }
    index = wrapped_index(echo->mod_write, (size_t)mod_delay,
                          GP100_M_ECHO2_MOD_SAMPLES);
    previous = index == 0u ? GP100_M_ECHO2_MOD_SAMPLES - 1u
                           : index - 1u;
    mod_fraction = mod_delay - floorf(mod_delay);
    modded = echo->mod_line[index] * (1.0f - mod_fraction) +
             echo->mod_line[previous] * mod_fraction;

    main_input = modded + echo->main_output * echo->current_feedback;
    if (main_input > 1.7f) {
        main_input = 1.7f;
    } else if (main_input < -1.7f) {
        main_input = -1.7f;
    }
    gp100_delay_write(echo->main_line, GP100_M_ECHO2_SAMPLES,
                           &echo->main_write, &echo->main_full, main_input);
    main_delay = echo->current_delay_samples * 0.5f;
    echo->main_output = gp100_delay_read_linear_valid(
        echo->main_line, GP100_M_ECHO2_SAMPLES,
        echo->main_write, main_delay, echo->main_full);
    echo->main_output = filter_sample(echo, &echo->main_filter,
                                      echo->main_output);
    return echo->main_output * 0.5f + echo->early_output;
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

bool gp100_m_echo2_reset(gp100_m_echo2_t *echo,
                              uint8_t mix,
                              float time,
                              uint8_t feedback,
                              uint8_t tone,
                              bool tempo_sync,
                              bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, time, feedback, tone, tempo_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_m_echo2_t, tap_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->current_delay_samples = gp100_effect_delay_samples((uint32_t)time, tempo_sync, 1u);
    echo->target_delay_samples = echo->current_delay_samples;
    echo->tone = (float)tone * 0.01f;
    echo->early_write = GP100_M_ECHO2_EARLY_SAMPLES - 1u;
    update_tone(echo);
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_m_echo2_process(gp100_m_echo2_t *echo,
                                uint8_t mix,
                                float time,
                                uint8_t feedback,
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
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, time, feedback, tone, tempo_sync,
                          tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, tempo_sync, tempo_period_ms);
    if (echo->tone != (float)tone * 0.01f) {
        echo->tone = (float)tone * 0.01f;
        update_tone(echo);
    }
    if (tempo_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    echo->tempo_sync = tempo_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                M_ECHO2_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     M_ECHO2_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 M_ECHO2_CONTROL_GLIDE;

    for (frame = 0u; frame < frames; ++frame) {
        float dry = audio[frame * 2u];
        float wet;
        float output;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        if (!tempo_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            if (echo->rings_active) {
                echo->tap_full = false;
                echo->main_full = false;
                memset(echo->early_line, 0, sizeof(echo->early_line));
                memset(echo->mod_line, 0, sizeof(echo->mod_line));
                memset(echo->tap_previous, 0, sizeof(echo->tap_previous));
                echo->tap_write = 0u;
                echo->main_write = 0u;
                echo->early_write = GP100_M_ECHO2_EARLY_SAMPLES - 1u;
                echo->mod_write = 0u;
                echo->rings_active = false;
            }
            continue;
        }
        echo->rings_active = true;
        wet = process_graph(echo, enabled ? dry * 0.5f : 0.0f);
        output = mix_sample(dry, wet, echo->current_mix, enabled, trail);
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
