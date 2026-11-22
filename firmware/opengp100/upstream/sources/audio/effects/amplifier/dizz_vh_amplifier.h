#ifndef GP100_DIZZ_VH_AMPLIFIER_H
#define GP100_DIZZ_VH_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_dizz_vh_filter
{
    float x[3], y[3];
} gp100_dizz_vh_filter_t;

typedef struct gp100_dizz_vh_amplifier
{
    gp100_dizz_vh_filter_t input_filter;
    gp100_dizz_vh_filter_t anti_alias_filter;
    gp100_dizz_vh_filter_t tone_filter;
    gp100_dizz_vh_filter_t gain_filter;
    gp100_dizz_vh_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float presence, presence_target;
    float master, master_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool configured;
} gp100_dizz_vh_amplifier_t;

bool gp100_dizz_vh_amplifier_reset(
    gp100_dizz_vh_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_dizz_vh_amplifier_process(
    gp100_dizz_vh_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
