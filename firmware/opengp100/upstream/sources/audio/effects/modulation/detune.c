#include "detune.h"

#include "audio/dsp/pitch_window_internal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static bool parameters_valid(int8_t cents, uint8_t wet, uint8_t dry)
{
    return cents >= -50 && cents <= 50 && wet <= 99u && dry <= 99u;
}

static float delayed_sample(const gp100_detune_channel_t *channel,
                            uint32_t phase)
{
    size_t delay = phase >> 8;
    size_t newer = channel->write_index >= delay
        ? channel->write_index - delay
        : channel->write_index + GP100_DETUNE_DELAY_SAMPLES - delay;
    size_t older = newer == 0u ? GP100_DETUNE_DELAY_SAMPLES - 1u
                               : newer - 1u;
    float fraction = (float)(phase & 0xffu);

    return ((256.0f - fraction) * channel->delay[newer] +
            channel->delay[older] * fraction) / 256.0f;
}

static uint32_t advance_phase(uint32_t phase, float rate)
{
    int32_t next = (int32_t)((float)(int32_t)phase + rate * 256.0f);

    if (next < 0) {
        next += (int32_t)GP100_PITCH_PHASE_LIMIT;
    } else if (next > (int32_t)GP100_PITCH_PHASE_LIMIT) {
        next -= (int32_t)GP100_PITCH_PHASE_LIMIT;
    }
    return (uint32_t)next;
}

static float process_shifted(gp100_detune_t *detune,
                             gp100_detune_channel_t *channel,
                             float input,
                             float rate)
{
    float sum = 0.0f;
    float filter_input;
    float output;
    size_t head;

    channel->delay[channel->write_index] = input;
    channel->write_index = (channel->write_index + 1u) %
                           GP100_DETUNE_DELAY_SAMPLES;
    for (head = 0u; head < 2u; ++head) {
        channel->phase[head] = advance_phase(channel->phase[head], rate);
        sum += delayed_sample(channel, channel->phase[head]) *
               gp100_pitch_window_value(detune->window, channel->phase[head]);
    }
    filter_input = sum * 1.5f * 3.0517578e-05f;
    output = filter_input * 0x1.11cf44p-1f +
             channel->previous_filter_input * 0x1.11cf44p-1f -
             channel->previous_filter_output * 0x1.1cf444p-4f;
    channel->previous_filter_input = filter_input;
    channel->previous_filter_output = output;
    return output;
}

static float move_toward(float current, float target)
{
    if (current < target) {
        current += 0.005f;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= 0.005f;
        return current < target ? target : current;
    }
    return current;
}

bool gp100_detune_reset(gp100_detune_t *detune,
                             int8_t cents,
                             uint8_t wet,
                             uint8_t dry)
{
    if (detune == NULL || !parameters_valid(cents, wet, dry)) {
        return false;
    }
    memset(detune, 0, sizeof(*detune));
    detune->cents = cents;
    detune->target_wet = (float)wet * 0.01f;
    detune->current_wet = detune->target_wet;
    detune->target_dry = (float)dry * 0.01f;
    detune->current_dry = detune->target_dry;
    detune->channel[0].phase[1] = GP100_PITCH_PHASE_LIMIT / 2u;
    detune->channel[1].phase[1] = GP100_PITCH_PHASE_LIMIT / 2u;
    gp100_pitch_build_window(detune->window);
    detune->configured = true;
    return true;
}

bool gp100_detune_process(gp100_detune_t *detune,
                               int8_t cents,
                               uint8_t wet,
                               uint8_t dry,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled)
{
    float rate;
    size_t frame;

    if (detune == NULL || !detune->configured ||
        !parameters_valid(cents, wet, dry) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    detune->cents = cents;
    detune->target_wet = (float)wet * 0.01f;
    detune->target_dry = (float)dry * 0.01f;
    detune->current_wet = move_toward(detune->current_wet,
                                      detune->target_wet);
    detune->current_dry = move_toward(detune->current_dry,
                                      detune->target_dry);
    rate = 1.0f - powf(2.0f, (float)cents / 1200.0f);

    for (frame = 0u; frame < frame_count; ++frame) {
        float input_left;
        float input_right;
        float shifted_left;
        float shifted_right;

        if (!enabled) {
            continue;
        }
        input_left = interleaved_stereo[frame * 2u];
        input_right = interleaved_stereo[frame * 2u + 1u];
        shifted_left = process_shifted(
            detune, &detune->channel[0], input_left, rate);
        shifted_right = process_shifted(
            detune, &detune->channel[1], input_right, rate);
        interleaved_stereo[frame * 2u] =
            shifted_left * detune->current_wet * 0.7f +
            input_left * detune->current_dry;
        interleaved_stereo[frame * 2u + 1u] =
            shifted_right * detune->current_wet * 0.7f +
            input_left * detune->current_dry;
    }
    return true;
}
