#ifndef GP100_BASS_PRE_AMPLIFIER_H
#define GP100_BASS_PRE_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_bass_pre_filter
{
    float x[3], y[3];
} gp100_bass_pre_filter_t;
typedef struct gp100_bass_pre_amplifier
{
    gp100_bass_pre_filter_t input[2], tone, bright_off[2], bright_on[2];
    float volume, volume_target, bass, bass_target, middle, middle_target,
        treble, treble_target;
    bool configured;
} gp100_bass_pre_amplifier_t;
bool gp100_bass_pre_amplifier_reset(gp100_bass_pre_amplifier_t *,
                                         uint8_t volume, bool bright,
                                         uint8_t bass, uint8_t middle,
                                         uint8_t treble);
bool gp100_bass_pre_amplifier_process(gp100_bass_pre_amplifier_t *,
                                           uint8_t volume, bool bright,
                                           uint8_t bass, uint8_t middle,
                                           uint8_t treble, float *, size_t,
                                           bool enabled);
#endif
