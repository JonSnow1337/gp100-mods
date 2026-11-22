#ifndef GP100_LAZARO_H
#define GP100_LAZARO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_lazaro_filter { float x1, x2, y1, y2; }
    gp100_lazaro_filter_t;

typedef struct gp100_lazaro {
    gp100_lazaro_filter_t input_filter;
    gp100_lazaro_filter_t gain_filter;
    gp100_lazaro_filter_t nonlinear_filter[2];
    gp100_lazaro_filter_t tone_filter;
    float oversample_history[2][40];
    float previous_clip_input[2];
    float volume;
    float volume_target;
    bool configured;
} gp100_lazaro_t;

bool gp100_lazaro_reset(gp100_lazaro_t *effect,
                             uint8_t gain, uint8_t tone, uint8_t volume);
bool gp100_lazaro_process(gp100_lazaro_t *effect,
                               uint8_t gain, uint8_t tone, uint8_t volume,
                               float *interleaved_stereo, size_t frame_count,
                               bool enabled);
#endif
