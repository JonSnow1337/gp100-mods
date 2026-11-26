#ifndef GP100_BAD_KT_OD_AMPLIFIER_H
#define GP100_BAD_KT_OD_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bad_kt_od_filter
{
    float x[3];
    float y[3];
} gp100_bad_kt_od_filter_t;

typedef struct gp100_bad_kt_od_amplifier
{
    gp100_bad_kt_od_filter_t input_filter[2];
    gp100_bad_kt_od_filter_t edge_filter;
    gp100_bad_kt_od_filter_t tone_filter;
    gp100_bad_kt_od_filter_t output_filter[3];
    gp100_bad_kt_od_filter_t presence_filter;
    float interpolation_history[4];
    float reconstruction_history[40];
    float gain;
    float gain_target;
    float presence;
    float master;
    float master_target;
    float bass;
    float edge;
    float treble;
    float treble_target;
    bool configured;
} gp100_bad_kt_od_amplifier_t;

bool gp100_bad_kt_od_amplifier_reset(
    gp100_bad_kt_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t edge, uint8_t treble);

bool gp100_bad_kt_od_amplifier_process(
    gp100_bad_kt_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t edge, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
