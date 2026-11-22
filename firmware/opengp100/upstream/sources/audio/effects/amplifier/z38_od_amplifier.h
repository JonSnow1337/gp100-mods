#ifndef GP100_Z38_OD_AMPLIFIER_H
#define GP100_Z38_OD_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_z38_od_filter
{
    float x[3], y[3];
} gp100_z38_od_filter_t;
typedef struct gp100_z38_od_amplifier
{
    gp100_z38_od_filter_t input[3], tone, cut, presence_filter;
    float interpolation_history[4], reconstruction_history[40];
    float gain, gain_target, cut_value, cut_target, presence, presence_target;
    float bass, bass_target, middle, middle_target, treble, treble_target;
    bool configured;
} gp100_z38_od_amplifier_t;
bool gp100_z38_od_amplifier_reset(gp100_z38_od_amplifier_t *amplifier,
                                       uint8_t gain, uint8_t cut,
                                       uint8_t presence, uint8_t bass,
                                       uint8_t middle, uint8_t treble);
bool gp100_z38_od_amplifier_process(
    gp100_z38_od_amplifier_t *amplifier, uint8_t gain, uint8_t cut,
    uint8_t presence, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);
#endif
