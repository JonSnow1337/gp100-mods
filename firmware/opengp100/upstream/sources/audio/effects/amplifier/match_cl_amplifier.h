#ifndef GP100_MATCH_CL_AMPLIFIER_H
#define GP100_MATCH_CL_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_match_cl_filter
{
    float x[3];
    float y[3];
} gp100_match_cl_filter_t;

typedef struct gp100_match_cl_amplifier
{
    gp100_match_cl_filter_t input_filter;
    gp100_match_cl_filter_t bass_filter;
    gp100_match_cl_filter_t middle_filter;
    gp100_match_cl_filter_t tone_filter;
    gp100_match_cl_filter_t presence_filter;
    float interpolation_history[4];
    float reconstruction_history[40];
    float gain, gain_target;
    float master, master_target;
    float presence;
    float bass;
    float middle;
    float treble;
    bool configured;
} gp100_match_cl_amplifier_t;

bool gp100_match_cl_amplifier_reset(
    gp100_match_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_match_cl_amplifier_process(
    gp100_match_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
