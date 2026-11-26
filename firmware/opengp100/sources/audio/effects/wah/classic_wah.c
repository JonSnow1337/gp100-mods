#include "classic_wah.h"

#include "fixed_wah_internal.h"

bool gp100_classic_wah_reset(gp100_classic_wah_t *wah,
                                  uint8_t range,
                                  uint8_t resonance,
                                  uint8_t output_level)
{
    return gp100_fixed_wah_reset(
        wah, range, resonance, output_level, 0x1.bb858ap-6f);
}

bool gp100_classic_wah_process(gp100_classic_wah_t *wah,
                                    uint8_t range,
                                    uint8_t resonance,
                                    uint8_t output_level,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled)
{
    return gp100_fixed_wah_process(
        wah, range, resonance, output_level,
        0.79f, 0.75f, 420.0f, 1680.0f, 7.0795f,
        interleaved_stereo, frame_count, enabled);
}
