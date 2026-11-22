#ifndef GP100_PING_PONG_H
#define GP100_PING_PONG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PING_PONG_SAMPLES 176400u

typedef struct gp100_ping_pong_coefficients {
    float b[3];
    float a[3];
} gp100_ping_pong_coefficients_t;

typedef struct gp100_ping_pong_filter {
    float x[2];
    float y[2];
} gp100_ping_pong_filter_t;

typedef struct gp100_ping_pong {
    gp100_ping_pong_filter_t color[2][2];
    gp100_ping_pong_coefficients_t color_coefficients[2];
    float loop_output[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t write_index[2];
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_PING_PONG_SAMPLES];
} gp100_ping_pong_t;

bool gp100_ping_pong_reset(gp100_ping_pong_t *echo,
                                uint8_t mix,
                                uint8_t feedback,
                                float time,
                                bool tempo_sync,
                                bool trail);

bool gp100_ping_pong_process(gp100_ping_pong_t *echo,
                                  uint8_t mix,
                                  uint8_t feedback,
                                  float time,
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
