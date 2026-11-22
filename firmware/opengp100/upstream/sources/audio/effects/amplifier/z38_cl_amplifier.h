#ifndef GP100_Z38_CL_AMPLIFIER_H
#define GP100_Z38_CL_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_z38_cl_filter
{
    float x[3], y[3];
} gp100_z38_cl_filter_t;

typedef struct gp100_z38_cl_amplifier
{
    gp100_z38_cl_filter_t voicing[3];
    gp100_z38_cl_filter_t tone;
    gp100_z38_cl_filter_t cut_filter;
    gp100_z38_cl_filter_t output_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float cut, cut_target;
    float master, master_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool configured;
} gp100_z38_cl_amplifier_t;

bool gp100_z38_cl_amplifier_reset(gp100_z38_cl_amplifier_t *amplifier,
                                       uint8_t gain, uint8_t cut,
                                       uint8_t master, uint8_t bass,
                                       uint8_t middle, uint8_t treble);

bool gp100_z38_cl_amplifier_process(
    gp100_z38_cl_amplifier_t *amplifier, uint8_t gain, uint8_t cut,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
