#ifndef GP100_FLAGMAN_AMPLIFIER_H
#define GP100_FLAGMAN_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_flagman_filter
{
    float x[3], y[3];
} gp100_flagman_filter_t;
typedef struct gp100_flagman_amplifier
{
    gp100_flagman_filter_t input[4], notch, tone, presence_filter;
    float fir_history[40], previous_nonlinear_input;
    float gain, gain_target, presence, master, master_target;
    float bass, bass_target, middle, middle_target, treble, treble_target;
    bool configured;
} gp100_flagman_amplifier_t;
bool gp100_flagman_amplifier_reset(
    gp100_flagman_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble);
bool gp100_flagman_amplifier_process(
    gp100_flagman_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);
#endif
