#include "audio/dsp/partitioned_convolution.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_SAMPLE_COUNT 1536u
#define TEST_MAX_TAPS 1024u
#define TEST_MAX_PARTITIONS \
    (TEST_MAX_TAPS / GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)
#define TEST_SPECTRUM_COUNT \
    (TEST_MAX_PARTITIONS * GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES)

static void direct_convolution(const float *input,
                               size_t sample_count,
                               const float *impulse,
                               size_t tap_count,
                               float *output)
{
    size_t sample;

    for (sample = 0u; sample < sample_count; ++sample) {
        size_t tap_limit = sample + 1u < tap_count ? sample + 1u : tap_count;
        size_t tap;
        float sum = 0.0f;

        for (tap = 0u; tap < tap_limit; ++tap) {
            sum += input[sample - tap] * impulse[tap];
        }
        output[sample] = sum;
    }
}

static int run_case(size_t tap_count)
{
    static gp100_partitioned_convolution_t state;
    static gp100_complex_t impulse_spectra[TEST_SPECTRUM_COUNT];
    static gp100_complex_t input_spectra[TEST_SPECTRUM_COUNT];
    static float impulse[TEST_MAX_TAPS];
    static float input[TEST_SAMPLE_COUNT];
    static float actual[TEST_SAMPLE_COUNT];
    static float expected[TEST_SAMPLE_COUNT];
    size_t index;
    size_t offset;

    for (index = 0u; index < tap_count; ++index) {
        impulse[index] = 0.003f * cosf((float)index * 0.071f) +
            0.002f * sinf((float)index * 0.193f);
    }
    impulse[0] += 0.75f;
    for (index = 0u; index < TEST_SAMPLE_COUNT; ++index) {
        input[index] = 0.2f * sinf((float)index * 0.037f) +
            0.1f * cosf((float)index * 0.113f);
    }
    memcpy(actual, input, sizeof(actual));
    direct_convolution(input, TEST_SAMPLE_COUNT, impulse, tap_count,
                       expected);
    if (!gp100_partitioned_convolution_initialize(
            &state, impulse, tap_count, impulse_spectra,
            TEST_SPECTRUM_COUNT, input_spectra, TEST_SPECTRUM_COUNT)) {
        return 1;
    }

    /* Unequal call sizes exercise continuity across public API calls. */
    offset = 0u;
    while (offset < TEST_SAMPLE_COUNT) {
        size_t count = offset == 0u ? 128u : 64u;

        if (!gp100_partitioned_convolution_process(
                &state, &actual[offset], count)) {
            return 2;
        }
        offset += count;
    }
    for (index = 0u; index < TEST_SAMPLE_COUNT; ++index) {
        float tolerance = 2.0e-5f + fabsf(expected[index]) * 2.0e-5f;

        if (!isfinite(actual[index]) ||
            fabsf(actual[index] - expected[index]) > tolerance) {
            fprintf(stderr,
                    "%lu-tap convolution differs at %lu: %.9g != %.9g\n",
                    (unsigned long)tap_count, (unsigned long)index,
                    (double)actual[index], (double)expected[index]);
            return 3;
        }
    }
    return 0;
}

int main(void)
{
    static gp100_partitioned_convolution_t state;
    gp100_complex_t impulse_spectra[
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t input_spectra[
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    float impulse[GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES] = {0.0f};
    float samples[GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES] = {0.0f};
    int result;

    result = run_case(128u);
    if (result != 0) {
        return result;
    }
    result = run_case(1024u);
    if (result != 0) {
        return result + 3;
    }

    impulse[0] = 1.0f;
    samples[0] = 0.5f;
    samples[63] = -0.25f;
    if (!gp100_partitioned_convolution_initialize(
            &state, impulse, sizeof(impulse) / sizeof(impulse[0]),
            impulse_spectra,
            sizeof(impulse_spectra) / sizeof(impulse_spectra[0]),
            input_spectra,
            sizeof(input_spectra) / sizeof(input_spectra[0])) ||
        !gp100_partitioned_convolution_process(
            &state, samples, sizeof(samples) / sizeof(samples[0])) ||
        fabsf(samples[0] - 0.5f) > 1.0e-6f ||
        fabsf(samples[63] + 0.25f) > 1.0e-6f ||
        gp100_partitioned_convolution_process(&state, samples, 63u) ||
        gp100_partitioned_convolution_initialize(
            &state, impulse, 63u, impulse_spectra,
            sizeof(impulse_spectra) / sizeof(impulse_spectra[0]),
            input_spectra,
            sizeof(input_spectra) / sizeof(input_spectra[0])) ||
        gp100_partitioned_convolution_initialize(
            &state, impulse, 64u, impulse_spectra, 127u,
            input_spectra,
            sizeof(input_spectra) / sizeof(input_spectra[0]))) {
        fputs("partitioned-convolution identity/validation failed\n", stderr);
        return 7;
    }
    return 0;
}
