#ifndef GP100_BLUES_OVERDRIVE_H
#define GP100_BLUES_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_blues_filter {
    float x1, x2, x3, y1, y2, y3;
} gp100_blues_filter_t;

typedef struct gp100_blues_overdrive {
    gp100_blues_filter_t gain_filter[2];
    gp100_blues_filter_t intermediate_filter;
    gp100_blues_filter_t tone_filter;
    gp100_blues_filter_t output_filter;
    float oversample_history[2][40];
    float previous_clip_input[2];
    float gain;
    float tone;
    float volume;
    float volume_target;
    bool configured;
} gp100_blues_overdrive_t;

bool gp100_blues_overdrive_reset(gp100_blues_overdrive_t *effect,
                                      uint8_t gain, uint8_t tone,
                                      uint8_t volume);
bool gp100_blues_overdrive_process(
    gp100_blues_overdrive_t *effect, uint8_t gain, uint8_t tone,
    uint8_t volume, float *interleaved_stereo, size_t frame_count,
    bool enabled);

#endif
