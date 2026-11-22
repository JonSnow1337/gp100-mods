#ifndef GP100_SWEET_DELAY_H
#define GP100_SWEET_DELAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SWEET_DELAY_SAMPLES 176400u
#define GP100_SWEET_DELAY_SHAPER_TAPS 40u

typedef struct gp100_sweet_delay_biquad {
    float previous_input[2];
    float previous_output[2];
} gp100_sweet_delay_biquad_t;

typedef struct gp100_sweet_delay_third_order {
    float previous_input[3];
    float previous_output[3];
} gp100_sweet_delay_third_order_t;

typedef struct gp100_sweet_delay {
    gp100_sweet_delay_biquad_t input_filter;
    gp100_sweet_delay_biquad_t output_filter;
    gp100_sweet_delay_third_order_t write_filter;
    gp100_sweet_delay_third_order_t reconstruction_filter;
    gp100_sweet_delay_biquad_t wet_filter;
    float shaper_history[GP100_SWEET_DELAY_SHAPER_TAPS];
    float shaper_previous_sample;
    float tone_previous_input;
    float tone_previous_output;
    float feedback_previous_input;
    float feedback_previous_output;
    float last_wet;
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t write_index;
    bool tempo_sync;
    bool trail;
    bool ring_active;
    bool configured;
    bool delay_full;
    float delay_line[GP100_SWEET_DELAY_SAMPLES];
} gp100_sweet_delay_t;

/* Mix and FdBk are raw 0..99 controls. Time is truncated to whole milliseconds
 * in 20..4000 or to a beat-division selector in 0..10 when Sync is enabled. */
bool gp100_sweet_delay_reset(gp100_sweet_delay_t *delay,
                                  uint8_t mix,
                                  float time,
                                  uint8_t feedback,
                                  bool tempo_sync,
                                  bool trail);

/* tempo_period_ms is the current quarter-note duration. */
bool gp100_sweet_delay_process(gp100_sweet_delay_t *delay,
                                    uint8_t mix,
                                    float time,
                                    uint8_t feedback,
                                    bool tempo_sync,
                                    bool trail,
                                    uint16_t tempo_period_ms,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
