#include "pitch_shifter.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define PITCH_PHASE_LIMIT UINT32_C(0x100000)

/* Exact phase increments for semitones -24 through +24. */
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

static void build_window(gp100_pitch_shifter_t *shifter)
{
    size_t index;

    for (index = 0u; index < 256u; ++index) {
        long value = lroundf(sinf(3.1415927f * (float)index / 255.5f) *
                             32768.0f);

        if (value > 32767L) {
            value = 32767L;
        }
        shifter->window[index] = (uint16_t)value;
    }
    shifter->window[256] = 0u;
    shifter->window[257] = 0u;
}

static float window_value(const gp100_pitch_shifter_t *shifter,
                          uint32_t phase)
{
    size_t index = phase >> 12;
    uint32_t fraction = (phase >> 5) & 0x7fu;

    return (float)((((128u - fraction) * shifter->window[index]) >> 7) +
                   ((fraction * shifter->window[index + 1u]) >> 7));
}

static float delayed_sample(const gp100_pitch_voice_t *voice,
                            uint32_t phase)
{
    size_t delay = phase >> 8;
    size_t newer = voice->write_index >= delay
        ? voice->write_index - delay
        : voice->write_index + GP100_PITCH_DELAY_SAMPLES - delay;
    size_t older = newer == 0u
        ? GP100_PITCH_DELAY_SAMPLES - 1u : newer - 1u;
    float fraction = (float)(phase & 0xffu);

    return ((256.0f - fraction) * voice->delay[newer] +
            voice->delay[older] * fraction) / 256.0f;
}

static uint32_t advance_phase(uint32_t phase, float rate)
{
    int32_t next = (int32_t)((float)(int32_t)phase + rate * 256.0f);

    if (next < 0) {
        next += (int32_t)PITCH_PHASE_LIMIT;
    } else if (next > (int32_t)PITCH_PHASE_LIMIT) {
        next -= (int32_t)PITCH_PHASE_LIMIT;
    }
    return (uint32_t)next;
}

static float process_pitch(gp100_pitch_shifter_t *shifter,
                           gp100_pitch_voice_t *voice,
                           float input,
                           int8_t semitones)
{
    float rate = pitch_phase_rates[(int)semitones + 24];
    float sum = 0.0f;
    float filter_input;
    float output;
    size_t index;

    voice->delay[voice->write_index] = input;
    voice->write_index =
        (voice->write_index + 1u) % GP100_PITCH_DELAY_SAMPLES;
    for (index = 0u; index < 2u; ++index) {
        voice->phase[index] = advance_phase(voice->phase[index], rate);
        sum += delayed_sample(voice, voice->phase[index]) *
               window_value(shifter, voice->phase[index]);
    }
    filter_input = sum * 1.5f / 32768.0f;
    output = filter_input * 0x1.11cf44p-1f +
             voice->previous_filter_input * 0x1.11cf44p-1f -
             voice->previous_filter_output * 0x1.1cf444p-4f;
    voice->previous_filter_input = filter_input;
    voice->previous_filter_output = output;
    return output;
}

static bool parameters_valid(int8_t high_pitch,
                             int8_t low_pitch,
                             uint8_t dry,
                             uint8_t high_level,
                             uint8_t low_level)
{
    return high_pitch >= 0 && high_pitch <= 24 &&
           low_pitch >= -24 && low_pitch <= 0 && dry <= 99u &&
           high_level <= 99u && low_level <= 99u;
}

static void set_parameters(gp100_pitch_shifter_t *shifter,
                           int8_t high_pitch,
                           int8_t low_pitch,
                           uint8_t dry,
                           uint8_t high_level,
                           uint8_t low_level)
{
    shifter->high_pitch = high_pitch;
    shifter->low_pitch = low_pitch;
    shifter->dry_mix = (float)dry * 0.01f;
    shifter->high_mix = (float)high_level * 0.01f;
    shifter->low_mix = (float)low_level * 0.01f;
}

bool gp100_pitch_shifter_reset(gp100_pitch_shifter_t *shifter,
                                    int8_t high_pitch,
                                    int8_t low_pitch,
                                    uint8_t dry,
                                    uint8_t high_level,
                                    uint8_t low_level)
{
    if (shifter == NULL || !parameters_valid(
            high_pitch, low_pitch, dry, high_level, low_level)) {
        return false;
    }
    memset(shifter, 0, sizeof(*shifter));
    set_parameters(shifter, high_pitch, low_pitch, dry, high_level, low_level);
    shifter->high.phase[1] = PITCH_PHASE_LIMIT / 2u;
    shifter->low.phase[1] = PITCH_PHASE_LIMIT / 2u;
    build_window(shifter);
    shifter->configured = true;
    return true;
}

bool gp100_pitch_shifter_process(gp100_pitch_shifter_t *shifter,
                                      int8_t high_pitch,
                                      int8_t low_pitch,
                                      uint8_t dry,
                                      uint8_t high_level,
                                      uint8_t low_level,
                                      float *interleaved_stereo,
                                      size_t frame_count,
                                      bool enabled)
{
    size_t frame;

    if (shifter == NULL || !shifter->configured || !parameters_valid(
            high_pitch, low_pitch, dry, high_level, low_level) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_parameters(shifter, high_pitch, low_pitch, dry, high_level, low_level);
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float high;
        float low;
        float output;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        high = process_pitch(shifter, &shifter->high, input, high_pitch);
        low = process_pitch(shifter, &shifter->low, input, low_pitch);
        output = high * shifter->high_mix * 0.7f +
                 low * shifter->low_mix * 0.7f + input * shifter->dry_mix;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
