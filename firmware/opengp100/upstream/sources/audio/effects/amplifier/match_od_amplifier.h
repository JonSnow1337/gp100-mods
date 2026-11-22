#ifndef GP100_MATCH_OD_AMPLIFIER_H
#define GP100_MATCH_OD_AMPLIFIER_H

#include "match_cl_amplifier.h"

typedef struct gp100_match_od_amplifier
{
    gp100_match_cl_amplifier_t core;
} gp100_match_od_amplifier_t;

bool gp100_match_od_amplifier_reset(
    gp100_match_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_match_od_amplifier_process(
    gp100_match_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
