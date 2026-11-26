#include "audio/effects/drive/bass_distortion.h"

#include <math.h>

int main(void)
{
    gp100_bass_distortion_t effect;
    float audio[2] = {0.25f, -0.5f};
    float initial_volume;
    size_t i;
    if (!gp100_bass_distortion_reset(&effect, 50u, 50u, 50u, 50u, 50u))
        return 1;
    initial_volume = effect.volume;
    if (!gp100_bass_distortion_process(
            &effect, 99u, 99u, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - (initial_volume + 0.005f)) > 0.000001f)
        return 2;
    for (i = 0u; i < 4096u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.0f) * 0.01f;
        if (!gp100_bass_distortion_process(
                &effect, 70u, 65u, 70u, 60u, 75u, audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 3;
    }
    if (fabsf(effect.treble - effect.treble_target) > 0.00011f)
        return 4;
    if (gp100_bass_distortion_reset(NULL, 0u, 0u, 0u, 0u, 0u) ||
        gp100_bass_distortion_reset(&effect, 100u, 0u, 0u, 0u, 0u) ||
        gp100_bass_distortion_process(
            &effect, 0u, 0u, 0u, 0u, 100u, NULL, 0u, true)) return 5;
    return 0;
}
