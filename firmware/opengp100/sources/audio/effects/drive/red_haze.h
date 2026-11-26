#ifndef GP100_RED_HAZE_H
#define GP100_RED_HAZE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_red_haze_filter { float x1, x2, y1, y2; }
    gp100_red_haze_filter_t;

typedef struct gp100_red_haze {
    gp100_red_haze_filter_t input_filter;
    gp100_red_haze_filter_t preclip_filter;
    gp100_red_haze_filter_t output_filter;
    float moving_average[2][4];
    float previous_preclip;
    float gain, gain_target;
    float volume, volume_target;
    bool configured;
} gp100_red_haze_t;

bool gp100_red_haze_reset(gp100_red_haze_t *effect,
                               uint8_t gain, uint8_t volume);
bool gp100_red_haze_process(gp100_red_haze_t *effect,
                                 uint8_t gain, uint8_t volume,
                                 float *interleaved_stereo,
                                 size_t frame_count, bool enabled);
#endif
