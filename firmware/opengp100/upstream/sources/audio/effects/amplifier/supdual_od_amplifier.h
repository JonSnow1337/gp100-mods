#ifndef GP100_SUPDUAL_OD_AMPLIFIER_H
#define GP100_SUPDUAL_OD_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_supdual_od_filter
{
    float x[3];
    float y[3];
} gp100_supdual_od_filter_t;

typedef struct gp100_supdual_od_channel
{
    gp100_supdual_od_filter_t input_filter[2];
    gp100_supdual_od_filter_t tone_filter;
    float fir_history[40];
    float previous_nonlinear_input;
} gp100_supdual_od_channel_t;

typedef struct gp100_supdual_od_amplifier
{
    gp100_supdual_od_channel_t channel[2];
    float volume_1;
    float volume_1_target;
    float tone_1;
    float tone_1_target;
    float volume_2;
    float volume_2_target;
    float tone_2;
    float tone_2_target;
    float output;
    float output_target;
    bool configured;
} gp100_supdual_od_amplifier_t;

bool gp100_supdual_od_amplifier_reset(
    gp100_supdual_od_amplifier_t *amplifier, uint8_t volume_1,
    uint8_t tone_1, uint8_t volume_2, uint8_t tone_2, uint8_t output);

bool gp100_supdual_od_amplifier_process(
    gp100_supdual_od_amplifier_t *amplifier, uint8_t volume_1,
    uint8_t tone_1, uint8_t volume_2, uint8_t tone_2, uint8_t output,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
