#ifndef GP100_GLOBAL_EQUALIZER_H
#define GP100_GLOBAL_EQUALIZER_H

#include "audio/dsp/audio_biquad.h"
#include "protocol/midi/sysex/vendor_sysex_global.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_GLOBAL_EQUALIZER_FILTER_COUNT 6u
#define GP100_GLOBAL_EQUALIZER_SAMPLE_RATE 44100.0f

/*
 * Readable form of the recovered factory Global EQ graph: low-cut, four
 * parametric bands, then high-cut and the output gain ramp. The shipped
 * generic processor leaves both of its additional guard bands disabled.
 */
typedef struct gp100_global_equalizer {
    gp100_stereo_biquad_t filters[
        GP100_GLOBAL_EQUALIZER_FILTER_COUNT];
    gp100_biquad_coefficients_t coefficients[
        GP100_GLOBAL_EQUALIZER_FILTER_COUNT];
    float parameters[GP100_SYSEX_GLOBAL_PARAMETER_COUNT];
    float current_output_gain;
    bool filter_active[GP100_GLOBAL_EQUALIZER_FILTER_COUNT];
    bool enabled;
    bool configured;
} gp100_global_equalizer_t;

void gp100_global_equalizer_reset(
    gp100_global_equalizer_t *equalizer);

/* Validate and transactionally publish one recovered settings snapshot. */
bool gp100_global_equalizer_configure(
    gp100_global_equalizer_t *equalizer,
    const gp100_sysex_global_state_t *state);

/* Process interleaved stereo in place using the last valid snapshot. */
bool gp100_global_equalizer_process(
    gp100_global_equalizer_t *equalizer,
    float *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
