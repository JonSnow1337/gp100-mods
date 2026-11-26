#include "audio/effects/drive/plustortion.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_plustortion_t effect;
    float audio[2] = {0.125f, -0.25f};
    size_t index;
    if (!gp100_plustortion_reset(&effect, 50u, 50u) ||
        effect.volume != 0.0f ||
        !gp100_plustortion_process(&effect, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        effect.volume != 0.0f) {
        fprintf(stderr, "Plustortion control test failed\n");
        return 1;
    }
    for (index = 0u; index < 2048u; ++index) {
        audio[0] = sinf(6.2831855f * (float)index / 43.0f) * 0.01f;
        if (!gp100_plustortion_process(&effect, 75u, 70u,
                                            audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Plustortion signal test failed\n");
            return 2;
        }
    }
    if (effect.volume < 0.2047f || effect.volume > 0.2049f ||
        gp100_plustortion_reset(NULL, 0u, 0u) ||
        gp100_plustortion_reset(&effect, 100u, 0u) ||
        gp100_plustortion_process(&effect, 0u, 0u, NULL, 1u, true))
        return 3;
    return 0;
}
