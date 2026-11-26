#ifndef GP100_TAICHI_OVERDRIVE_H
#define GP100_TAICHI_OVERDRIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_taichi_biquad_state {
    float x1, x2, y1, y2;
} gp100_taichi_biquad_state_t;

typedef struct gp100_taichi_overdrive {
    gp100_taichi_biquad_state_t input_filter;
    float output_x1;
    float output_y1;
    float oversample_history[40];
    float previous_clip_input;
    float gain;
    float tone;
    float volume;
    float volume_target;
    float voice;
    float voice_target;
    bool configured;
} gp100_taichi_overdrive_t;

bool gp100_taichi_overdrive_reset(gp100_taichi_overdrive_t *effect,
                                       uint8_t gain,
                                       uint8_t tone,
                                       uint8_t volume,
                                       uint8_t voice);

bool gp100_taichi_overdrive_process(
    gp100_taichi_overdrive_t *effect,
    uint8_t gain,
    uint8_t tone,
    uint8_t volume,
    uint8_t voice,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#endif
