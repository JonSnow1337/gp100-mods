#ifndef GP100_BOGSV_CL_AMPLIFIER_H
#define GP100_BOGSV_CL_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bogsv_cl_filter
{
    float x[3];
    float y[3];
} gp100_bogsv_cl_filter_t;

typedef struct gp100_bogsv_cl_amplifier
{
    gp100_bogsv_cl_filter_t input_filter[2];
    gp100_bogsv_cl_filter_t tone_filter;
    gp100_bogsv_cl_filter_t bright_filter;
    gp100_bogsv_cl_filter_t voicing_filter[3];
    gp100_bogsv_cl_filter_t envelope_filter;
    gp100_bogsv_cl_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float presence;
    float master, master_target;
    float bass, bass_target;
    float treble, treble_target;
    bool bright;
    bool configured;
} gp100_bogsv_cl_amplifier_t;

bool gp100_bogsv_cl_amplifier_reset(
    gp100_bogsv_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t treble, bool bright);

bool gp100_bogsv_cl_amplifier_process(
    gp100_bogsv_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t treble, bool bright,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
