#ifndef GP100_UK45_AMPLIFIER_H
#define GP100_UK45_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_uk45_filter
{
    float x[3], y[3];
} gp100_uk45_filter_t;
typedef struct gp100_uk45_amplifier
{
    gp100_uk45_filter_t input_filter[3], volume_filter, notch_filter;
    gp100_uk45_filter_t tone_filter, presence_filter;
    float fir_history[2][40], previous_nonlinear_input[2];
    float volume, presence, output, output_target, bass, middle, treble,
        treble_target;
    bool configured;
} gp100_uk45_amplifier_t;
bool gp100_uk45_amplifier_reset(gp100_uk45_amplifier_t *amplifier,
                                     uint8_t volume, uint8_t presence,
                                     uint8_t output, uint8_t bass,
                                     uint8_t middle, uint8_t treble);
bool gp100_uk45_amplifier_process(gp100_uk45_amplifier_t *amplifier,
                                       uint8_t volume, uint8_t presence,
                                       uint8_t output, uint8_t bass,
                                       uint8_t middle, uint8_t treble,
                                       float *interleaved_stereo,
                                       size_t frame_count, bool enabled);
#endif
