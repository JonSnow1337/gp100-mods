#ifndef GP100_DECIMATION_INTERNAL_H
#define GP100_DECIMATION_INTERNAL_H

#include <stddef.h>
#include <string.h>

/* The 40-tap reconstruction filter shared by the factory waveshapers.
 * Supply four oversampled values from oldest to newest. */
static inline float gp100_decimate_four(float history[40],
                                             const float samples[4])
{
    static const float coefficients[40] = {
        -0.0011f, -0.0007f, 0.0001f, 0.0015f, 0.0032f,
        0.0048f, 0.0053f, 0.0034f, -0.0016f, -0.0092f,
        -0.0175f, -0.0233f, -0.0227f, -0.0123f, 0.0097f,
        0.0421f, 0.0807f, 0.1193f, 0.1504f, 0.1678f,
        0.1678f, 0.1504f, 0.1193f, 0.0807f, 0.0421f,
        0.0097f, -0.0123f, -0.0227f, -0.0233f, -0.0175f,
        -0.0092f, -0.0016f, 0.0034f, 0.0053f, 0.0048f,
        0.0032f, 0.0015f, 0.0001f, -0.0007f, -0.0011f
    };
    float output = 0.0f;
    size_t index;

    memmove(&history[4], history, 36u * sizeof(float));
    for (index = 0u; index < 4u; ++index) {
        history[index] = samples[3u - index];
    }
    for (index = 40u; index > 0u; --index) {
        output += history[index - 1u] * coefficients[index - 1u];
    }
    return output;
}

#endif
