#include "pitch_bender.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define PITCH_BENDER_PHASE_LIMIT UINT32_C(0x100000)

/* Exact factory phase increments for semitones -24 through +24. */
static const float pitch_phase_rates[49] = {
    0x1.8p-1f, 0x1.786382p-1f, 0x1.705326p-1f, 0x1.67c80ep-1f,
    0x1.5ebae6p-1f, 0x1.5523f6p-1f, 0x1.4afb0cp-1f, 0x1.403778p-1f,
    0x1.34d00ap-1f, 0x1.28bbp-1f, 0x1.1bee0ep-1f, 0x1.0e5e4p-1f,
    0x1p-1f, 0x1.e18e0ap-2f, 0x1.c14ca2p-2f, 0x1.9f203cp-2f,
    0x1.7aeb9cp-2f, 0x1.548fdap-2f, 0x1.2bec32p-2f, 0x1.00ddep-2f,
    0x1.a6804cp-3f, 0x1.45d80ep-3f, 0x1.bee0e6p-4f, 0x1.cbc7eap-5f,
    0.0f, -0x1.e71f28p-5f, -0x1.f59ab6p-4f, -0x1.837f04p-3f,
    -0x1.0a28bcp-2f, -0x1.56e046p-2f, -0x1.a8279cp-2f, -0x1.fe4438p-2f,
    -0x1.2cbfd6p-1f, -0x1.5d13f2p-1f, -0x1.9047cp-1f, -0x1.c686fcp-1f,
    -0x1p+0f, -0x1.1e71f2p+0f, -0x1.3eb358p+0f, -0x1.60dfcp+0f,
    -0x1.85146p+0f, -0x1.ab702p+0f, -0x1.d413ccp+0f, -0x1.ff221cp+0f,
    -0x1.165feap+1f, -0x1.2e89fap+1f, -0x1.4823ep+1f, -0x1.63437ep+1f,
    -0x1.8p+1f
};

static void build_window(gp100_pitch_bender_t *bender)
{
    size_t index;

    for (index = 0u; index < 256u; ++index) {
        long value = lroundf(sinf(3.1415927f * (float)index / 255.5f) *
                             32768.0f);

        if (value > 32767L) {
            value = 32767L;
        }
        bender->window[index] = (uint16_t)value;
    }
    bender->window[256] = 0u;
    bender->window[257] = 0u;
}

static float window_value(const gp100_pitch_bender_t *bender,
                          uint32_t phase)
{
    size_t index = phase >> 12;
    uint32_t fraction = (phase >> 5) & 0x7fu;

    return (float)((((128u - fraction) * bender->window[index]) >> 7) +
                   ((fraction * bender->window[index + 1u]) >> 7));
}

static float delayed_sample(const gp100_pitch_bender_t *bender,
                            uint32_t phase)
{
    size_t delay = phase >> 8;
    size_t newer = bender->write_index >= delay
        ? bender->write_index - delay
        : bender->write_index + GP100_PITCH_BENDER_DELAY_SAMPLES - delay;
    size_t older = newer == 0u
        ? GP100_PITCH_BENDER_DELAY_SAMPLES - 1u : newer - 1u;
    float fraction = (float)(phase & 0xffu);

    return ((256.0f - fraction) * bender->delay[newer] +
            bender->delay[older] * fraction) / 256.0f;
}

static uint32_t advance_phase(uint32_t phase, float rate)
{
    int32_t next = (int32_t)((float)(int32_t)phase + rate * 256.0f);

    if (next < 0) {
        next += (int32_t)PITCH_BENDER_PHASE_LIMIT;
    } else if (next > (int32_t)PITCH_BENDER_PHASE_LIMIT) {
        next -= (int32_t)PITCH_BENDER_PHASE_LIMIT;
    }
    return (uint32_t)next;
}

static float process_shifted(gp100_pitch_bender_t *bender,
                             float input,
                             float rate)
{
    float sum = 0.0f;
    float filter_input;
    float output;
    size_t index;

    bender->delay[bender->write_index] = input;
    bender->write_index = (bender->write_index + 1u) %
                          GP100_PITCH_BENDER_DELAY_SAMPLES;
    for (index = 0u; index < 2u; ++index) {
        bender->phase[index] = advance_phase(bender->phase[index], rate);
        sum += delayed_sample(bender, bender->phase[index]) *
               window_value(bender, bender->phase[index]);
    }
    filter_input = sum * 1.5f / 32768.0f;
    output = filter_input * 0x1.11cf44p-1f +
             bender->previous_filter_input * 0x1.11cf44p-1f -
             bender->previous_filter_output * 0x1.1cf444p-4f;
    bender->previous_filter_input = filter_input;
    bender->previous_filter_output = output;
    return output;
}

static bool parameters_valid(int8_t high_pitch,
                             int8_t low_pitch,
                             uint8_t wet,
                             uint8_t dry,
                             uint8_t range)
{
    return high_pitch >= 0 && high_pitch <= 12 &&
           low_pitch >= -12 && low_pitch <= 0 && wet <= 99u &&
           dry <= 99u && range <= 99u;
}

static void set_parameters(gp100_pitch_bender_t *bender,
                           int8_t high_pitch,
                           int8_t low_pitch,
                           uint8_t wet,
                           uint8_t dry,
                           uint8_t range)
{
    bender->high_pitch = high_pitch;
    bender->low_pitch = low_pitch;
    bender->wet_mix = (float)wet * 0.01f;
    bender->dry_mix = (float)dry * 0.01f;
    bender->range_mix = (float)range * 0.01f;
}

bool gp100_pitch_bender_reset(gp100_pitch_bender_t *bender,
                                   int8_t high_pitch,
                                   int8_t low_pitch,
                                   uint8_t wet,
                                   uint8_t dry,
                                   uint8_t range)
{
    if (bender == NULL || !parameters_valid(
            high_pitch, low_pitch, wet, dry, range)) {
        return false;
    }
    memset(bender, 0, sizeof(*bender));
    set_parameters(bender, high_pitch, low_pitch, wet, dry, range);
    bender->phase[1] = PITCH_BENDER_PHASE_LIMIT / 2u;
    build_window(bender);
    bender->configured = true;
    return true;
}

bool gp100_pitch_bender_process(gp100_pitch_bender_t *bender,
                                     int8_t high_pitch,
                                     int8_t low_pitch,
                                     uint8_t wet,
                                     uint8_t dry,
                                     uint8_t range,
                                     float *interleaved_stereo,
                                     size_t frame_count,
                                     bool enabled)
{
    float low_rate;
    float high_rate;
    float rate;
    size_t frame;

    if (bender == NULL || !bender->configured || !parameters_valid(
            high_pitch, low_pitch, wet, dry, range) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_parameters(bender, high_pitch, low_pitch, wet, dry, range);
    low_rate = pitch_phase_rates[(int)low_pitch + 24];
    high_rate = pitch_phase_rates[(int)high_pitch + 24];
    rate = low_rate + bender->range_mix * (high_rate - low_rate);
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float shifted;
        float output;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        shifted = process_shifted(bender, input, rate);
        output = shifted * bender->wet_mix * 0.7f +
                 input * bender->dry_mix;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
