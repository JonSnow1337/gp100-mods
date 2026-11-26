#ifndef GP100_BOOSTER_H
#define GP100_BOOSTER_H

#include "audio/dsp/audio_biquad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_BOOST14_STAGE_COUNT 4u
#define GP100_BOOST_OVERSAMPLE 4u
#define GP100_BOOST_FIR_TAPS 40u

typedef struct gp100_first_order_coefficients {
    float b0;
    float b1;
    float a1;
} gp100_first_order_coefficients_t;

typedef struct gp100_first_order_state {
    float x1;
    float y1;
} gp100_first_order_state_t;

typedef struct gp100_boost {
    float drive;
    float input_numerator;
    float input_denominator;
    float oversample_previous;
    float fir_history[GP100_BOOST_FIR_TAPS];
    gp100_first_order_coefficients_t bright_filter;
    gp100_first_order_coefficients_t output_filter;
    gp100_first_order_state_t bright_state;
    gp100_first_order_state_t output_state;
    uint8_t gain;
    bool bright;
    bool configured;
} gp100_boost_t;

typedef struct gp100_boost14 {
    gp100_biquad_coefficients_t
        coefficients[GP100_BOOST14_STAGE_COUNT];
    gp100_biquad_channel_state_t
        stage[GP100_BOOST14_STAGE_COUNT];
    float target_gain;
    float current_gain;
    bool configured;
} gp100_boost14_t;

/* Gain is 0..99; Bright selects the recovered two-position output filter. */
bool gp100_boost_reset(gp100_boost_t *booster,
                            uint8_t gain,
                            bool bright);

bool gp100_boost_process(gp100_boost_t *booster,
                              uint8_t gain,
                              bool bright,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled);

/* Gain is the catalog's integral 0..99 parameter. */
bool gp100_boost14_reset(gp100_boost14_t *booster,
                              uint8_t gain);

/*
 * Processes the left input through four fixed biquads and copies it to both
 * channels. Bypass preserves audio, filter history, and the gain ramp.
 */
bool gp100_boost14_process(gp100_boost14_t *booster,
                                uint8_t gain,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled);

#ifdef __cplusplus
}
#endif

#endif
