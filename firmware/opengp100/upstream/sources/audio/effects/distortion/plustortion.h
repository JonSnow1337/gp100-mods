#ifndef GP100_PLUSTORTION_H
#define GP100_PLUSTORTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_plustortion_filter {
    float x1, x2, y1, y2;
} gp100_plustortion_filter_t;

typedef struct gp100_plustortion {
    gp100_plustortion_filter_t gain_filter;
    gp100_plustortion_filter_t pre_clip_filter[2];
    gp100_plustortion_filter_t interstage_filter;
    gp100_plustortion_filter_t output_filter[4];
    float clip_history[2][40];
    float previous_clip_input[2];
    float volume;
    float volume_target;
    bool configured;
} gp100_plustortion_t;

bool gp100_plustortion_reset(gp100_plustortion_t *effect,
                                  uint8_t gain,
                                  uint8_t volume);

bool gp100_plustortion_process(gp100_plustortion_t *effect,
                                    uint8_t gain,
                                    uint8_t volume,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#endif
