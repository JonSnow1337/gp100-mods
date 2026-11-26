#ifndef GP100_VINTAGE_WAH_H
#define GP100_VINTAGE_WAH_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_vintage_wah {
    float range;
    float target_resonance;
    float current_resonance;
    float target_output_level;
    float current_output_level;
    float sweep_position;
    float cutoff_hz;
    gp100_biquad_coefficients_t input_filter_coefficients;
    gp100_biquad_channel_state_t input_filter_state;
    gp100_biquad_coefficients_t wah_filter_coefficients;
    gp100_biquad_channel_state_t wah_filter_state;
    bool configured;
} gp100_vintage_wah_t;

/* Range, Q, and VOL are raw 0..99 controls. */
bool gp100_vintage_wah_reset(gp100_vintage_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level);

bool gp100_vintage_wah_process(gp100_vintage_wah_t *wah,
                                    uint8_t range,
                                    uint8_t resonance,
                                    uint8_t output_level,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
