#ifndef GP100_NOISE_GATE_H
#define GP100_NOISE_GATE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_NOISE_GATE_SAMPLE_RATE 44100.0f

typedef enum gp100_noise_gate_type {
    GP100_NOISE_GATE_1 = 0,
    GP100_NOISE_GATE_2 = 1
} gp100_noise_gate_type_t;

typedef struct gp100_noise_gate {
    gp100_noise_gate_type_t type;
    float threshold_parameter;
    float attack_parameter;
    float release_parameter;
    float threshold_linear;
    float attack_seconds;
    float release_seconds;
    float attack_coefficient;
    float release_coefficient;
    float gain;
    float hold_counter;
    bool configured;
} gp100_noise_gate_t;

/*
 * Gate 1 consumes parameters[0] (threshold). Gate 2 consumes threshold,
 * attack, and release from parameters[0..2]. All are factory 0..99 values.
 */
bool gp100_noise_gate_reset(
    gp100_noise_gate_t *gate,
    gp100_noise_gate_type_t type,
    const float *parameters);

/*
 * Processes interleaved stereo frames in place. The factory detector is the
 * right channel of detector_interleaved; the gated left input is copied to
 * both output channels. A false enabled value leaves audio and state intact.
 */
bool gp100_noise_gate_process(
    gp100_noise_gate_t *gate,
    const float *parameters,
    float *interleaved_stereo,
    const float *detector_interleaved,
    size_t frame_count,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
