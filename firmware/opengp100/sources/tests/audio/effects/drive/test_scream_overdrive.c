#include "audio/effects/drive/scream_overdrive.h"
#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_scream_overdrive_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t i;
    if (!gp100_scream_overdrive_reset(&effect, 50u, 50u, 50u, false, true) ||
        effect.gain < 0.17f || effect.gain > 0.18f ||
        !gp100_scream_overdrive_process(&effect, 99u, 50u, 99u,
                                             true, true, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - 0.505f) > 0.000001f) {
        fprintf(stderr, "Scream OD control test failed\n"); return 1;
    }
    for (i = 0u; i < 256u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 32.0f) * 0.1f;
        if (!gp100_scream_overdrive_process(&effect, 80u, 60u, 70u,
                                                  true, true, audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Scream OD signal test failed\n"); return 2;
        }
    }
    if (gp100_scream_overdrive_reset(NULL, 0u, 0u, 0u, false, false) ||
        gp100_scream_overdrive_reset(&effect, 100u, 0u, 0u, false, false) ||
        gp100_scream_overdrive_process(&effect, 0u, 0u, 0u, false, false,
                                            NULL, 1u, true)) return 3;
    return 0;
}
