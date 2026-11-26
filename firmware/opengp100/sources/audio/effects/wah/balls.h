#ifndef GP100_BALLS_H
#define GP100_BALLS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct gp100_balls_filter {
    float input_cap_history;
    float feedback_cap_history;
} gp100_balls_filter_t;

typedef struct gp100_balls {
    gp100_balls_filter_t filter[2];
    float decimator[15];
    float previous_sample;
    float input_cap;
    float buffer_cap;
    float detector_cap;
    float envelope;
    float clean_cap;
    float output_cap;
    float response;
    bool enabled;
} gp100_balls_t;

void gp100_balls_reset(gp100_balls_t *effect, float response);
void gp100_balls_process(gp100_balls_t *effect, float response,
                            float *audio, size_t frames, bool enabled);

#endif
