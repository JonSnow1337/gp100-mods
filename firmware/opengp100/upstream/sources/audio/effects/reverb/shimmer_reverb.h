#ifndef GP100_SHIMMER_REVERB_H
#define GP100_SHIMMER_REVERB_H

#include "room_reverb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gp100_room_reverb_t gp100_shimmer_reverb_t;

bool gp100_shimmer_reverb_reset(
    gp100_shimmer_reverb_t *reverb,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    int8_t lo_end,
    int8_t hi_end,
    bool trail);

bool gp100_shimmer_reverb_process(
    gp100_shimmer_reverb_t *reverb,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    int8_t lo_end,
    int8_t hi_end,
    bool trail,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
