#ifndef GP100_OCTAVER_H
#define GP100_OCTAVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_OCTAVER_DELAY_SAMPLES 4096u

typedef struct gp100_octaver {
    float delay[GP100_OCTAVER_DELAY_SAMPLES];
    uint16_t window[258];
    uint32_t low_phase[3];
    uint32_t high_phase[2];
    size_t write_index;
    float low_mix;
    float high_mix;
    float dry_mix;
    float previous_high_input;
    float previous_high_output;
    bool configured;
} gp100_octaver_t;

bool gp100_octaver_reset(gp100_octaver_t *octaver,
                              uint8_t low_octave,
                              uint8_t high_octave,
                              uint8_t dry);

bool gp100_octaver_process(gp100_octaver_t *octaver,
                                uint8_t low_octave,
                                uint8_t high_octave,
                                uint8_t dry,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled);

#endif
