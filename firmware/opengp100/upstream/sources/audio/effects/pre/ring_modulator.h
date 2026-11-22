#ifndef GP100_RING_MODULATOR_H
#define GP100_RING_MODULATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_ring_modulator {
    float oscillator_samples;
    float normalized_frequency;
    float mix;
    float previous_filtered;
    uint8_t frequency;
    int8_t fine;
    uint8_t tone;
    bool configured;
} gp100_ring_modulator_t;

/* Mix/Freq/Tone are 0..99; Fine is -50..+50. */
bool gp100_ring_modulator_reset(gp100_ring_modulator_t *modulator,
                                     uint8_t mix,
                                     uint8_t frequency,
                                     int8_t fine,
                                     uint8_t tone);

bool gp100_ring_modulator_process(gp100_ring_modulator_t *modulator,
                                       uint8_t mix,
                                       uint8_t frequency,
                                       int8_t fine,
                                       uint8_t tone,
                                       float *interleaved_stereo,
                                       size_t frame_count,
                                       bool enabled);

#endif
