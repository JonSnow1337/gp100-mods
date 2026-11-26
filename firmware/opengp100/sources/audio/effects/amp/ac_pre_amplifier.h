#ifndef GP100_AC_PRE_AMPLIFIER_H
#define GP100_AC_PRE_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct gp100_ac_pre_filter
{
    float x[2], y[2];
} gp100_ac_pre_filter_t;
typedef struct gp100_ac_pre_amplifier
{
    gp100_ac_pre_filter_t tone[2], equalizer;
    float volume, volume_target, tone_value, tone_target, balance,
        balance_target, frequency, frequency_target, q, q_target, gain,
        gain_target;
    bool configured;
} gp100_ac_pre_amplifier_t;
bool gp100_ac_pre_amplifier_reset(gp100_ac_pre_amplifier_t *,
                                       uint8_t volume, uint8_t tone,
                                       uint8_t balance, uint8_t frequency,
                                       uint8_t q, uint8_t gain);
bool gp100_ac_pre_amplifier_process(gp100_ac_pre_amplifier_t *,
                                         uint8_t volume, uint8_t tone,
                                         uint8_t balance, uint8_t frequency,
                                         uint8_t q, uint8_t gain, float *,
                                         size_t, bool enabled);
#endif
