#ifndef GP100_CHIEF_H
#define GP100_CHIEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_chief_filter {
    float x1, x2, x3, y1, y2, y3;
} gp100_chief_filter_t;

typedef struct gp100_chief {
    gp100_chief_filter_t gain_filter;
    gp100_chief_filter_t pre_clip_filter;
    gp100_chief_filter_t tone_filter;
    float output_x1;
    float output_y1;
    float oversample_history[40];
    float previous_clip_input;
    float control[5];
    float target[5];
    bool configured;
} gp100_chief_t;

bool gp100_chief_reset(gp100_chief_t *effect,
                            uint8_t gain,
                            uint8_t volume,
                            uint8_t bass,
                            uint8_t middle,
                            uint8_t treble);

bool gp100_chief_process(gp100_chief_t *effect,
                              uint8_t gain,
                              uint8_t volume,
                              uint8_t bass,
                              uint8_t middle,
                              uint8_t treble,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled);

#endif
