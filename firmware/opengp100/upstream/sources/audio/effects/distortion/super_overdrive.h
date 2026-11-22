#ifndef GP100_SUPER_OVERDRIVE_H
#define GP100_SUPER_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_super_filter { float x1, x2, x3, y1, y2, y3; }
    gp100_super_filter_t;
typedef struct gp100_super_overdrive {
    gp100_super_filter_t input_filter, gain_filter, tone_filter;
    gp100_super_filter_t output_filter[4];
    float oversample_history[40];
    float previous_clip_input;
    float gain, tone, volume, volume_target;
    bool configured;
} gp100_super_overdrive_t;

bool gp100_super_overdrive_reset(gp100_super_overdrive_t *effect,
                                      uint8_t gain, uint8_t tone,
                                      uint8_t volume);
bool gp100_super_overdrive_process(
    gp100_super_overdrive_t *effect, uint8_t gain, uint8_t tone,
    uint8_t volume, float *interleaved_stereo, size_t frame_count,
    bool enabled);
#endif
