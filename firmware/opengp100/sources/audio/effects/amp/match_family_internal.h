#ifndef GP100_MATCH_FAMILY_INTERNAL_H
#define GP100_MATCH_FAMILY_INTERNAL_H

#include "match_cl_amplifier.h"

float gp100_match_family_gain_control(uint8_t gain);

bool gp100_match_family_reset(
    gp100_match_cl_amplifier_t *amplifier, float gain_value,
    uint8_t presence, uint8_t master,
    uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_match_family_process(
    gp100_match_cl_amplifier_t *amplifier, float gain_target,
    uint8_t presence, uint8_t master,
    uint8_t bass, uint8_t middle, uint8_t treble,
    float input_drive_scale, float output_scale,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
