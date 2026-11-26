#ifndef GP100_PHASER_H
#define GP100_PHASER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PHASER_STAGES 4u

typedef struct gp100_phaser_stage {
    float previous_input;
    float previous_output;
} gp100_phaser_stage_t;

typedef struct gp100_phaser {
    gp100_phaser_stage_t stage[2][GP100_PHASER_STAGES];
    float target_rate_step;
    float current_rate_step;
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_phaser_t;

/* Free Rate is 0.1..10 Hz; with Sync enabled it is an integer beat-division
 * selector in 0..10. */
bool gp100_phaser_reset(gp100_phaser_t *phaser,
                             float rate,
                             bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_phaser_process(gp100_phaser_t *phaser,
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
