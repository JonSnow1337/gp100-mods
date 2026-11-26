#ifndef GP100_TWEEDY_AMPLIFIER_H
#define GP100_TWEEDY_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_tweedy_filter
{
    float x1, x2;
    float y1, y2;
} gp100_tweedy_filter_t;

typedef struct gp100_tweedy_amplifier
{
    gp100_tweedy_filter_t input_filter[2];
    gp100_tweedy_filter_t tone_filter;
    gp100_tweedy_filter_t drive_filter;
    gp100_tweedy_filter_t output_filter;
    gp100_tweedy_filter_t envelope_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float tone, tone_target;
    float volume, volume_target;
    bool configured;
} gp100_tweedy_amplifier_t;

bool gp100_tweedy_amplifier_reset(gp100_tweedy_amplifier_t *amplifier,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume);

bool gp100_tweedy_amplifier_process(
    gp100_tweedy_amplifier_t *amplifier, uint8_t gain, uint8_t tone,
    uint8_t volume, float *interleaved_stereo, size_t frame_count,
    bool enabled);

#endif
