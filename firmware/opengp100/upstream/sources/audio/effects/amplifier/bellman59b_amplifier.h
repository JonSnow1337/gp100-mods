#ifndef GP100_BELLMAN59B_AMPLIFIER_H
#define GP100_BELLMAN59B_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bellman59b_filter
{
    float x[3];
    float y[3];
} gp100_bellman59b_filter_t;

typedef struct gp100_bellman59b_amplifier
{
    gp100_bellman59b_filter_t input_filter[2];
    gp100_bellman59b_filter_t volume_filter;
    gp100_bellman59b_filter_t tone_filter;
    gp100_bellman59b_filter_t envelope_filter;
    gp100_bellman59b_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float volume;
    float volume_target;
    float presence;
    float output;
    float output_target;
    float bass;
    float middle;
    float middle_target;
    float treble;
    float treble_target;
    bool configured;
} gp100_bellman59b_amplifier_t;

bool gp100_bellman59b_amplifier_reset(
    gp100_bellman59b_amplifier_t *amplifier, uint8_t volume,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble);

bool gp100_bellman59b_amplifier_process(
    gp100_bellman59b_amplifier_t *amplifier, uint8_t volume,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble, float *interleaved_stereo, size_t frame_count,
    bool enabled);

#endif
