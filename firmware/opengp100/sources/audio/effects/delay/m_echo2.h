#ifndef GP100_M_ECHO2_H
#define GP100_M_ECHO2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_M_ECHO2_SAMPLES 176400u
#define GP100_M_ECHO2_EARLY_SAMPLES 352u
#define GP100_M_ECHO2_MOD_SAMPLES 133u

typedef struct gp100_m_echo2_filter {
    float x[2];
    float y[2];
} gp100_m_echo2_filter_t;

typedef struct gp100_m_echo2 {
    float early_line[GP100_M_ECHO2_EARLY_SAMPLES];
    float mod_line[GP100_M_ECHO2_MOD_SAMPLES];
    gp100_m_echo2_filter_t tap_filter[4];
    gp100_m_echo2_filter_t main_filter;
    float tap_previous[4];
    float filter_b[3];
    float filter_a[2];
    float main_output;
    float early_output;
    float modulation_counter;
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    float tone;
    size_t tap_write;
    size_t main_write;
    size_t early_write;
    size_t mod_write;
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool tap_full;
    bool main_full;
    float tap_line[GP100_M_ECHO2_SAMPLES];
    float main_line[GP100_M_ECHO2_SAMPLES];
} gp100_m_echo2_t;

bool gp100_m_echo2_reset(gp100_m_echo2_t *echo,
                              uint8_t mix,
                              float time,
                              uint8_t feedback,
                              uint8_t tone,
                              bool tempo_sync,
                              bool trail);

bool gp100_m_echo2_process(gp100_m_echo2_t *echo,
                                uint8_t mix,
                                float time,
                                uint8_t feedback,
                                uint8_t tone,
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
