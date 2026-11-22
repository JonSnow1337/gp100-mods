#ifndef GP100_OPTO_TREM_H
#define GP100_OPTO_TREM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_opto_trem {
    float target_depth;
    float current_depth;
    float target_rate_step;
    float current_rate_step;
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_opto_trem_t;

/* Depth is a raw 0..99 control. Free Rate is 0.1..10 Hz; with Sync enabled
 * it is an integer beat-division selector in 0..10. */
bool gp100_opto_trem_reset(gp100_opto_trem_t *tremolo,
                                uint8_t depth,
                                float rate,
                                bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_opto_trem_process(gp100_opto_trem_t *tremolo,
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
