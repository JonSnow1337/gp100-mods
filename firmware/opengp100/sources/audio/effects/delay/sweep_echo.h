#ifndef GP100_SWEEP_ECHO_H
#define GP100_SWEEP_ECHO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SWEEP_ECHO_SAMPLES 176400u

typedef struct gp100_sweep_echo_filter {
    float x[2];
    float y[2];
} gp100_sweep_echo_filter_t;

typedef struct gp100_sweep_echo {
    gp100_sweep_echo_filter_t sweep_filter[2];
    float feedback_sample[2];
    float sweep_phase[2];
    float sweep_direction[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    float target_depth;
    float current_depth;
    float target_sweep_step;
    float current_sweep_step;
    size_t write_index[2];
    bool time_sync;
    bool sweep_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_SWEEP_ECHO_SAMPLES];
} gp100_sweep_echo_t;

bool gp100_sweep_echo_reset(gp100_sweep_echo_t *echo,
                                 uint8_t mix,
                                 uint8_t feedback,
                                 float time,
                                 uint8_t sweep_depth,
                                 float sweep_rate,
                                 bool time_sync,
                                 bool sweep_sync,
                                 bool trail);

bool gp100_sweep_echo_process(gp100_sweep_echo_t *echo,
                                   uint8_t mix,
                                   uint8_t feedback,
                                   float time,
                                   uint8_t sweep_depth,
                                   float sweep_rate,
                                   bool time_sync,
                                   bool sweep_sync,
                                   bool trail,
                                   uint16_t tempo_period_ms,
                                   float *interleaved_stereo,
                                   size_t frame_count,
                                   bool enabled);

#ifdef __cplusplus
}
#endif

#endif
