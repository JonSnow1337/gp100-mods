#ifndef GP100_DETUNE_H
#define GP100_DETUNE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DETUNE_DELAY_SAMPLES 4096u

typedef struct gp100_detune_channel {
    float delay[GP100_DETUNE_DELAY_SAMPLES];
    uint32_t phase[2];
    size_t write_index;
    float previous_filter_input;
    float previous_filter_output;
} gp100_detune_channel_t;

typedef struct gp100_detune {
    gp100_detune_channel_t channel[2];
    uint16_t window[258];
    int8_t cents;
    float target_wet;
    float current_wet;
    float target_dry;
    float current_dry;
    bool configured;
} gp100_detune_t;

/* Detune is -50..+50 cents; Wet and Dry are raw 0..99 controls. */
bool gp100_detune_reset(gp100_detune_t *detune,
                             int8_t cents,
                             uint8_t wet,
                             uint8_t dry);

bool gp100_detune_process(gp100_detune_t *detune,
                               int8_t cents,
                               uint8_t wet,
                               uint8_t dry,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled);

#ifdef __cplusplus
}
#endif

#endif
