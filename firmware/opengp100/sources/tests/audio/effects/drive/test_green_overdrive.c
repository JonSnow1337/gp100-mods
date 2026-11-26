#include "audio/effects/drive/green_overdrive.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_green_overdrive_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t index;
    if (!gp100_green_overdrive_reset(&effect, 50u, 50u, 50u) ||
        effect.gain < 0.17f || effect.gain > 0.18f ||
        !gp100_green_overdrive_process(&effect, 99u, 99u, 99u,
                                             audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - 0.505f) > 0.000001f) {
        fprintf(stderr, "Green OD control test failed\n");
        return 1;
    }
    for (index = 0u; index < 512u; ++index) {
        audio[0] = sinf(6.2831855f * (float)index / 37.0f) * 0.01f;
        if (!gp100_green_overdrive_process(&effect, 75u, 60u, 70u,
                                                 audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Green OD signal test failed\n");
            return 2;
        }
    }
    if (gp100_green_overdrive_reset(NULL, 0u, 0u, 0u) ||
        gp100_green_overdrive_reset(&effect, 100u, 0u, 0u) ||
        gp100_green_overdrive_process(&effect, 0u, 0u, 0u,
                                           NULL, 1u, true))
        return 3;
    return 0;
}
