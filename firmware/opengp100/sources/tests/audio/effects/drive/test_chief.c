#include "audio/effects/drive/chief.h"
#include "../../../../audio/effects/drive/chief_nonlinearity.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_chief_t effect;
    float audio[2] = {0.125f, -0.25f};
    size_t index;
    static const struct {
        float input;
        float output;
    } curve_points[] = {
        {0.0f, 0.0f},
        {0.1f, 0.09110287f},
        {0.5f, 0.44768546f},
        {1.0f, 0.6135292f},
        {6.0f, 0.7362953f},
        {10.0f, 0.7588302f}
    };

    for (index = 0u; index < sizeof(curve_points) / sizeof(curve_points[0]);
         ++index) {
        float positive = chief_nonlinearity(curve_points[index].input);
        float negative = chief_nonlinearity(-curve_points[index].input);
        if (fabsf(positive - curve_points[index].output) > 0.00001f ||
            fabsf(positive + negative) > 0.000001f) {
            fprintf(stderr, "Chief nonlinear curve test failed\n");
            return 1;
        }
    }
    if (fabsf(chief_nonlinearity(0.4999f) -
              chief_nonlinearity(0.5f)) > 0.002f ||
        fabsf(chief_nonlinearity(5.9999f) -
              chief_nonlinearity(6.0f)) > 0.002f) {
        fprintf(stderr, "Chief nonlinear boundary test failed\n");
        return 1;
    }

    if (!gp100_chief_reset(&effect, 50u, 50u, 50u, 50u, 50u) ||
        effect.control[0] < 0.24f || effect.control[0] > 0.27f ||
        effect.control[2] < 0.24f || effect.control[2] > 0.27f ||
        !gp100_chief_process(&effect, 99u, 99u, 99u, 99u, 99u,
                                  audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        effect.control[0] <= 0.25f) {
        fprintf(stderr, "Chief control test failed\n");
        return 1;
    }
    for (index = 0u; index < 1024u; ++index) {
        audio[0] = sinf(6.2831855f * (float)index / 47.0f) * 0.001f;
        if (!gp100_chief_process(&effect, 70u, 70u, 60u, 55u, 45u,
                                      audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Chief signal test failed\n");
            return 2;
        }
    }
    if (gp100_chief_reset(NULL, 0u, 0u, 0u, 0u, 0u) ||
        gp100_chief_reset(&effect, 100u, 0u, 0u, 0u, 0u) ||
        gp100_chief_process(&effect, 0u, 0u, 0u, 0u, 0u,
                                 NULL, 1u, true))
        return 3;
    return 0;
}
