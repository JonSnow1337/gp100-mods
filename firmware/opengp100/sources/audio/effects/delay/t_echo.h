#ifndef GP100_T_ECHO_H
#define GP100_T_ECHO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_T_ECHO_SAMPLES 176400u

typedef struct gp100_t_echo_biquad {
    float x[2];
    float y[2];
} gp100_t_echo_biquad_t;

typedef struct gp100_t_echo {
    gp100_t_echo_biquad_t color[2][2];
    float raw_delay[2];
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
    float delay_line[2][GP100_T_ECHO_SAMPLES];
} gp100_t_echo_t;

bool gp100_t_echo_reset(gp100_t_echo_t *echo,
                             uint8_t mix, float time, uint8_t feedback,
                             bool tempo_sync, bool trail);

bool gp100_t_echo_process(gp100_t_echo_t *echo,
                               uint8_t mix, float time, uint8_t feedback,
                               bool tempo_sync, bool trail,
                               uint16_t tempo_period_ms,
                               float *interleaved_stereo,
                               size_t frame_count, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
