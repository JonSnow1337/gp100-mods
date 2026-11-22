#ifndef GP100_COMPRESSOR_H
#define GP100_COMPRESSOR_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_COMP_FILTER_COUNT 2u
#define GP100_COMP4_PREFILTER_HISTORY 3u

typedef struct gp100_comp4_prefilter_coefficients {
    float numerator[GP100_COMP4_PREFILTER_HISTORY + 1u];
    float denominator[GP100_COMP4_PREFILTER_HISTORY + 1u];
} gp100_comp4_prefilter_coefficients_t;

typedef struct gp100_comp4_prefilter_state {
    float input[GP100_COMP4_PREFILTER_HISTORY];
    float output[GP100_COMP4_PREFILTER_HISTORY];
} gp100_comp4_prefilter_state_t;

typedef struct gp100_comp {
    float sustain;
    float sustain_target;
    float output_level;
    float output_target;
    float gain_reduction_db;
    float attack_coefficient;
    float release_coefficient;
    gp100_biquad_coefficients_t
        filter_coefficients[GP100_COMP_FILTER_COUNT];
    gp100_biquad_channel_state_t
        filter_state[GP100_COMP_FILTER_COUNT];
    uint8_t sustain_parameter;
    uint8_t output_parameter;
    bool configured;
} gp100_comp_t;

typedef struct gp100_comp4 {
    float sustain;
    float sustain_target;
    float attack;
    float attack_target;
    float output_level;
    float output_target;
    float clip;
    float clip_target;
    float attack_time_seconds;
    float gain_reduction_db;
    float attack_coefficient;
    float release_coefficient;
    gp100_comp4_prefilter_coefficients_t prefilter_coefficients;
    gp100_comp4_prefilter_state_t prefilter_state;
    gp100_biquad_coefficients_t
        filter_coefficients[GP100_COMP_FILTER_COUNT];
    gp100_biquad_channel_state_t
        filter_state[GP100_COMP_FILTER_COUNT];
    uint8_t sustain_parameter;
    uint8_t attack_parameter;
    uint8_t output_parameter;
    uint8_t clip_parameter;
    bool configured;
} gp100_comp4_t;

/* Sustain and Output are the catalog's integral 0..99 parameters. */
bool gp100_comp_reset(gp100_comp_t *compressor,
                           uint8_t sustain,
                           uint8_t output);

bool gp100_comp_process(gp100_comp_t *compressor,
                             uint8_t sustain,
                             uint8_t output,
                             float *interleaved_stereo,
                             size_t frame_count,
                             bool enabled);

/* Sustain, Attack, Output, and Clip are integral 0..99 parameters. */
bool gp100_comp4_reset(gp100_comp4_t *compressor,
                            uint8_t sustain,
                            uint8_t attack,
                            uint8_t output,
                            uint8_t clip);

bool gp100_comp4_process(gp100_comp4_t *compressor,
                              uint8_t sustain,
                              uint8_t attack,
                              uint8_t output,
                              uint8_t clip,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled);

#ifdef __cplusplus
}
#endif

#endif
