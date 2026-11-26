#ifndef GP100_PARTITIONED_CONVOLUTION_H
#define GP100_PARTITIONED_CONVOLUTION_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES 64u
#define GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES 128u

typedef struct gp100_complex {
    float real;
    float imaginary;
} gp100_complex_t;

typedef struct gp100_partitioned_convolution {
    gp100_complex_t *impulse_spectra;
    gp100_complex_t *input_spectra;
    size_t partition_count;
    size_t spectrum_position;
    float overlap[GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES];
    gp100_complex_t
        fft_work[GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t
        fft_accumulator[GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t
        twiddle[GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES / 2u];
    bool configured;
} gp100_partitioned_convolution_t;

/*
 * Initializes a 64-sample uniform partitioned convolver. tap_count must be a
 * nonzero multiple of 64. Each caller-owned spectrum array needs
 * (tap_count / 64) * 128 complex elements and must remain alive until the
 * convolver is no longer used.
 */
bool gp100_partitioned_convolution_initialize(
    gp100_partitioned_convolution_t *state,
    const float *impulse,
    size_t tap_count,
    gp100_complex_t *impulse_spectra,
    size_t impulse_spectra_capacity,
    gp100_complex_t *input_spectra,
    size_t input_spectra_capacity);

/* Processes samples in place. sample_count must be a multiple of 64. */
bool gp100_partitioned_convolution_process(
    gp100_partitioned_convolution_t *state,
    float *samples,
    size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
