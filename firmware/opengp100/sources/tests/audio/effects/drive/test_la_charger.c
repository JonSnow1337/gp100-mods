#include "audio/effects/drive/la_charger.h"

#include <math.h>

int main(void)
{
    gp100_la_charger_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t i;
    if (!gp100_la_charger_reset(&effect, 50u, 50u, 50u) ||
        !gp100_la_charger_process(&effect, 99u, 99u, 99u,
                                       audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f) return 1;
    for (i = 0u; i < 512u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.0f) * 0.001f;
        if (!gp100_la_charger_process(&effect, 70u, 60u, 70u,
                                           audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 2;
    }
    if (gp100_la_charger_reset(NULL, 0u, 0u, 0u) ||
        gp100_la_charger_reset(&effect, 100u, 0u, 0u) ||
        gp100_la_charger_process(&effect, 0u, 100u, 0u,
                                      NULL, 0u, true)) return 3;
    return 0;
}
