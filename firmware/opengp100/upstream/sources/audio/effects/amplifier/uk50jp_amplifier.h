#ifndef GP100_UK50JP_AMPLIFIER_H
#define GP100_UK50JP_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_uk50jp_filter
{
    float x[3];
    float y[3];
} gp100_uk50jp_filter_t;

typedef struct gp100_uk50jp_amplifier
{
    gp100_uk50jp_filter_t channel_one_input[2];
    gp100_uk50jp_filter_t channel_two_input[2];
    gp100_uk50jp_filter_t volume_one_filter;
    gp100_uk50jp_filter_t volume_two_filter;
    gp100_uk50jp_filter_t channel_two_voicing[2];
    gp100_uk50jp_filter_t tone_filter;
    gp100_uk50jp_filter_t presence_filter;
    float fir_history[2][40];
    float previous_nonlinear_input[2];
    float volume_one;
    float volume_one_target;
    float volume_two;
    float volume_two_target;
    float presence;
    float output;
    float output_target;
    float bass;
    float middle;
    float middle_target;
    float treble;
    float treble_target;
    bool configured;
} gp100_uk50jp_amplifier_t;

bool gp100_uk50jp_amplifier_reset(gp100_uk50jp_amplifier_t *amplifier,
                                       uint8_t volume_one, uint8_t presence,
                                       uint8_t output, uint8_t bass,
                                       uint8_t middle, uint8_t treble,
                                       uint8_t volume_two);

bool gp100_uk50jp_amplifier_process(
    gp100_uk50jp_amplifier_t *amplifier, uint8_t volume_one,
    uint8_t presence, uint8_t output, uint8_t bass, uint8_t middle,
    uint8_t treble, uint8_t volume_two, float *interleaved_stereo,
    size_t frame_count, bool enabled);

#endif
