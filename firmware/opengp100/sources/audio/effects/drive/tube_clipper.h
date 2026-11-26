#ifndef GP100_TUBE_CLIPPER_H
#define GP100_TUBE_CLIPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_tube_biquad_state {
    float x1, x2, y1, y2;
} gp100_tube_biquad_state_t;

typedef struct gp100_tube_third_order_state {
    float x1, x2, x3, y1, y2, y3;
} gp100_tube_third_order_state_t;

typedef struct gp100_tube_clipper {
    float input_x1;
    float input_y1;
    gp100_tube_biquad_state_t pre_filter;
    gp100_tube_biquad_state_t post_filter;
    gp100_tube_third_order_state_t tone_stack;
    float oversample_history[40];
    float previous_clip_input;
    float gain;
    float gain_target;
    float volume;
    float volume_target;
    float bass;
    float bass_target;
    float treble;
    float treble_target;
    bool configured;
} gp100_tube_clipper_t;

bool gp100_tube_clipper_reset(gp100_tube_clipper_t *effect,
                                   uint8_t gain,
                                   uint8_t volume,
                                   uint8_t bass,
                                   uint8_t treble);

bool gp100_tube_clipper_process(gp100_tube_clipper_t *effect,
                                     uint8_t gain,
                                     uint8_t volume,
                                     uint8_t bass,
                                     uint8_t treble,
                                     float *interleaved_stereo,
                                     size_t frame_count,
                                     bool enabled);

#endif
