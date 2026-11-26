#include "audio/effects/drive/taichi_overdrive.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_taichi_overdrive_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t index;
    if (!gp100_taichi_overdrive_reset(&effect, 50u, 50u, 50u, 50u) ||
        fabsf(effect.gain - 0.5f) > 0.000001f ||
        effect.voice < 0.3512f || effect.voice > 0.3515f ||
        !gp100_taichi_overdrive_process(&effect, 99u, 99u, 99u, 99u,
                                              audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - 0.505f) > 0.000001f) {
        fprintf(stderr, "TaiChi OD control test failed\n");
        return 1;
    }
    audio[0] = 0.001f;
    audio[1] = -0.001f;
    if (!gp100_taichi_overdrive_process(&effect, 50u, 50u, 50u, 50u,
                                              audio, 1u, true) ||
        effect.voice < 0.7055f || effect.voice > 0.7058f)
        return 2;
    for (index = 0u; index < 512u; ++index) {
        audio[0] = sinf(6.2831855f * (float)index / 41.0f) * 0.01f;
        if (!gp100_taichi_overdrive_process(&effect, 70u, 60u, 75u, 40u,
                                                  audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "TaiChi OD signal test failed\n");
            return 3;
        }
    }
    if (gp100_taichi_overdrive_reset(NULL, 0u, 0u, 0u, 0u) ||
        gp100_taichi_overdrive_reset(&effect, 100u, 0u, 0u, 0u) ||
        gp100_taichi_overdrive_process(&effect, 0u, 0u, 0u, 0u,
                                            NULL, 1u, true))
        return 4;
    return 0;
}
