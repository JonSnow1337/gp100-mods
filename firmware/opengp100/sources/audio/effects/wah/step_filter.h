#ifndef GP100_STEP_FILTER_H
#define GP100_STEP_FILTER_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_step_filter {
    float steps[4];
    float phase_counter;
    float target_cycle_increment;
    float current_cycle_increment;
    float selected_step;
    float cutoff_hz;
    gp100_biquad_channel_state_t input_high_pass;
    gp100_biquad_channel_state_t band_pass;
    bool tempo_sync;
    bool configured;
} gp100_step_filter_t;

bool gp100_step_filter_reset(gp100_step_filter_t *filter,
                                  uint8_t step_1,
                                  uint8_t step_2,
                                  uint8_t step_3,
                                  uint8_t step_4,
                                  float rate,
                                  bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_step_filter_process(gp100_step_filter_t *filter,
                                    uint8_t step_1,
                                    uint8_t step_2,
                                    uint8_t step_3,
                                    uint8_t step_4,
                                    float rate,
                                    bool tempo_sync,
                                    uint16_t tempo_period_ms,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#endif
