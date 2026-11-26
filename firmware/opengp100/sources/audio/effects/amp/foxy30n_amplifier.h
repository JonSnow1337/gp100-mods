#ifndef GP100_FOXY30N_AMPLIFIER_H
#define GP100_FOXY30N_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_foxy30n_filter
{
    float x[3];
    float y[3];
} gp100_foxy30n_filter_t;

typedef struct gp100_foxy30n_amplifier
{
    gp100_foxy30n_filter_t input_filter[2];
    gp100_foxy30n_filter_t normal_volume_filter;
    gp100_foxy30n_filter_t bright_volume_filter;
    gp100_foxy30n_filter_t cut_filter;
    gp100_foxy30n_filter_t envelope_filter;
    gp100_foxy30n_filter_t output_shelf;
    float fir_history[40];
    float previous_nonlinear_input;
    float volume, volume_target;
    float cut, cut_target;
    float master, master_target;
    bool bright;
    bool configured;
} gp100_foxy30n_amplifier_t;

bool gp100_foxy30n_amplifier_reset(
    gp100_foxy30n_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, bool bright);

bool gp100_foxy30n_amplifier_process(
    gp100_foxy30n_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, bool bright, float *interleaved_stereo, size_t frame_count,
    bool enabled);

#endif
