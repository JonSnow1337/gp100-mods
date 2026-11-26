#ifndef GP100_KNIGHTS_OD_AMPLIFIER_H
#define GP100_KNIGHTS_OD_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_knights_od_filter
{
    float x[3], y[3];
} gp100_knights_od_filter_t;

typedef struct gp100_knights_od_amplifier
{
    gp100_knights_od_filter_t input_filter[2];
    gp100_knights_od_filter_t preamp_filter;
    gp100_knights_od_filter_t anti_alias_filter;
    gp100_knights_od_filter_t tone_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain, gain_target;
    float volume, volume_target;
    float bass, bass_target;
    float middle, middle_target;
    float treble, treble_target;
    bool configured;
} gp100_knights_od_amplifier_t;

bool gp100_knights_od_amplifier_reset(
    gp100_knights_od_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t treble, uint8_t volume);

bool gp100_knights_od_amplifier_process(
    gp100_knights_od_amplifier_t *amplifier, uint8_t gain, uint8_t bass,
    uint8_t middle, uint8_t treble, uint8_t volume, float *interleaved_stereo,
    size_t frame_count, bool enabled);

#endif
