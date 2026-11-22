#ifndef GP100_VIN_RACK_H
#define GP100_VIN_RACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_VIN_RACK_SAMPLES 176400u
#define GP100_VIN_RACK_MOD_SAMPLES 257u

typedef struct gp100_vin_rack_filter {
    float x[2];
    float y[2];
} gp100_vin_rack_filter_t;

typedef struct gp100_vin_rack {
    float mod_line[2][GP100_VIN_RACK_MOD_SAMPLES];
    gp100_vin_rack_filter_t tone_filter[2];
    float feedback_sample[2];
    float held_sample[2];
    float mod_phase[2];
    float mod_direction[2];
    float tone_b[3];
    float tone_a[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    float current_mod;
    float target_tone;
    float current_tone;
    size_t write_index[2];
    size_t mod_write_index[2];
    size_t hold_counter;
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    bool delay_full[2];
    float delay_line[2][GP100_VIN_RACK_SAMPLES];
} gp100_vin_rack_t;

bool gp100_vin_rack_reset(gp100_vin_rack_t *echo,
                               uint8_t mix,
                               uint8_t feedback,
                               float time,
                               uint8_t modulation,
                               uint8_t tone,
                               bool tempo_sync,
                               bool trail);

bool gp100_vin_rack_process(gp100_vin_rack_t *echo,
                                 uint8_t mix,
                                 uint8_t feedback,
                                 float time,
                                 uint8_t modulation,
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
