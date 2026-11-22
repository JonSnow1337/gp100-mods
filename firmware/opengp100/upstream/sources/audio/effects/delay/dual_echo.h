#ifndef GP100_DUAL_ECHO_H
#define GP100_DUAL_ECHO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DUAL_ECHO_SAMPLES 176400u

typedef struct gp100_dual_echo {
    float last_wet[2];
    float target_mix[2];
    float current_mix[2];
    float target_feedback[2];
    float current_feedback[2];
    float target_delay_samples[2];
    float current_delay_samples[2];
    size_t write_index[2];
    bool tempo_sync[2];
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_DUAL_ECHO_SAMPLES];
} gp100_dual_echo_t;

bool gp100_dual_echo_reset(gp100_dual_echo_t *echo,
                                uint8_t mix_a,
                                uint8_t feedback_a,
                                float time_a,
                                uint8_t mix_b,
                                uint8_t feedback_b,
                                float time_b,
                                bool sync_a,
                                bool sync_b,
                                bool trail);

bool gp100_dual_echo_process(gp100_dual_echo_t *echo,
                                  uint8_t mix_a,
                                  uint8_t feedback_a,
                                  float time_a,
                                  uint8_t mix_b,
                                  uint8_t feedback_b,
                                  float time_b,
                                  bool sync_a,
                                  bool sync_b,
                                  bool trail,
                                  uint16_t tempo_period_ms,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled);

#ifdef __cplusplus
}
#endif

#endif
