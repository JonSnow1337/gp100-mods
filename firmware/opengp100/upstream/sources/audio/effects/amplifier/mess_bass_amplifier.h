#ifndef GP100_MESS_BASS_AMPLIFIER_H
#define GP100_MESS_BASS_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_mess_bass_filter
{
    float x[3], y[3];
} gp100_mess_bass_filter_t;
typedef struct gp100_mess_bass_amplifier
{
    gp100_mess_bass_filter_t input, volume_filter, fixed_filter, tone,
        post[6];
    float nonlinear_history[40], previous, volume, volume_target, master,
        master_target, treble, treble_target;
    bool configured;
} gp100_mess_bass_amplifier_t;
bool gp100_mess_bass_amplifier_reset(gp100_mess_bass_amplifier_t *,
                                          uint8_t volume, uint8_t master,
                                          uint8_t bass, uint8_t middle,
                                          uint8_t treble);
bool gp100_mess_bass_amplifier_process(gp100_mess_bass_amplifier_t *,
                                            uint8_t volume, uint8_t master,
                                            uint8_t bass, uint8_t middle,
                                            uint8_t treble, float *, size_t,
                                            bool enabled);
#endif
