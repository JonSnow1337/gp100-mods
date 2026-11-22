#ifndef GP100_MINI_BASS_AMPLIFIER_H
#define GP100_MINI_BASS_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_mini_bass_filter
{
    float x[4], y[4];
} gp100_mini_bass_filter_t;
typedef struct gp100_mini_bass_amplifier
{
    gp100_mini_bass_filter_t input, tone, volume_filter;
    float nonlinear_history[40];
    float previous, volume, volume_target, bass, bass_target, treble,
        treble_target;
    bool configured;
} gp100_mini_bass_amplifier_t;
bool gp100_mini_bass_amplifier_reset(gp100_mini_bass_amplifier_t *,
                                          uint8_t volume, uint8_t bass,
                                          uint8_t treble);
bool gp100_mini_bass_amplifier_process(gp100_mini_bass_amplifier_t *,
                                            uint8_t volume, uint8_t bass,
                                            uint8_t treble, float *, size_t,
                                            bool enabled);
#endif
