#ifndef GP100_DARK_TWIN_AMPLIFIER_H
#define GP100_DARK_TWIN_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_dark_twin_filter
{
    float x[3];
    float y[3];
} gp100_dark_twin_filter_t;

typedef struct gp100_dark_twin_amplifier
{
    gp100_dark_twin_filter_t input_filter[3];
    gp100_dark_twin_filter_t tone_filter;
    gp100_dark_twin_filter_t bright_filter;
    gp100_dark_twin_filter_t voicing_filter[2];
    gp100_dark_twin_filter_t envelope_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float volume, volume_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool bright;
    bool configured;
} gp100_dark_twin_amplifier_t;

bool gp100_dark_twin_amplifier_reset(
    gp100_dark_twin_amplifier_t *amplifier, uint8_t gain, uint8_t volume,
    uint8_t bass, uint8_t middle, uint8_t treble, bool bright);

bool gp100_dark_twin_amplifier_process(
    gp100_dark_twin_amplifier_t *amplifier, uint8_t gain, uint8_t volume,
    uint8_t bass, uint8_t middle, uint8_t treble, bool bright,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
