#ifndef GP100_PITCH_WINDOW_INTERNAL_H
#define GP100_PITCH_WINDOW_INTERNAL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_PITCH_PHASE_LIMIT UINT32_C(0x100000)

/* The two pitch-shift packages share this quantized half-sine table. */
static inline void gp100_pitch_build_window(uint16_t window[258])
{
    size_t index;

    for (index = 0u; index < 256u; ++index) {
        long value = lroundf(sinf(3.1415927f * (float)index / 255.5f) *
                             32768.0f);

        window[index] = (uint16_t)(value > 32767L ? 32767L : value);
    }
    window[256] = 0u;
    window[257] = 0u;
}

static inline float gp100_pitch_window_value(const uint16_t window[258],
                                                 uint32_t phase)
{
    const size_t index = phase >> 12u;
    const uint32_t fraction = (phase >> 5u) & 0x7fu;

    return (float)((((128u - fraction) * window[index]) >> 7u) +
                   ((fraction * window[index + 1u]) >> 7u));
}

#endif
