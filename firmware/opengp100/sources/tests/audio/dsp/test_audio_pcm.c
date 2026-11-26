#include "audio/dsp/audio_pcm.h"

#include <math.h>
#include <stdio.h>

static int test_unpack(void)
{
    const int32_t input[] = {
        GP100_PCM24_MIN, -4194304, 0, 4194304,
        GP100_PCM24_MAX, INT32_MIN, INT32_MAX
    };
    float output[sizeof(input) / sizeof(input[0])];

    if (!gp100_audio_pcm24_to_float(
            input, output, sizeof(input) / sizeof(input[0])) ||
        output[0] != -1.0f || output[1] != -0.5f || output[2] != 0.0f ||
        output[3] != 0.5f || !(output[4] < 1.0f) ||
        output[5] != -1.0f || !(output[6] < 1.0f) ||
        gp100_audio_pcm24_to_float(NULL, output, 1u)) {
        return 1;
    }
    return 0;
}

static int test_pack(void)
{
    const float input[] = {
        -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, NAN
    };
    int32_t output[sizeof(input) / sizeof(input[0])];
    size_t clipped = 0u;

    if (!gp100_audio_float_to_pcm24(
            input, output, sizeof(input) / sizeof(input[0]), &clipped) ||
        output[0] != GP100_PCM24_MIN ||
        output[1] != GP100_PCM24_MIN || output[2] != -4194304 ||
        output[3] != 0 || output[4] != 4194304 ||
        output[5] != GP100_PCM24_MAX ||
        output[6] != GP100_PCM24_MAX || output[7] != 0 ||
        clipped != 4u ||
        gp100_audio_float_to_pcm24(input, NULL, 1u, &clipped)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_unpack();

    if (result != 0) {
        fprintf(stderr, "PCM24 unpack test failed: %d\n", result);
        return result;
    }
    result = test_pack();
    if (result != 0) {
        fprintf(stderr, "PCM24 pack test failed: %d\n", result);
        return 10 + result;
    }
    return 0;
}
