#ifndef GP100_HAMMY_H
#define GP100_HAMMY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_HAMMY_DELAY_SAMPLES 4000u
#define GP100_HAMMY_CROSSING_CAPACITY 50u

typedef struct gp100_hammy_filter {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
} gp100_hammy_filter_t;

typedef struct gp100_hammy {
    float delay[GP100_HAMMY_DELAY_SAMPLES];
    size_t write_index;
    float read_position;
    float splice_old_position;
    float splice_phase;
    float splice_span;
    bool splice_active;
    uint32_t splice_count;
    float detected_period;
    float raw_period;
    float last_raw_period;
    float previous_raw_period;
    float crossings[GP100_HAMMY_CROSSING_CAPACITY];
    size_t crossing_count;
    int history_position;
    int crossing_anchor;
    int pending_crossing_anchor;
    uint32_t detector_sample_count;
    float recent_peak;
    float earlier_peak;
    bool peak_scan_pending;
    bool extremum_armed;
    uint32_t period_search_count;
    float transient_envelope[100];
    size_t transient_envelope_position;
    uint32_t transient_hold;
    int hard_transient_marker;
    bool hard_transient;
    bool soft_transient;
    gp100_hammy_filter_t input_high_pass;
    gp100_hammy_filter_t anti_alias_low_pass;
    float anti_alias_ratio;
    uint8_t range;
    bool harmony;
    float volume_target;
    float position_target;
    float volume;
    float harmony_mix;
    float position;
    float semitones;
    float ratio;
    bool configured;
} gp100_hammy_t;

/* Range is 0..5, Harmony is Off/On, and VOL/Position are 0..99. */
bool gp100_hammy_reset(gp100_hammy_t *hammy,
                            uint8_t range,
                            bool harmony,
                            uint8_t volume,
                            uint8_t position);

bool gp100_hammy_process(gp100_hammy_t *hammy,
                              uint8_t range,
                              bool harmony,
                              uint8_t volume,
                              uint8_t position,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled);

#endif
