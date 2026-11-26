#include "audio_pcm.h"

#include <math.h>

#define PCM24_SCALE 8388608.0f

bool gp100_audio_pcm24_to_float(const int32_t *input,
                                     float *output,
                                     size_t sample_count)
{
    size_t sample;

    if ((sample_count != 0u && (input == NULL || output == NULL))) {
        return false;
    }
    for (sample = 0u; sample < sample_count; ++sample) {
        int32_t value = input[sample];
        if (value < GP100_PCM24_MIN) {
            value = GP100_PCM24_MIN;
        } else if (value > GP100_PCM24_MAX) {
            value = GP100_PCM24_MAX;
        }
        output[sample] = (float)value / PCM24_SCALE;
    }
    return true;
}

bool gp100_audio_float_to_pcm24(const float *input,
                                     int32_t *output,
                                     size_t sample_count,
                                     size_t *clipped_samples)
{
    size_t clipped = 0u;
    size_t sample;

    if ((sample_count != 0u && (input == NULL || output == NULL))) {
        return false;
    }
    for (sample = 0u; sample < sample_count; ++sample) {
        float value = input[sample];
        if (!isfinite(value)) {
            output[sample] = 0;
            ++clipped;
        } else if (value <= -1.0f) {
            output[sample] = GP100_PCM24_MIN;
            if (value < -1.0f) {
                ++clipped;
            }
        } else if (value >= 1.0f) {
            output[sample] = GP100_PCM24_MAX;
            ++clipped;
        } else {
            output[sample] = (int32_t)(value * PCM24_SCALE);
        }
    }
    if (clipped_samples != NULL) {
        *clipped_samples = clipped;
    }
    return true;
}
