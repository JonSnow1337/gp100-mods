#ifndef GP100_PITCH_BENDER_H
#define GP100_PITCH_BENDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_PITCH_BENDER_DELAY_SAMPLES 4096u

typedef struct gp100_pitch_bender {
    float delay[GP100_PITCH_BENDER_DELAY_SAMPLES];
    uint32_t phase[2];
    size_t write_index;
    float previous_filter_input;
    float previous_filter_output;
    uint16_t window[258];
    int8_t high_pitch;
    int8_t low_pitch;
    float wet_mix;
    float dry_mix;
    float range_mix;
    bool configured;
} gp100_pitch_bender_t;

/* H-Pitch is 0..12; L-Pitch is -12..0; Wet/Dry/Range are 0..99. */
bool gp100_pitch_bender_reset(gp100_pitch_bender_t *bender,
                                   int8_t high_pitch,
                                   int8_t low_pitch,
                                   uint8_t wet,
                                   uint8_t dry,
                                   uint8_t range);

bool gp100_pitch_bender_process(gp100_pitch_bender_t *bender,
                                     int8_t high_pitch,
                                     int8_t low_pitch,
                                     uint8_t wet,
                                     uint8_t dry,
                                     uint8_t range,
                                     float *interleaved_stereo,
                                     size_t frame_count,
                                     bool enabled);

#endif
