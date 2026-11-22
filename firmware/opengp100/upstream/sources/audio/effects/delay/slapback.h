#ifndef GP100_SLAPBACK_H
#define GP100_SLAPBACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_SLAPBACK_SAMPLES 13230u
#define GP100_SLAPBACK_PRE_SAMPLES 80u

typedef struct gp100_slapback_filter {
    float x[2];
    float y[2];
} gp100_slapback_filter_t;

typedef struct gp100_slapback {
    float pre_delay[2][GP100_SLAPBACK_PRE_SAMPLES];
    gp100_slapback_filter_t color[2];
    float last_wet[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t write_index[2];
    size_t pre_write[2];
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_SLAPBACK_SAMPLES];
} gp100_slapback_t;

bool gp100_slapback_reset(gp100_slapback_t *echo,
                               uint8_t mix, float time_ms,
                               uint8_t feedback, bool trail);
bool gp100_slapback_process(gp100_slapback_t *echo,
                                 uint8_t mix, float time_ms,
                                 uint8_t feedback, bool trail,
                                 float *interleaved_stereo,
                                 size_t frame_count, bool enabled);

#endif
