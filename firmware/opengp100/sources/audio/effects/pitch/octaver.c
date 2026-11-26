#include "octaver.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define OCTAVER_PHASE_LIMIT UINT32_C(0x100000)

static void build_window(gp100_octaver_t *octaver)
{
    size_t index;

    for (index = 0u; index < 256u; ++index) {
        long value = lroundf(sinf(3.1415927f * (float)index / 255.5f) *
                             32768.0f);

        if (value > 32767L) {
            value = 32767L;
        }
        octaver->window[index] = (uint16_t)value;
    }
    octaver->window[256] = 0u;
    octaver->window[257] = 0u;
}

static float window_value(const gp100_octaver_t *octaver,
                          uint32_t phase)
{
    size_t index = phase >> 12;
    uint32_t fraction = (phase >> 5) & 0x7fu;
    uint32_t left = ((128u - fraction) * octaver->window[index]) >> 7;
    uint32_t right = (fraction * octaver->window[index + 1u]) >> 7;

    return (float)(left + right);
}

static float delayed_sample(const gp100_octaver_t *octaver,
                            uint32_t phase)
{
    size_t delay = phase >> 8;
    size_t newer = octaver->write_index >= delay
        ? octaver->write_index - delay
        : octaver->write_index + GP100_OCTAVER_DELAY_SAMPLES - delay;
    size_t older = newer == 0u
        ? GP100_OCTAVER_DELAY_SAMPLES - 1u : newer - 1u;
    float fraction = (float)(phase & 0xffu);

    return ((256.0f - fraction) * octaver->delay[newer] +
            octaver->delay[older] * fraction) / 256.0f;
}

static uint32_t advance_phase(uint32_t phase, int32_t increment)
{
    int32_t next = (int32_t)phase + increment;

    if (next < 0) {
        next += (int32_t)OCTAVER_PHASE_LIMIT;
    } else if (next > (int32_t)OCTAVER_PHASE_LIMIT) {
        next -= (int32_t)OCTAVER_PHASE_LIMIT;
    }
    return (uint32_t)next;
}

static bool parameters_valid(uint8_t low_octave,
                             uint8_t high_octave,
                             uint8_t dry)
{
    return low_octave <= 99u && high_octave <= 99u && dry <= 99u;
}

static void set_parameters(gp100_octaver_t *octaver,
                           uint8_t low_octave,
                           uint8_t high_octave,
                           uint8_t dry)
{
    octaver->low_mix = (float)low_octave * 0.01f;
    octaver->high_mix = (float)high_octave * 0.01f;
    octaver->dry_mix = (float)dry * 0.01f;
}

bool gp100_octaver_reset(gp100_octaver_t *octaver,
                              uint8_t low_octave,
                              uint8_t high_octave,
                              uint8_t dry)
{
    if (octaver == NULL || !parameters_valid(low_octave, high_octave, dry)) {
        return false;
    }
    memset(octaver, 0, sizeof(*octaver));
    set_parameters(octaver, low_octave, high_octave, dry);
    octaver->low_phase[1] = UINT32_C(0x55555);
    octaver->low_phase[2] = UINT32_C(0xaaaaa);
    octaver->high_phase[1] = UINT32_C(0x80000);
    build_window(octaver);
    octaver->configured = true;
    return true;
}

bool gp100_octaver_process(gp100_octaver_t *octaver,
                                uint8_t low_octave,
                                uint8_t high_octave,
                                uint8_t dry,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled)
{
    size_t frame;

    if (octaver == NULL || !octaver->configured ||
        !parameters_valid(low_octave, high_octave, dry) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_parameters(octaver, low_octave, high_octave, dry);

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float low_sum = 0.0f;
        float high_sum = 0.0f;
        float high_input;
        float high_filtered;
        float output;
        size_t voice;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        octaver->delay[octaver->write_index] = input;
        octaver->write_index =
            (octaver->write_index + 1u) % GP100_OCTAVER_DELAY_SAMPLES;

        for (voice = 0u; voice < 3u; ++voice) {
            octaver->low_phase[voice] = advance_phase(
                octaver->low_phase[voice], 128);
            low_sum += delayed_sample(octaver, octaver->low_phase[voice]) *
                       window_value(octaver, octaver->low_phase[voice]);
        }
        for (voice = 0u; voice < 2u; ++voice) {
            octaver->high_phase[voice] = advance_phase(
                octaver->high_phase[voice], -256);
            high_sum += delayed_sample(octaver, octaver->high_phase[voice]) *
                        window_value(octaver, octaver->high_phase[voice]);
        }

        high_input = high_sum * 1.5f * 0.000030517578f;
        high_filtered = high_input * 0x1.11cf44p-1f +
                        octaver->previous_high_input * 0x1.11cf44p-1f -
                        octaver->previous_high_output * 0x1.1cf444p-4f;
        output = low_sum * 0.000030517578f * octaver->low_mix +
                 high_filtered * octaver->high_mix + input * octaver->dry_mix;
        octaver->previous_high_input = high_input;
        octaver->previous_high_output = high_filtered;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
