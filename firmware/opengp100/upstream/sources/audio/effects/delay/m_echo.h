#ifndef GP100_M_ECHO_H
#define GP100_M_ECHO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_M_ECHO_SAMPLES 176400u
#define GP100_M_ECHO_MOD_SAMPLES 618u

typedef struct gp100_m_echo_biquad {
    float x[2];
    float y[2];
} gp100_m_echo_biquad_t;

typedef struct gp100_m_echo {
    float modulation_line[2][GP100_M_ECHO_MOD_SAMPLES];
    gp100_m_echo_biquad_t delay_filter[2];
    float high_pass_x[2];
    float high_pass_y[2];
    float last_wet[2];
    float modulation_phase[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t delay_write[2];
    size_t modulation_write[2];
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_M_ECHO_SAMPLES];
} gp100_m_echo_t;

bool gp100_m_echo_reset(gp100_m_echo_t *echo,
                             uint8_t mix,
                             float time,
                             uint8_t feedback,
                             bool tempo_sync,
                             bool trail);

bool gp100_m_echo_process(gp100_m_echo_t *echo,
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
