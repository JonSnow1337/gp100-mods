#ifndef GP100_DEEP_SEA_REVERB_H
#define GP100_DEEP_SEA_REVERB_H

#include "room_reverb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gp100_room_reverb_t gp100_deep_sea_reverb_t;

bool gp100_deep_sea_reverb_reset(gp100_deep_sea_reverb_t *reverb,
                                      uint8_t mix,
                                      uint8_t decay,
                                      bool trail);

bool gp100_deep_sea_reverb_process(
    gp100_deep_sea_reverb_t *reverb,
    uint8_t mix,
    uint8_t decay,
    bool trail,
    float *interleaved_stereo,
    size_t frame_count,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
