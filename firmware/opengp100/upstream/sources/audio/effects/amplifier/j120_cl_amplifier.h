#ifndef GP100_J120_CL_AMPLIFIER_H
#define GP100_J120_CL_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_j120_tone_filter
{
    double x[3];
    double y[3];
} gp100_j120_tone_filter_t;

typedef struct gp100_j120_cl_amplifier
{
    gp100_j120_tone_filter_t tone_filter;
    float bright_x1;
    float bright_y1;
    float volume, volume_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool bright;
    bool configured;
} gp100_j120_cl_amplifier_t;

bool gp100_j120_cl_amplifier_reset(
    gp100_j120_cl_amplifier_t *amplifier, uint8_t volume, uint8_t bass,
    uint8_t middle, uint8_t treble, bool bright);

bool gp100_j120_cl_amplifier_process(
    gp100_j120_cl_amplifier_t *amplifier, uint8_t volume, uint8_t bass,
    uint8_t middle, uint8_t treble, bool bright, float *interleaved_stereo,
    size_t frame_count, bool enabled);

#endif
