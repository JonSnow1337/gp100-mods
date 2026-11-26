#ifndef GP100_BASS_DISTORTION_H
#define GP100_BASS_DISTORTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bass_distortion_filter {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
} gp100_bass_distortion_filter_t;

typedef struct gp100_bass_distortion {
    gp100_bass_distortion_filter_t input_filter[2];
    gp100_bass_distortion_filter_t post_clip_filter;
    gp100_bass_distortion_filter_t reconstruction_filter;
    gp100_bass_distortion_filter_t tone_filter;
    float oversample_history[2][40];
    float previous_stage_input[2];
    float gain;
    float blend, blend_target;
    float volume, volume_target;
    float bass;
    float treble, treble_target;
    bool configured;
} gp100_bass_distortion_t;

bool gp100_bass_distortion_reset(
    gp100_bass_distortion_t *effect, uint8_t gain, uint8_t blend,
    uint8_t volume, uint8_t bass, uint8_t treble);

bool gp100_bass_distortion_process(
    gp100_bass_distortion_t *effect, uint8_t gain, uint8_t blend,
    uint8_t volume, uint8_t bass, uint8_t treble,
    float *interleaved_stereo, size_t frame_count, bool enabled);

#endif
