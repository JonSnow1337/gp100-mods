#ifndef GP100_FLEX_OVERDRIVE_H
#define GP100_FLEX_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_flex_overdrive_filter {
    float x1, x2;
    float y1, y2;
} gp100_flex_overdrive_filter_t;

typedef struct gp100_flex_overdrive {
    gp100_flex_overdrive_filter_t input_filter;
    gp100_flex_overdrive_filter_t gain_filter;
    gp100_flex_overdrive_filter_t tone_filter;
    gp100_flex_overdrive_filter_t mode_filter[3];
    float oversample_history[40];
    float previous_clip_input;
    float volume, volume_target;
    float blend, blend_target;
    bool configured;
} gp100_flex_overdrive_t;

bool gp100_flex_overdrive_reset(gp100_flex_overdrive_t *effect,
                                     uint8_t gain, uint8_t tone,
                                     uint8_t volume, uint8_t mode,
                                     uint8_t blend);
bool gp100_flex_overdrive_process(gp100_flex_overdrive_t *effect,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume, uint8_t mode,
                                       uint8_t blend,
                                       float *interleaved_stereo,
                                       size_t frame_count, bool enabled);

#endif
