#ifndef GP100_LSTAR_OD_AMPLIFIER_H
#define GP100_LSTAR_OD_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_lstar_od_filter
{
    float x[3];
    float y[3];
} gp100_lstar_od_filter_t;

typedef struct gp100_lstar_od_amplifier
{
    gp100_lstar_od_filter_t input_filter;
    gp100_lstar_od_filter_t conditioning_filter;
    gp100_lstar_od_filter_t tone_filter;
    gp100_lstar_od_filter_t gain_filter;
    gp100_lstar_od_filter_t master_presence_filter;
    gp100_lstar_od_filter_t envelope_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain;
    float gain_target;
    float presence;
    float presence_target;
    float master;
    float master_target;
    float bass;
    float bass_target;
    float middle;
    float middle_target;
    float treble;
    float treble_target;
    bool configured;
} gp100_lstar_od_amplifier_t;

bool gp100_lstar_od_amplifier_reset(
    gp100_lstar_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_lstar_od_amplifier_process(
    gp100_lstar_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
