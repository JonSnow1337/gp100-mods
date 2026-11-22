#ifndef GP100_VIBE_H
#define GP100_VIBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_VIBE_ALL_PASS_STAGES 3u

typedef struct gp100_vibe_biquad {
    float x[2];
    float y[2];
} gp100_vibe_biquad_t;

typedef struct gp100_vibe_all_pass {
    float previous_input;
    float previous_output;
} gp100_vibe_all_pass_t;

typedef struct gp100_vibe {
    gp100_vibe_biquad_t input_filter[2];
    gp100_vibe_all_pass_t stage[2][GP100_VIBE_ALL_PASS_STAGES];
    float last_wet[2];
    float target_depth;
    float current_depth;
    float target_rate_step;
    float current_rate_step;
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_vibe_t;

/* Depth is a raw 0..99 control. Free Rate is 0.1..10 Hz; with Sync enabled
 * it is an integer beat-division selector in 0..10. */
bool gp100_vibe_reset(gp100_vibe_t *vibe,
                           uint8_t depth,
                           float rate,
                           bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_vibe_process(gp100_vibe_t *vibe,
                             uint8_t depth,
                             float rate,
                             bool tempo_sync,
                             uint16_t tempo_period_ms,
                             float *interleaved_stereo,
                             size_t frame_count,
                             bool enabled);

#ifdef __cplusplus
}
#endif

#endif
