#ifndef GP100_FIXED_WAH_INTERNAL_H
#define GP100_FIXED_WAH_INTERNAL_H

#include "vintage_wah.h"

bool gp100_fixed_wah_reset(gp100_vintage_wah_t *wah,
                                uint8_t range,
                                uint8_t resonance,
                                uint8_t output_level,
                                float input_omega);

bool gp100_fixed_wah_process(gp100_vintage_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level,
                                  float resonance_blend,
                                  float dry_mix,
                                  float cutoff_low_hz,
                                  float cutoff_span_hz,
                                  float wet_gain,
                                  float *interleaved_stereo,
                                  size_t frame_count,
                                  bool enabled);

#endif
