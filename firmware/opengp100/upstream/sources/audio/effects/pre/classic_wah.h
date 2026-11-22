#ifndef GP100_CLASSIC_WAH_H
#define GP100_CLASSIC_WAH_H

#include "vintage_wah.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gp100_vintage_wah_t gp100_classic_wah_t;

/* Range, Q, and VOL are raw 0..99 controls. */
bool gp100_classic_wah_reset(gp100_classic_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level);

bool gp100_classic_wah_process(gp100_classic_wah_t *wah,
                                    uint8_t range,
                                    uint8_t resonance,
                                    uint8_t output_level,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
