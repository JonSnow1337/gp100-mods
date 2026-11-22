#ifndef GP100_SATURATE_H
#define GP100_SATURATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_saturate_filter {
    float x1;
    float x2;
    float y1;
    float y2;
} gp100_saturate_filter_t;

typedef struct gp100_saturate {
    gp100_saturate_filter_t input_filter;
    gp100_saturate_filter_t reconstruction_filter;
    gp100_saturate_filter_t high_cut_filter;
    float shaped_history[40];
    float previous_oversample_input;
    float gain;
    float mix;
    float output_level;
    float high_cut_control;
    float gain_target;
    float mix_target;
    float output_target;
    float high_cut_target;
    bool configured;
} gp100_saturate_t;

/* Gain, Mix, Output, and H-Cut are all 0..99. */
bool gp100_saturate_reset(gp100_saturate_t *saturate,
                               uint8_t gain,
                               uint8_t mix,
                               uint8_t output,
                               uint8_t high_cut);

bool gp100_saturate_process(gp100_saturate_t *saturate,
                                 uint8_t gain,
                                 uint8_t mix,
                                 uint8_t output,
                                 uint8_t high_cut,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled);

#endif
