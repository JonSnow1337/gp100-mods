#include "partitioned_convolution.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define GP100_TWO_PI 6.2831853071795864769f

static void fft_transform(
    const gp100_partitioned_convolution_t *state,
    gp100_complex_t values[GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES],
    bool inverse)
{
    const size_t count = GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES;
    size_t index;
    size_t reversed = 0u;
    size_t length;

    for (index = 1u; index < count; ++index) {
        size_t bit = count >> 1u;

        while ((reversed & bit) != 0u) {
            reversed ^= bit;
            bit >>= 1u;
        }
        reversed ^= bit;
        if (index < reversed) {
            gp100_complex_t temporary = values[index];

            values[index] = values[reversed];
            values[reversed] = temporary;
        }
    }

    for (length = 2u; length <= count; length <<= 1u) {
        size_t half = length >> 1u;
        size_t twiddle_step = count / length;
        size_t offset;

        /* Butterflies at the same offset share a twiddle across the stage. */
        for (offset = 0u; offset < half; ++offset) {
            const gp100_complex_t twiddle =
                state->twiddle[offset * twiddle_step];
            const float twiddle_real = twiddle.real;
            const float twiddle_imaginary = inverse
                ? -twiddle.imaginary : twiddle.imaginary;

            for (index = offset; index < count; index += length) {
                const gp100_complex_t even = values[index];
                const gp100_complex_t odd = values[index + half];
                gp100_complex_t rotated;

                rotated.real = odd.real * twiddle_real -
                    odd.imaginary * twiddle_imaginary;
                rotated.imaginary = odd.real * twiddle_imaginary +
                    odd.imaginary * twiddle_real;
                values[index].real = even.real + rotated.real;
                values[index].imaginary = even.imaginary + rotated.imaginary;
                values[index + half].real = even.real - rotated.real;
                values[index + half].imaginary =
                    even.imaginary - rotated.imaginary;
            }
        }
    }

    if (inverse) {
        const float scale = 1.0f / (float)count;

        for (index = 0u; index < count; ++index) {
            values[index].real *= scale;
            values[index].imaginary *= scale;
        }
    }
}

bool gp100_partitioned_convolution_initialize(
    gp100_partitioned_convolution_t *state,
    const float *impulse,
    size_t tap_count,
    gp100_complex_t *impulse_spectra,
    size_t impulse_spectra_capacity,
    gp100_complex_t *input_spectra,
    size_t input_spectra_capacity)
{
    const size_t block = GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
    const size_t fft_count =
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES;
    const size_t real_bins = fft_count / 2u + 1u;
    size_t required_spectra;
    size_t partition_count;
    size_t index;
    size_t partition;

    if (state == NULL || impulse == NULL || impulse_spectra == NULL ||
        input_spectra == NULL || tap_count == 0u || tap_count % block != 0u) {
        return false;
    }
    partition_count = tap_count / block;
    if (partition_count > SIZE_MAX / fft_count) {
        return false;
    }
    required_spectra = partition_count * fft_count;
    if (impulse_spectra_capacity < required_spectra ||
        input_spectra_capacity < required_spectra) {
        return false;
    }
    for (index = 0u; index < tap_count; ++index) {
        if (!isfinite(impulse[index])) {
            return false;
        }
    }

    memset(state, 0, sizeof(*state));
    state->impulse_spectra = impulse_spectra;
    state->input_spectra = input_spectra;
    state->partition_count = partition_count;
    memset(input_spectra, 0,
           required_spectra * sizeof(input_spectra[0]));
    for (index = 0u; index < fft_count / 2u; ++index) {
        float angle = -GP100_TWO_PI * (float)index / (float)fft_count;

        state->twiddle[index].real = cosf(angle);
        state->twiddle[index].imaginary = sinf(angle);
    }
    for (partition = 0u; partition < partition_count; ++partition) {
        gp100_complex_t *spectrum =
            &impulse_spectra[partition * real_bins];

        memset(state->fft_work, 0, sizeof(state->fft_work));
        for (index = 0u; index < block; ++index) {
            state->fft_work[index].real =
                impulse[partition * block + index];
        }
        fft_transform(state, state->fft_work, false);
        memcpy(spectrum, state->fft_work, real_bins * sizeof(*spectrum));
    }
    state->configured = true;
    return true;
}

bool gp100_partitioned_convolution_process(
    gp100_partitioned_convolution_t *state,
    float *samples,
    size_t sample_count)
{
    const size_t block = GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
    const size_t fft_count =
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES;
    const size_t real_bins = fft_count / 2u + 1u;
    size_t sample_offset;

    if (state == NULL || !state->configured ||
        sample_count % block != 0u ||
        (sample_count != 0u && samples == NULL)) {
        return false;
    }

    for (sample_offset = 0u; sample_offset < sample_count;
         sample_offset += block) {
        gp100_complex_t *input_spectrum =
            &state->input_spectra[state->spectrum_position * real_bins];
        size_t history_position = state->spectrum_position;
        size_t partition;
        size_t bin;

        memset(state->fft_work, 0, sizeof(state->fft_work));
        for (bin = 0u; bin < block; ++bin) {
            state->fft_work[bin].real = samples[sample_offset + bin];
        }
        fft_transform(state, state->fft_work, false);
        memcpy(input_spectrum, state->fft_work,
               real_bins * sizeof(*input_spectrum));
        memset(state->fft_accumulator, 0,
               real_bins * sizeof(state->fft_accumulator[0]));

        /* Real signals have conjugate-symmetric spectra. Accumulate only
         * DC through Nyquist and reflect the interior bins before the IFFT. */
        for (partition = 0u; partition < state->partition_count;
             ++partition) {
            const gp100_complex_t *impulse_spectrum =
                &state->impulse_spectra[partition * real_bins];
            const gp100_complex_t *history_spectrum =
                &state->input_spectra[history_position * real_bins];

            for (bin = 0u; bin < real_bins; ++bin) {
                state->fft_accumulator[bin].real +=
                    impulse_spectrum[bin].real * history_spectrum[bin].real -
                    impulse_spectrum[bin].imaginary *
                        history_spectrum[bin].imaginary;
                state->fft_accumulator[bin].imaginary +=
                    impulse_spectrum[bin].real *
                        history_spectrum[bin].imaginary +
                    impulse_spectrum[bin].imaginary *
                        history_spectrum[bin].real;
            }
            history_position = history_position == 0u
                ? state->partition_count - 1u : history_position - 1u;
        }
        state->fft_accumulator[0].imaginary = 0.0f;
        state->fft_accumulator[fft_count / 2u].imaginary = 0.0f;
        for (bin = 1u; bin < fft_count / 2u; ++bin) {
            state->fft_accumulator[fft_count - bin].real =
                state->fft_accumulator[bin].real;
            state->fft_accumulator[fft_count - bin].imaginary =
                -state->fft_accumulator[bin].imaginary;
        }
        fft_transform(state, state->fft_accumulator, true);
        for (bin = 0u; bin < block; ++bin) {
            samples[sample_offset + bin] =
                state->fft_accumulator[bin].real + state->overlap[bin];
            state->overlap[bin] =
                state->fft_accumulator[bin + block].real;
        }
        ++state->spectrum_position;
        if (state->spectrum_position == state->partition_count) {
            state->spectrum_position = 0u;
        }
    }
    return true;
}
