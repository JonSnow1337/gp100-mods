#ifndef GP100_GREEN_OVERDRIVE_H
#define GP100_GREEN_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_green_filter {
    float x1, x2, y1, y2;
} gp100_green_filter_t;

typedef struct gp100_green_overdrive {
    gp100_green_filter_t input_filter[2];
    gp100_green_filter_t gain_filter;
    gp100_green_filter_t tone_filter;
    gp100_green_filter_t output_filter[3];
    float oversample_history[40];
    float previous_clip_input;
    float gain;
    float tone;
    float volume;
    float volume_target;
    bool configured;
} gp100_green_overdrive_t;

bool gp100_green_overdrive_reset(gp100_green_overdrive_t *effect,
                                      uint8_t gain,
                                      uint8_t tone,
                                      uint8_t volume);

bool gp100_green_overdrive_process(
    gp100_green_overdrive_t *effect,
    uint8_t gain,
    uint8_t tone,
    uint8_t volume,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#endif
