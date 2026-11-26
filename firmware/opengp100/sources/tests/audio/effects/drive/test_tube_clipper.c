#include "audio/effects/drive/tube_clipper.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_tube_clipper_t effect;
    float audio[2] = {0.125f, -0.25f};
    size_t index;
    if (!gp100_tube_clipper_reset(&effect, 50u, 50u, 50u, 50u) ||
        fabsf(effect.gain - 0.047055f) > 0.000001f ||
        fabsf(effect.volume - 0.106038f) > 0.000001f ||
        fabsf(effect.bass - 0.130200f) > 0.000001f ||
        !gp100_tube_clipper_process(&effect, 99u, 99u, 99u, 99u,
                                         audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        fabsf(effect.volume - 0.111038f) > 0.000001f ||
        fabsf(effect.bass - 0.135200f) > 0.000001f) {
        fprintf(stderr, "Tube Clipper control test failed\n");
        return 1;
    }
    for (index = 0u; index < 512u; ++index) {
        audio[0] = sinf(6.2831855f * (float)index / 37.0f) * 0.00001f;
        audio[1] = -audio[0];
        if (!gp100_tube_clipper_process(&effect, 70u, 70u, 60u, 40u,
                                             audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Tube Clipper signal test failed\n");
            return 2;
        }
    }
    if (gp100_tube_clipper_reset(NULL, 0u, 0u, 0u, 0u) ||
        gp100_tube_clipper_reset(&effect, 100u, 0u, 0u, 0u) ||
        gp100_tube_clipper_process(&effect, 0u, 0u, 0u, 0u,
                                        NULL, 1u, true))
        return 3;
    return 0;
}
