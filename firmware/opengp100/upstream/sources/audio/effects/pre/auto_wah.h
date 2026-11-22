#ifndef GP100_AUTO_WAH_H
#define GP100_AUTO_WAH_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_auto_wah {
    float target_depth;
    float current_depth;
    float target_phase_step;
    float current_phase_step;
    float target_output_level;
    float current_output_level;
    float phase;
    float phase_direction;
    float low_hz;
    float high_hz;
    float resonance;
    float cutoff_hz;
    gp100_biquad_coefficients_t filter_coefficients;
    gp100_biquad_channel_state_t filter_state;
    uint8_t rate;
    bool tempo_sync;
    bool configured;
} gp100_auto_wah_t;

/*
 * Depth, Rate, VOL, Low, Q, and High are raw 0..99 controls. When Sync is
 * enabled, Rate is a beat-division selector limited to 0..10. The factory
 * truncates Rate to an integer before indexing either table.
 */
bool gp100_auto_wah_reset(gp100_auto_wah_t *wah,
                               uint8_t depth,
                               float rate,
                               uint8_t output_level,
                               uint8_t low,
                               uint8_t resonance,
                               uint8_t high,
                               bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration, not beats per minute. */
bool gp100_auto_wah_process(gp100_auto_wah_t *wah,
                                 uint8_t depth,
                                 float rate,
                                 uint8_t output_level,
                                 uint8_t low,
                                 uint8_t resonance,
                                 uint8_t high,
                                 bool tempo_sync,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled);

#ifdef __cplusplus
}
#endif

#endif
