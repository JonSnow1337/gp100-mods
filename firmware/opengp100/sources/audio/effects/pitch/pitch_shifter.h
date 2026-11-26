#ifndef GP100_PITCH_SHIFTER_H
#define GP100_PITCH_SHIFTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_PITCH_DELAY_SAMPLES 4096u

typedef struct gp100_pitch_voice {
    float delay[GP100_PITCH_DELAY_SAMPLES];
    uint32_t phase[2];
    size_t write_index;
    float previous_filter_input;
    float previous_filter_output;
} gp100_pitch_voice_t;

typedef struct gp100_pitch_shifter {
    gp100_pitch_voice_t high;
    gp100_pitch_voice_t low;
    uint16_t window[258];
    int8_t high_pitch;
    int8_t low_pitch;
    float dry_mix;
    float high_mix;
    float low_mix;
    bool configured;
} gp100_pitch_shifter_t;

/* H-Pitch is 0..24; L-Pitch is -24..0; Dry/H-Vol/L-Vol are 0..99. */
bool gp100_pitch_shifter_reset(gp100_pitch_shifter_t *shifter,
                                    int8_t high_pitch,
                                    int8_t low_pitch,
                                    uint8_t dry,
                                    uint8_t high_level,
                                    uint8_t low_level);

bool gp100_pitch_shifter_process(gp100_pitch_shifter_t *shifter,
                                      int8_t high_pitch,
                                      int8_t low_pitch,
                                      uint8_t dry,
                                      uint8_t high_level,
                                      uint8_t low_level,
                                      float *interleaved_stereo,
                                      size_t frame_count,
                                      bool enabled);

#endif
