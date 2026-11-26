#ifndef GP100_DARKTALE_H
#define GP100_DARKTALE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_darktale_filter {
    float x1, x2, x3;
    float y1, y2, y3;
} gp100_darktale_filter_t;

typedef struct gp100_darktale {
    gp100_darktale_filter_t gain_filter;
    gp100_darktale_filter_t post_slew_filter;
    gp100_darktale_filter_t reconstruction_filter;
    gp100_darktale_filter_t tone_filter;
    float oversample_history[3][40];
    float previous_stage_input[3];
    float volume, volume_target;
    bool configured;
} gp100_darktale_t;

bool gp100_darktale_reset(gp100_darktale_t *effect,
                               uint8_t gain, uint8_t tone, uint8_t volume);
bool gp100_darktale_process(gp100_darktale_t *effect,
                                 uint8_t gain, uint8_t tone, uint8_t volume,
                                 float *interleaved_stereo,
                                 size_t frame_count, bool enabled);

#endif
