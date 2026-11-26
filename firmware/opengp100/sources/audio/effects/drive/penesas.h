#ifndef GP100_PENESAS_H
#define GP100_PENESAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_penesas_filter {
    float x1, x2;
    float y1, y2;
} gp100_penesas_filter_t;

typedef struct gp100_penesas_filter_coefficients {
    float b0, b1, b2;
    float a1, a2;
} gp100_penesas_filter_coefficients_t;

typedef struct gp100_penesas_fourth_order_coefficients {
    double b[5];
    double a[5];
} gp100_penesas_fourth_order_coefficients_t;

typedef struct gp100_penesas_fourth_order {
    double x[4];
    double y[4];
} gp100_penesas_fourth_order_t;

typedef struct gp100_penesas {
    float oversample_history[2][40];
    float previous_oversample_input[2];
    gp100_penesas_filter_t input_filter[5];
    gp100_penesas_fourth_order_coefficients_t feedback_coefficients[2];
    gp100_penesas_fourth_order_t feedback_filter[2];
    double feedback_return;
    gp100_penesas_filter_coefficients_t tone_filter_coefficients;
    gp100_penesas_filter_t tone_filter;
    gp100_penesas_filter_t output_filter[11];
    float gain;
    float gain_target;
    float tone;
    float output_level;
    bool configured;
} gp100_penesas_t;

/* Gain, Tone, and VOL are all 0..99. */
bool gp100_penesas_reset(gp100_penesas_t *effect,
                              uint8_t gain,
                              uint8_t tone,
                              uint8_t volume);

bool gp100_penesas_process(gp100_penesas_t *effect,
                                uint8_t gain,
                                uint8_t tone,
                                uint8_t volume,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled);

#endif
