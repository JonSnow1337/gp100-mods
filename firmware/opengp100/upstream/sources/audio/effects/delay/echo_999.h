#ifndef GP100_ECHO_999_H
#define GP100_ECHO_999_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_ECHO_999_SAMPLES 176400u

typedef struct gp100_echo_999_filter {
    float x[3];
    float y[3];
} gp100_echo_999_filter_t;

typedef struct gp100_echo_999 {
    gp100_echo_999_filter_t input_filter;
    gp100_echo_999_filter_t delay_input_filter;
    gp100_echo_999_filter_t time_color[3];
    gp100_echo_999_filter_t delay_output_filter;
    gp100_echo_999_filter_t delay_limiter_filter;
    gp100_echo_999_filter_t output_filter;
    float last_delay;
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t write_index;
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full;
    float delay_line[GP100_ECHO_999_SAMPLES];
} gp100_echo_999_t;

bool gp100_echo_999_reset(gp100_echo_999_t *echo,
                               uint8_t mix, float time, uint8_t feedback,
                               bool tempo_sync, bool trail);
bool gp100_echo_999_process(gp100_echo_999_t *echo,
                                 uint8_t mix, float time, uint8_t feedback,
                                 bool tempo_sync, bool trail,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count, bool enabled);

#endif
