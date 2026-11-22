#ifndef GP100_G_CHORUS_H
#define GP100_G_CHORUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_G_CHORUS_DELAY_SAMPLES 300u

typedef struct gp100_g_chorus_iir1 {
    float x1;
    float y1;
} gp100_g_chorus_iir1_t;

typedef struct gp100_g_chorus_iir3 {
    float x[3];
    float y[3];
} gp100_g_chorus_iir3_t;

typedef struct gp100_g_chorus_iir4 {
    float x[4];
    float y[4];
} gp100_g_chorus_iir4_t;

typedef struct gp100_g_chorus {
    float delay[2][GP100_G_CHORUS_DELAY_SAMPLES];
    size_t write_index[2];
    gp100_g_chorus_iir1_t input_filter;
    gp100_g_chorus_iir3_t feedback_filter[2];
    gp100_g_chorus_iir1_t depth_filter[2];
    gp100_g_chorus_iir4_t output_filter[2];
    float current_depth;
    float target_depth;
    float current_rate_step;
    float target_rate_step;
    float current_tone;
    float target_tone;
    float feedback_output[2];
    uint32_t lfo_sample;
    uint8_t rate_selector;
    bool tempo_sync;
    bool configured;
} gp100_g_chorus_t;

/* Depth and Tone are raw 0..99 controls. Free Rate is 0.1..10 Hz; when
 * Sync is enabled it is an integer beat-division selector in 0..10. */
bool gp100_g_chorus_reset(gp100_g_chorus_t *chorus,
                               uint8_t depth,
                               float rate,
                               uint8_t tone,
                               bool tempo_sync);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_g_chorus_process(gp100_g_chorus_t *chorus,
                                 uint8_t depth,
                                 float rate,
                                 uint8_t tone,
                                 bool tempo_sync,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled);

#ifdef __cplusplus
}
#endif

#endif
