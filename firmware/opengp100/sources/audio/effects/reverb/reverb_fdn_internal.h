#ifndef GP100_REVERB_FDN_INTERNAL_H
#define GP100_REVERB_FDN_INTERNAL_H

#include "room_reverb.h"

typedef struct gp100_reverb_fdn_profile {
    float delay_samples[4];
    float decay_scale;
    float decay_offset;
    float shelf_a_exponent;
    float shelf_b_exponent;
    float low_warp_angle;
    float high_warp_angle;
    float output_angle;
    float left_output_gain;
    float predelay_mod_depth;
    float predelay_mod_rate;
    bool dynamic_damping;
    bool dynamic_tone;
    bool full_matrix;
    bool filter_output_sum;
    bool dynamic_shelf_filter;
    bool shimmer_input;
    bool skip_output_filter;
} gp100_reverb_fdn_profile_t;

bool gp100_reverb_fdn_reset(
    gp100_room_reverb_t *reverb,
    const gp100_reverb_fdn_profile_t *profile,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    uint8_t damping,
    int8_t lo_end,
    int8_t hi_end,
    bool trail);

bool gp100_reverb_fdn_process(
    gp100_room_reverb_t *reverb,
    const gp100_reverb_fdn_profile_t *profile,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    uint8_t damping,
    int8_t lo_end,
    int8_t hi_end,
    bool trail,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#endif
