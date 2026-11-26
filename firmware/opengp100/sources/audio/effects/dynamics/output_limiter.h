#ifndef GP100_OUTPUT_LIMITER_H
#define GP100_OUTPUT_LIMITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_LIMITER_SAMPLE_RATE 44100u
#define GP100_LIMITER_LOOKAHEAD_FRAMES 88u /* 2 ms at 44.1 kHz */
#define GP100_LIMITER_WINDOW_FRAMES (GP100_LIMITER_LOOKAHEAD_FRAMES + 1u)
#define GP100_LIMITER_MIN_THRESHOLD_DB (-60)
#define GP100_LIMITER_MAX_THRESHOLD_DB 12
#define GP100_LIMITER_MIN_CEILING_DB (-60)
#define GP100_LIMITER_MAX_CEILING_DB 0
#define GP100_LIMITER_MIN_RELEASE_DB_PER_SEC 1
#define GP100_LIMITER_MAX_RELEASE_DB_PER_SEC 120
#define GP100_LIMITER_DEFAULT_RELEASE_DB_PER_SEC 15

typedef struct gp100_output_limiter_config {
    float threshold;
    float ceiling;
    float makeup;
    float release_multiplier;
    float full_scale;
    int16_t threshold_db;
    int16_t ceiling_db;
    int16_t release_db_per_sec;
    bool enabled;
} gp100_output_limiter_config_t;

typedef struct gp100_output_limiter_peak {
    float level;
    uint32_t frame;
} gp100_output_limiter_peak_t;

typedef struct gp100_output_limiter {
    float delay[GP100_LIMITER_WINDOW_FRAMES][2];
    float gain_history[GP100_LIMITER_WINDOW_FRAMES];
    gp100_output_limiter_peak_t peaks[GP100_LIMITER_WINDOW_FRAMES + 1u];
    double gain_sum;
    float gain;
    uint32_t frame;
    unsigned position;
    unsigned wet_frames;
    unsigned peak_head;
    unsigned peak_tail;
    bool active;
    bool bypass_history;
} gp100_output_limiter_t;

/* Prepare coefficients outside the audio callback. Published at a block boundary. */
void gp100_output_limiter_configure(gp100_output_limiter_config_t *config,
                                   bool enabled, int16_t threshold_db,
                                   int16_t ceiling_db, int16_t release_db_per_sec,
                                   float full_scale);

/* The state starts zero-initialized. Samples are interleaved stereo in the
 * same amplitude units as the configured full scale. */
void gp100_output_limiter_process(gp100_output_limiter_t *limiter,
                                 const gp100_output_limiter_config_t *config,
                                 float *samples, size_t frames);

#endif
