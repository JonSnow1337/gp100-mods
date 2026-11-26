#ifndef GP100_CLASSIC_BASS_AMPLIFIER_H
#define GP100_CLASSIC_BASS_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_classic_bass_filter
{
    float x[4];
    float y[4];
} gp100_classic_bass_filter_t;

typedef struct gp100_classic_bass_amplifier
{
    gp100_classic_bass_filter_t input_filter[2];
    gp100_classic_bass_filter_t bass_treble_filter;
    gp100_classic_bass_filter_t treble_filter;
    gp100_classic_bass_filter_t middle_filter;
    float gain, gain_target;
    float master, master_target;
    bool configured;
} gp100_classic_bass_amplifier_t;

bool gp100_classic_bass_amplifier_reset(
    gp100_classic_bass_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t middle_range, uint8_t treble, uint8_t master);

bool gp100_classic_bass_amplifier_process(
    gp100_classic_bass_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t middle_range, uint8_t treble, uint8_t master,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
