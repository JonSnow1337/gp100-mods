#ifndef GP100_UK800_AMPLIFIER_H
#define GP100_UK800_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_uk800_filter
{
    float x[3];
    float y[3];
} gp100_uk800_filter_t;

typedef struct gp100_uk800_amplifier
{
    gp100_uk800_filter_t input_filter[2];
    gp100_uk800_filter_t gain_filter;
    gp100_uk800_filter_t post_gain_filter;
    gp100_uk800_filter_t tone_filter;
    gp100_uk800_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain;
    float gain_target;
    float presence;
    float master;
    float master_target;
    float bass;
    float middle;
    float treble;
    float treble_target;
    bool configured;
} gp100_uk800_amplifier_t;

bool gp100_uk800_amplifier_reset(gp100_uk800_amplifier_t *amplifier,
                                      uint8_t gain, uint8_t presence,
                                      uint8_t master, uint8_t bass,
                                      uint8_t middle, uint8_t treble);

bool gp100_uk800_amplifier_process(gp100_uk800_amplifier_t *amplifier,
                                        uint8_t gain, uint8_t presence,
                                        uint8_t master, uint8_t bass,
                                        uint8_t middle, uint8_t treble,
                                        float *interleaved_stereo,
                                        size_t frame_count, bool enabled);

#endif
