#ifndef GP100_FOXY30TB_AMPLIFIER_H
#define GP100_FOXY30TB_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_foxy30tb_filter
{
    float x[3];
    float y[3];
} gp100_foxy30tb_filter_t;

typedef struct gp100_foxy30tb_amplifier
{
    gp100_foxy30tb_filter_t input_filter[2];
    gp100_foxy30tb_filter_t volume_filter;
    gp100_foxy30tb_filter_t character_tone_filter;
    gp100_foxy30tb_filter_t cut_contour_filter;
    gp100_foxy30tb_filter_t envelope_filter;
    gp100_foxy30tb_filter_t cut_shelf_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float volume;
    float volume_target;
    float cut;
    float cut_target;
    float master;
    float master_target;
    float bass;
    float bass_target;
    float treble;
    float treble_target;
    bool character;
    bool configured;
} gp100_foxy30tb_amplifier_t;

bool gp100_foxy30tb_amplifier_reset(
    gp100_foxy30tb_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, uint8_t bass, uint8_t treble, bool character);

bool gp100_foxy30tb_amplifier_process(
    gp100_foxy30tb_amplifier_t *amplifier, uint8_t volume, uint8_t cut,
    uint8_t master, uint8_t bass, uint8_t treble, bool character,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
