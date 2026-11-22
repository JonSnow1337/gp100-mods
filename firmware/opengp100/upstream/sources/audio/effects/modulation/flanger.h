#ifndef GP100_FLANGER_H
#define GP100_FLANGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_FLANGER_DELAY_SAMPLES 250u

typedef struct gp100_flanger {
    float delay[2][GP100_FLANGER_DELAY_SAMPLES];
    size_t write_index[2];
    float last_tap[2];
    float target_depth;
    float current_depth;
    float target_rate_step;
    float current_rate_step;
    float target_feedback;
    float current_feedback;
    float target_bias;
    float current_bias;
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_flanger_t;

/* Depth, FdBk, and Bias are raw 0..99 controls. Free Rate is 0.1..10 Hz;
 * with Sync enabled it is an integer beat-division selector in 0..10. */
bool gp100_flanger_reset(gp100_flanger_t *flanger,
                              uint8_t depth,
                              float rate,
                              uint8_t feedback,
                              uint8_t bias,
                              bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_flanger_process(gp100_flanger_t *flanger,
                                uint8_t depth,
                                float rate,
                                uint8_t feedback,
                                uint8_t bias,
                                bool tempo_sync,
                                uint16_t tempo_period_ms,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled);

#ifdef __cplusplus
}
#endif

#endif
