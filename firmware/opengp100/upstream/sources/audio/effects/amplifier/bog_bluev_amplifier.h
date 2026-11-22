#ifndef GP100_BOG_BLUEV_AMPLIFIER_H
#define GP100_BOG_BLUEV_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bog_bluev_filter
{
    float x[3];
    float y[3];
} gp100_bog_bluev_filter_t;

typedef struct gp100_bog_bluev_amplifier
{
    gp100_bog_bluev_filter_t input_filter[2];
    gp100_bog_bluev_filter_t gain_filter;
    gp100_bog_bluev_filter_t fixed_filter;
    gp100_bog_bluev_filter_t voicing_filter;
    gp100_bog_bluev_filter_t anti_alias_filter;
    gp100_bog_bluev_filter_t tone_filter;
    gp100_bog_bluev_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float presence, presence_target;
    float master, master_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool configured;
} gp100_bog_bluev_amplifier_t;

bool gp100_bog_bluev_amplifier_reset(
    gp100_bog_bluev_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);

bool gp100_bog_bluev_amplifier_process(
    gp100_bog_bluev_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
