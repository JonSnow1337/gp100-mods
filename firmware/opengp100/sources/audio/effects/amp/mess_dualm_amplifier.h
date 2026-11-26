#ifndef GP100_MESS_DUALM_AMPLIFIER_H
#define GP100_MESS_DUALM_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_mess_dualm_filter
{
    float x[3], y[3];
} gp100_mess_dualm_filter_t;
typedef struct gp100_mess_dualm_amplifier
{
    gp100_mess_dualm_filter_t input_filter, gain_filter, anti_alias_filter;
    gp100_mess_dualm_filter_t tone_filter, presence_filter;
    float fir_history[2][40], previous_nonlinear_input[2];
    float gain, gain_target, presence, presence_target, master, master_target;
    float bass, bass_target, middle, middle_target, treble, treble_target;
    bool configured;
} gp100_mess_dualm_amplifier_t;
bool gp100_mess_dualm_amplifier_reset(
    gp100_mess_dualm_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble);
bool gp100_mess_dualm_amplifier_process(
    gp100_mess_dualm_amplifier_t *amplifier, uint8_t gain,
    uint8_t presence, uint8_t master, uint8_t bass, uint8_t middle,
    uint8_t treble, float *interleaved_stereo, size_t frame_count,
    bool enabled);
#endif
