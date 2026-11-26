#include "audio/effects/drive/flex_overdrive.h"

#include <math.h>

int main(void)
{
    gp100_flex_overdrive_t effect;
    float audio[2] = {0.25f, -0.5f};
    unsigned mode;
    size_t i;
    if (!gp100_flex_overdrive_reset(&effect, 50u, 50u, 50u, 0u, 50u) ||
        !gp100_flex_overdrive_process(&effect, 99u, 99u, 99u, 0u, 99u,
                                           audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - 0.2578f) > 0.000001f ||
        fabsf(effect.blend - 0.505f) > 0.000001f) return 1;
    for (mode = 0u; mode < 3u; ++mode) {
        if (!gp100_flex_overdrive_reset(&effect, 50u, 50u, 50u,
                                              (uint8_t)mode, 50u)) return 2;
        for (i = 0u; i < 256u; ++i) {
            audio[0] = sinf(6.2831855f * (float)i / 47.0f) * 0.001f;
            if (!gp100_flex_overdrive_process(
                    &effect, 70u, 60u, 70u, (uint8_t)mode, 65u,
                    audio, 1u, true) || !isfinite(audio[0]) ||
                audio[0] != audio[1]) return 3;
        }
    }
    if (gp100_flex_overdrive_reset(NULL, 0u, 0u, 0u, 0u, 0u) ||
        gp100_flex_overdrive_reset(&effect, 0u, 0u, 0u, 3u, 0u) ||
        gp100_flex_overdrive_process(&effect, 0u, 0u, 0u, 0u, 100u,
                                          NULL, 0u, true)) return 4;
    return 0;
}
