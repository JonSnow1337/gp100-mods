#ifndef GP100_TOUCH_WAH_H
#define GP100_TOUCH_WAH_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_touch_wah {
    float sense;
    float range;
    float resonance;
    float mix;
    float envelope;
    float envelope_position;
    float cutoff_hz;
    gp100_biquad_coefficients_t filter_coefficients;
    gp100_biquad_channel_state_t filter_state;
    uint8_t mode;
    bool configured;
} gp100_touch_wah_t;

/* Sense, Range, Q, and Mix are 0..99; Mode is 0..3. */
bool gp100_touch_wah_reset(gp100_touch_wah_t *wah,
                                uint8_t sense,
                                uint8_t range,
                                uint8_t resonance,
                                uint8_t mix,
                                uint8_t mode);

bool gp100_touch_wah_process(gp100_touch_wah_t *wah,
                                  uint8_t sense,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t mix,
                                  uint8_t mode,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled);

#ifdef __cplusplus
}
#endif

#endif
