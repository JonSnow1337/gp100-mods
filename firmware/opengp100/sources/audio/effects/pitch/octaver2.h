#ifndef GP100_OCTAVER2_H
#define GP100_OCTAVER2_H

#include "audio/dsp/audio_biquad.h"

#include <stdint.h>

typedef struct gp100_octaver2 {
    gp100_biquad_channel_state_t tracker[2];
    float oct1[3];
    float oct2[3];
    float input_bias[2];
    float tracker_bias;
    float detector_reference;
    float positive_peak;
    float negative_peak;
    float switch_feedback;
    float oct1_bias;
    float output_bias[2];
    float gate;
    float levels[3];
    uint16_t samples_since_edge;
    uint8_t divider;
    bool armed;
    bool active;
} gp100_octaver2_t;

void gp100_octaver2_reset(gp100_octaver2_t *state, const float levels[3]);
void gp100_octaver2_process(gp100_octaver2_t *state, const float controls[4],
                         float *stereo, size_t frames, bool enabled);

#endif
