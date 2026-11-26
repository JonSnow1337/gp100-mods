#ifndef GP100_YELLOW_OVERDRIVE_H
#define GP100_YELLOW_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_yellow_filter { float x1, x2, y1, y2; }
    gp100_yellow_filter_t;

typedef struct gp100_yellow_overdrive {
    gp100_yellow_filter_t input_filter;
    gp100_yellow_filter_t gain_filter;
    gp100_yellow_filter_t output_filter[2];
    float oversample_history[40];
    float previous_clip_input;
    float gain;
    float volume;
    float volume_target;
    bool configured;
} gp100_yellow_overdrive_t;

bool gp100_yellow_overdrive_reset(gp100_yellow_overdrive_t *effect,
                                       uint8_t gain, uint8_t volume);
bool gp100_yellow_overdrive_process(
    gp100_yellow_overdrive_t *effect, uint8_t gain, uint8_t volume,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
