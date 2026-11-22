#include "vintage_wah.h"

#include "fixed_wah_internal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define VINTAGE_WAH_CONTROL_SCALE 0.01f
#define VINTAGE_WAH_CONTROL_STEP 0x1.a36e2ep-14f
#define VINTAGE_WAH_SAMPLE_RATE 44100.0f

static float move_toward(float current, float target)
{
    if (current < target) {
        current += VINTAGE_WAH_CONTROL_STEP;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= VINTAGE_WAH_CONTROL_STEP;
        return current < target ? target : current;
    }
    return target;
}

static float process_filter(
    const gp100_biquad_coefficients_t *coefficients,
    gp100_biquad_channel_state_t *state,
    float input)
{
    float output = (input * coefficients->b0 +
                    state->x1 * coefficients->b1 +
                    state->x2 * coefficients->b2 -
                    state->y1 * coefficients->a1 -
                    state->y2 * coefficients->a2) / coefficients->a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static void configure_input_filter(gp100_vintage_wah_t *wah,
                                   float omega)
{
    float sine = sinf(omega);
    float cosine = cosf(omega);
    float alpha = sine / 1.414f;

    wah->input_filter_coefficients.b0 = (cosine + 1.0f) * 0.5f;
    wah->input_filter_coefficients.b1 = -1.0f - cosine;
    wah->input_filter_coefficients.b2 =
        wah->input_filter_coefficients.b0;
    wah->input_filter_coefficients.a0 = alpha + 1.0f;
    wah->input_filter_coefficients.a1 = cosine * -2.0f;
    wah->input_filter_coefficients.a2 = 1.0f - alpha;
}

bool gp100_fixed_wah_reset(gp100_vintage_wah_t *wah,
                                uint8_t range,
                                uint8_t resonance,
                                uint8_t output_level,
                                float input_omega)
{
    if (wah == NULL || range > 99u || resonance > 99u ||
        output_level > 99u) {
        return false;
    }
    memset(wah, 0, sizeof(*wah));
    wah->range = (float)range * VINTAGE_WAH_CONTROL_SCALE;
    wah->target_resonance =
        (float)resonance * VINTAGE_WAH_CONTROL_SCALE;
    wah->current_resonance = wah->target_resonance;
    wah->target_output_level =
        (float)output_level * VINTAGE_WAH_CONTROL_SCALE;
    wah->current_output_level = wah->target_output_level;
    configure_input_filter(wah, input_omega);
    wah->configured = true;
    return true;
}

bool gp100_fixed_wah_process(gp100_vintage_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level,
                                  float resonance_blend,
                                  float dry_mix,
                                  float cutoff_low_hz,
                                  float cutoff_span_hz,
                                  float wet_gain,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled)
{
    size_t frame;

    if (wah == NULL || !wah->configured || range > 99u || resonance > 99u ||
        output_level > 99u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    wah->range = (float)range * VINTAGE_WAH_CONTROL_SCALE;
    wah->target_resonance =
        (float)resonance * VINTAGE_WAH_CONTROL_SCALE;
    wah->target_output_level =
        (float)output_level * VINTAGE_WAH_CONTROL_SCALE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float conditioned;
        float resonance_high;
        float resonance_low;
        float effective_resonance;
        float omega;
        float alpha;
        float wet;
        float output;

        wah->current_resonance = move_toward(
            wah->current_resonance, wah->target_resonance);
        wah->current_output_level = move_toward(
            wah->current_output_level, wah->target_output_level);
        if (!enabled) {
            continue;
        }

        input = interleaved_stereo[frame * 2u];
        conditioned = process_filter(&wah->input_filter_coefficients,
                                     &wah->input_filter_state, input);
        wah->sweep_position = wah->range < 0.5f
            ? wah->range * 0.4f
            : wah->range * 1.6f - 0.6f;
        resonance_high = wah->current_resonance * 9.0f + 1.0f;
        resonance_low = (resonance_high - 1.0f) *
                        (1.0f - resonance_blend) + 1.0f;
        effective_resonance = resonance_low +
            (resonance_high - resonance_low) *
            (1.0f - wah->sweep_position);
        wah->cutoff_hz = wah->sweep_position * cutoff_span_hz +
                         cutoff_low_hz;
        omega = wah->cutoff_hz / VINTAGE_WAH_SAMPLE_RATE * 6.2831855f;
        alpha = sinf(omega) / (effective_resonance * 2.0f);

        wah->wah_filter_coefficients.b0 = alpha * wet_gain;
        wah->wah_filter_coefficients.b1 = 0.0f;
        wah->wah_filter_coefficients.b2 = -alpha * wet_gain;
        wah->wah_filter_coefficients.a0 = alpha + 1.0f;
        wah->wah_filter_coefficients.a1 = cosf(omega) * -2.0f;
        wah->wah_filter_coefficients.a2 = 1.0f - alpha;

        wet = process_filter(&wah->wah_filter_coefficients,
                             &wah->wah_filter_state, conditioned);
        output = (wet + input * 0.5f * dry_mix) *
                 wah->current_output_level;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}

bool gp100_vintage_wah_reset(gp100_vintage_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level)
{
    return gp100_fixed_wah_reset(
        wah, range, resonance, output_level, 0x1.a1d818p-6f);
}

bool gp100_vintage_wah_process(gp100_vintage_wah_t *wah,
                                    uint8_t range,
                                    uint8_t resonance,
                                    uint8_t output_level,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled)
{
    return gp100_fixed_wah_process(
        wah, range, resonance, output_level,
        0.8f, 0.68f, 440.0f, 1840.0f, 7.4989f,
        interleaved_stereo, frame_count, enabled);
}
