#ifndef GP100_BELLMAN59N_AMPLIFIER_H
#define GP100_BELLMAN59N_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bellman59n_filter
{
    float x1, x2, x3;
    float y1, y2, y3;
} gp100_bellman59n_filter_t;

typedef struct gp100_bellman59n_amplifier
{
    gp100_bellman59n_filter_t input_filter[2];
    gp100_bellman59n_filter_t gain_filter;
    gp100_bellman59n_filter_t tone_filter;
    gp100_bellman59n_filter_t envelope_filter;
    gp100_bellman59n_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float presence;
    float volume, volume_target;
    float bass;
    float middle, middle_target;
    float treble, treble_target;
    bool configured;
} gp100_bellman59n_amplifier_t;

bool gp100_bellman59n_amplifier_reset(
    gp100_bellman59n_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t volume, uint8_t bass, uint8_t middle,
    uint8_t treble);

bool gp100_bellman59n_amplifier_process(
    gp100_bellman59n_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t volume, uint8_t bass, uint8_t middle,
    uint8_t treble, float *interleaved_stereo, size_t frame_count,
    bool enabled);

#endif
