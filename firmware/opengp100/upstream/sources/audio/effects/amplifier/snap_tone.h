#ifndef GP100_SNAP_TONE_H
#define GP100_SNAP_TONE_H

#include "audio/dsp/audio_biquad.h"
#include "audio/dsp/partitioned_convolution.h"
#include "protocol/midi/sysex/vendor_sysex_user_amp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SNAP_TONE_PRE_TAPS 128u
#define GP100_SNAP_TONE_POST_TAPS 1024u
#define GP100_SNAP_TONE_PRE_PARTITIONS \
    (GP100_SNAP_TONE_PRE_TAPS / \
     GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)
#define GP100_SNAP_TONE_POST_PARTITIONS \
    (GP100_SNAP_TONE_POST_TAPS / \
     GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)
#define GP100_SNAP_TONE_EXPAND_FIRST_ORDER 7u
#define GP100_SNAP_TONE_EXPAND_SECOND_ORDER 5u
#define GP100_SNAP_TONE_COLLAPSE_FIRST_ORDER 4u
#define GP100_SNAP_TONE_COLLAPSE_SECOND_ORDER 6u
#define GP100_SNAP_TONE_EXPAND_ORDER \
    (GP100_SNAP_TONE_EXPAND_FIRST_ORDER + \
     GP100_SNAP_TONE_EXPAND_SECOND_ORDER)
#define GP100_SNAP_TONE_COLLAPSE_ORDER \
    (GP100_SNAP_TONE_COLLAPSE_FIRST_ORDER + \
     GP100_SNAP_TONE_COLLAPSE_SECOND_ORDER)

typedef struct gp100_snap_tone_targets {
    float input_gain;
    float output_gain;
    float tone_db[3];
} gp100_snap_tone_targets_t;

typedef struct gp100_snap_tone {
    gp100_user_amp_model_layout_t model;
    float pre_impulse[GP100_SNAP_TONE_PRE_TAPS];
    float post_impulse[GP100_SNAP_TONE_POST_TAPS];
    gp100_partitioned_convolution_t pre_convolution;
    gp100_partitioned_convolution_t post_convolution;
    gp100_complex_t pre_impulse_spectra[
        GP100_SNAP_TONE_PRE_PARTITIONS *
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t pre_input_spectra[
        GP100_SNAP_TONE_PRE_PARTITIONS *
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t post_impulse_spectra[
        GP100_SNAP_TONE_POST_PARTITIONS *
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    gp100_complex_t post_input_spectra[
        GP100_SNAP_TONE_POST_PARTITIONS *
        GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES];
    float expand_state[GP100_SNAP_TONE_EXPAND_ORDER];
    float collapse_state[GP100_SNAP_TONE_COLLAPSE_ORDER];
    float collapse_delay[2];
    double input_biquad_state[2];
    double output_biquad_state[2];
    gp100_biquad_coefficients_t tone_coefficients[3];
    float tone_state[3][2];
    float current_tone_db[3];
    float current_input_gain;
    float current_output_gain;
    float current_wet;
    bool loaded;
    bool configured;
    bool wrapper_configured;
} gp100_snap_tone_t;

bool gp100_snap_tone_load(
    gp100_snap_tone_t *state,
    const uint8_t *blob,
    size_t blob_size);

/* Keep the amplifier wrapper active with a silent model path. */
bool gp100_snap_tone_reset_empty(gp100_snap_tone_t *state);

bool gp100_snap_tone_control_targets(
    const float parameters[5],
    gp100_snap_tone_targets_t *targets);

bool gp100_snap_tone_tone_coefficients(
    const float tone_db[3],
    gp100_biquad_coefficients_t coefficients[3]);

/* Recovered all-pass lattice interpolation/decimation primitives. The factory
 * uses orders 7 then 5 before the waveshaper and 4 then 6 after it. */
bool gp100_snap_tone_lattice_expand(
    const float *input,
    size_t sample_count,
    const float *coefficients,
    float *state,
    size_t order,
    float *output_pairs);

bool gp100_snap_tone_lattice_collapse(
    const float *input_pairs,
    size_t sample_count,
    const float *coefficients,
    float *state,
    size_t order,
    float *second_branch_delay,
    float *output);

/* Readable model core including both recovered 4x lattice transforms and
 * 64-sample partitioned convolvers. sample_count must be a multiple of 64. */
bool gp100_snap_tone_process_mono(
    gp100_snap_tone_t *state,
    float *samples,
    size_t sample_count);

bool gp100_snap_tone_process_stereo(
    gp100_snap_tone_t *state,
    const float parameters[5],
    bool enabled,
    float *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
