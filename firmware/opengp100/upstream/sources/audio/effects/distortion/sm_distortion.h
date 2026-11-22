#ifndef GP100_SM_DISTORTION_H
#define GP100_SM_DISTORTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_sm_distortion_filter {
    float x1, x2;
    float y1, y2;
} gp100_sm_distortion_filter_t;

typedef struct gp100_sm_distortion {
    gp100_sm_distortion_filter_t input_filter;
    gp100_sm_distortion_filter_t post_first_clip_filter;
    gp100_sm_distortion_filter_t gain_filter;
    gp100_sm_distortion_filter_t post_second_clip_filter;
    gp100_sm_distortion_filter_t tone_filter;
    float oversample_history[3][40];
    float previous_stage_input[3];
    float volume, volume_target;
    bool configured;
} gp100_sm_distortion_t;

bool gp100_sm_distortion_reset(gp100_sm_distortion_t *effect,
                                    uint8_t gain, uint8_t tone,
                                    uint8_t volume);
bool gp100_sm_distortion_process(gp100_sm_distortion_t *effect,
                                      uint8_t gain, uint8_t tone,
                                      uint8_t volume,
                                      float *interleaved_stereo,
                                      size_t frame_count, bool enabled);

#endif
