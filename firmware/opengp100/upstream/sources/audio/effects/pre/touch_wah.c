#include "touch_wah.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define TOUCH_WAH_ENVELOPE_STEP 0x1.db8b7p-13f
#define TOUCH_WAH_SAMPLE_RATE 44100.0f

static void set_parameters(gp100_touch_wah_t *wah,
                           uint8_t sense,
                           uint8_t range,
                           uint8_t resonance,
                           uint8_t mix,
                           uint8_t mode)
{
    wah->sense = (float)sense * 0.01f;
    wah->range = (float)range * 0.01f;
    wah->resonance = (float)resonance * 0.01f;
    wah->mix = (float)mix * 0.01f;
    wah->mode = mode;
}

static float move_envelope(float current, float target)
{
    if (current < target) {
        current += TOUCH_WAH_ENVELOPE_STEP;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= TOUCH_WAH_ENVELOPE_STEP;
        return current < target ? target : current;
    }
    return target;
}

static float process_filter(gp100_touch_wah_t *wah, float input)
{
    gp100_biquad_channel_state_t *state = &wah->filter_state;
    const gp100_biquad_coefficients_t *c = &wah->filter_coefficients;
    float output = (input * c->b0 + state->x1 * c->b1 +
                    state->x2 * c->b2 - state->y1 * c->a1 -
                    state->y2 * c->a2) / c->a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

bool gp100_touch_wah_reset(gp100_touch_wah_t *wah,
                                uint8_t sense,
                                uint8_t range,
                                uint8_t resonance,
                                uint8_t mix,
                                uint8_t mode)
{
    if (wah == NULL || sense > 99u || range > 99u || resonance > 99u ||
        mix > 99u || mode > 3u) {
        return false;
    }
    memset(wah, 0, sizeof(*wah));
    set_parameters(wah, sense, range, resonance, mix, mode);
    wah->configured = true;
    return true;
}

bool gp100_touch_wah_process(gp100_touch_wah_t *wah,
                                  uint8_t sense,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t mix,
                                  uint8_t mode,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled)
{
    size_t frame;

    if (wah == NULL || !wah->configured || sense > 99u || range > 99u ||
        resonance > 99u || mix > 99u || mode > 3u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_parameters(wah, sense, range, resonance, mix, mode);

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float target;
        float clamped;
        float position;
        float low_hz;
        float high_hz;
        float omega;
        float resonance_high;
        float resonance_low;
        float effective_resonance;
        float alpha;
        float wet;
        float output;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        target = fabsf(input) * (wah->mode == 1u ? 160.0f : 80.0f) *
                 wah->sense;
        wah->envelope = move_envelope(wah->envelope, target);
        clamped = wah->envelope > 1.0f ? 1.0f : wah->envelope;
        position = clamped < 0.5f ? clamped * 0.2f
                                  : clamped * 1.8f - 0.8f;
        wah->envelope_position = position;

        low_hz = wah->mode == 1u ? 100.0f : 250.0f;
        high_hz = wah->mode == 1u ? 1000.0f : 2100.0f;
        high_hz += (wah->range - 0.5f) * 1000.0f;
        wah->cutoff_hz = low_hz + (high_hz - low_hz) * position;
        omega = wah->cutoff_hz * 6.2831855f / TOUCH_WAH_SAMPLE_RATE;

        resonance_high = wah->resonance * 9.0f + 1.0f;
        resonance_low = (resonance_high - 1.0f) * 0.19999999f + 1.0f;
        effective_resonance = resonance_low +
            (resonance_high - resonance_low) * (1.0f - position);
        alpha = sinf(omega) / (effective_resonance * 2.0f);
        wah->filter_coefficients.b0 = alpha * 7.0795f;
        wah->filter_coefficients.b1 = 0.0f;
        wah->filter_coefficients.b2 = -alpha * 7.0795f;
        wah->filter_coefficients.a0 = alpha + 1.0f;
        wah->filter_coefficients.a1 = cosf(omega) * -2.0f;
        wah->filter_coefficients.a2 = 1.0f - alpha;

        wet = process_filter(wah, input);
        output = wet * wah->mix + input * (1.0f - wah->mix);
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
