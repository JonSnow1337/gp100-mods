#ifndef GP100_VIBRATO_H
#define GP100_VIBRATO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_VIBRATO_DELAY_SAMPLES 100u

typedef struct gp100_vibrato_iir2 {
    float x[2];
    float y[2];
} gp100_vibrato_iir2_t;

typedef struct gp100_vibrato_iir3 {
    float x[3];
    float y[3];
} gp100_vibrato_iir3_t;

typedef struct gp100_vibrato_iir4 {
    float x[4];
    float y[4];
} gp100_vibrato_iir4_t;

typedef struct gp100_vibrato {
    float delay[2][GP100_VIBRATO_DELAY_SAMPLES];
    size_t write_index[2];
    gp100_vibrato_iir2_t input_filter[2];
    gp100_vibrato_iir4_t delay_filter[2];
    gp100_vibrato_iir2_t shaping_filter[2];
    gp100_vibrato_iir3_t output_filter[2];
    float target_depth;
    float current_depth;
    float target_rate_step;
    float current_rate_step;
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_vibrato_t;

/* Depth is a raw 0..99 control. Free Rate is 0.1..10 Hz; with Sync enabled
 * it is an integer beat-division selector in 0..10. */
bool gp100_vibrato_reset(gp100_vibrato_t *vibrato,
                              uint8_t depth,
                              float rate,
                              bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_vibrato_process(gp100_vibrato_t *vibrato,
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
