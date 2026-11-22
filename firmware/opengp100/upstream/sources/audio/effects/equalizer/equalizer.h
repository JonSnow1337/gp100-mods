#ifndef GP100_EQUALIZER_H
#define GP100_EQUALIZER_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EQUALIZER_BAND_COUNT 5u
#define GP100_EQUALIZER_SAMPLE_RATE 44100.0f

typedef enum gp100_equalizer_type {
    GP100_EQUALIZER_1 = 0,
    GP100_EQUALIZER_2 = 1,
    GP100_EQUALIZER_MESS = 2
} gp100_equalizer_type_t;

typedef struct gp100_equalizer_band {
    gp100_biquad_coefficients_t coefficients;
    gp100_biquad_channel_state_t state;
    float parameter;
    float frequency_hz;
    float gain_db;
    float two_q;
} gp100_equalizer_band_t;

typedef struct gp100_equalizer {
    gp100_equalizer_type_t type;
    gp100_equalizer_band_t bands[GP100_EQUALIZER_BAND_COUNT];
    float level;
    bool configured;
} gp100_equalizer_t;

/* EQ 1/EQ 2 use five gains plus level; Mess EQ uses five gains. */
bool gp100_equalizer_reset(
    gp100_equalizer_t *equalizer,
    gp100_equalizer_type_t type,
    const float *parameters);

/*
 * Processes the left channel through five peaking filters and copies the
 * result to both channels, matching the factory modules' dual-mono output.
 */
bool gp100_equalizer_process(
    gp100_equalizer_t *equalizer,
    const float *parameters,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
