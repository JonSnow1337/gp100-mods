#ifndef GP100_SINE_TREM_H
#define GP100_SINE_TREM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_sine_trem_filter {
    float previous_input[2];
    float previous_output[2];
} gp100_sine_trem_filter_t;

typedef struct gp100_sine_trem {
    gp100_sine_trem_filter_t output_filter[2];
    float target_depth;
    float current_depth;
    float target_rate_step;
    float current_rate_step;
    float target_level;
    float current_level;
    float lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_sine_trem_t;

/* Depth and Level are raw 0..99 controls. Free Rate is 0.1..10 Hz; with
 * Sync enabled it is an integer beat-division selector in 0..10. */
bool gp100_sine_trem_reset(gp100_sine_trem_t *tremolo,
                                uint8_t depth,
                                float rate,
                                uint8_t level,
                                bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_sine_trem_process(gp100_sine_trem_t *tremolo,
                                  uint8_t depth,
                                  float rate,
                                  uint8_t level,
                                  bool tempo_sync,
                                  uint16_t tempo_period_ms,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled);

#ifdef __cplusplus
}
#endif

#endif
