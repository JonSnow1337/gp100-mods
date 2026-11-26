#include "audio/effects/drive/blues_overdrive.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_blues_overdrive_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t i;
    if (!gp100_blues_overdrive_reset(&effect, 50u, 50u, 50u) ||
        effect.gain < 0.24f || effect.gain > 0.27f ||
        effect.tone < 0.49f || effect.tone > 0.51f ||
        !gp100_blues_overdrive_process(&effect, 99u, 99u, 99u,
                                             audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        effect.volume <= 0.25f) {
        fprintf(stderr, "Blues OD control test failed\n");
        return 1;
    }
    for (i = 0u; i < 512u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 43.0f) * 0.001f;
        if (!gp100_blues_overdrive_process(&effect, 70u, 60u, 70u,
                                                 audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 2;
    }
    if (gp100_blues_overdrive_reset(NULL, 0u, 0u, 0u) ||
        gp100_blues_overdrive_reset(&effect, 100u, 0u, 0u) ||
        gp100_blues_overdrive_process(&effect, 0u, 0u, 0u,
                                           NULL, 1u, true)) return 3;
    return 0;
}
