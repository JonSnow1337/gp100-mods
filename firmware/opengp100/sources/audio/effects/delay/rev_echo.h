#ifndef GP100_REV_ECHO_H
#define GP100_REV_ECHO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_REV_ECHO_SAMPLES 176400u
#define GP100_REV_ECHO_UPPER_START 88200u

typedef struct gp100_rev_echo_filter {
    float x[2];
    float y[2];
} gp100_rev_echo_filter_t;

typedef struct gp100_rev_echo {
    gp100_rev_echo_filter_t upper_filter[2];
    gp100_rev_echo_filter_t reverse_filter[2];
    float feedback_sample[2];
    float last_reverse_sample[2];
    float target_mix;
    float current_mix;
    float target_feedback;
    float current_feedback;
    float target_delay_samples;
    float current_delay_samples;
    size_t input_write[2];
    size_t reverse_read[2];
    size_t upper_write[2];
    size_t upper_read[2];
    size_t lower_written[2];
    size_t upper_written_end[2];
    uint16_t crossfade_q15[2];
    bool tempo_sync;
    bool trail;
    bool rings_active;
    bool configured;
    float delay_line[2][GP100_REV_ECHO_SAMPLES];
} gp100_rev_echo_t;

/* Mix and FdBk are raw 0..99 controls. Free Time is 20..4000 ms; with Sync
 * enabled it is an integer beat-division selector in 0..10. */
bool gp100_rev_echo_reset(gp100_rev_echo_t *echo,
                               uint8_t mix,
                               float time,
                               uint8_t feedback,
                               bool tempo_sync,
                               bool trail);

bool gp100_rev_echo_process(gp100_rev_echo_t *echo,
                                 uint8_t mix,
                                 float time,
                                 uint8_t feedback,
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
