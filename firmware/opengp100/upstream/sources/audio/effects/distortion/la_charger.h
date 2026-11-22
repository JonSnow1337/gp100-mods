#ifndef GP100_LA_CHARGER_H
#define GP100_LA_CHARGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_la_charger_filter {
    float x1, x2;
    float y1, y2;
} gp100_la_charger_filter_t;

typedef struct gp100_la_charger {
    gp100_la_charger_filter_t gain_filter[2];
    gp100_la_charger_filter_t post_filter[3];
    gp100_la_charger_filter_t tone_filter;
    float oversample_history[40];
    float previous_clip_input;
    bool configured;
} gp100_la_charger_t;

bool gp100_la_charger_reset(gp100_la_charger_t *effect,
                                 uint8_t gain, uint8_t tone, uint8_t volume);
bool gp100_la_charger_process(gp100_la_charger_t *effect,
                                   uint8_t gain, uint8_t tone,
                                   uint8_t volume, float *interleaved_stereo,
                                   size_t frame_count, bool enabled);

#endif
