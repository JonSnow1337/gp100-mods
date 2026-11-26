#include "audio/effects/drive/lazaro.h"
#include "../../../../audio/effects/drive/lazaro_nonlinearity.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    gp100_lazaro_t effect;
    float audio[2] = {0.25f, -0.5f};
    size_t i;
    static const struct {
        float input;
        float model_a;
        float model_b;
    } curve_points[] = {
        {-10.0f, -0.7602837f, -1.0835758f},
        {-4.0f, -0.6904194f, -1.0068772f},
        {-0.6f, -0.4675625f, -0.5919559f},
        {-0.1f, -0.09868985f, -0.10027978f},
        {0.0f, 0.0f, 0.0f},
        {0.1f, 0.09948517f, 0.10004066f},
        {0.45f, 0.3759177f, 0.4463580f},
        {5.0f, 0.5580635f, 0.7609429f},
        {10.0f, 0.5881877f, 0.7939023f}
    };

    for (i = 0u; i < sizeof(curve_points) / sizeof(curve_points[0]); ++i) {
        lazaro_diode_models_t models =
            lazaro_diode_models(curve_points[i].input);
        if (fabsf(models.model_a - curve_points[i].model_a) > 0.00001f ||
            fabsf(models.model_b - curve_points[i].model_b) > 0.00001f) {
            fprintf(stderr, "Lazaro nonlinear curve test failed\n");
            return 1;
        }
    }
    if (!gp100_lazaro_reset(&effect, 50u, 50u, 50u) ||
        effect.volume != 0.0f || effect.volume_target != 0.5f ||
        !gp100_lazaro_process(&effect, 99u, 99u, 99u,
                                   audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        fabsf(effect.volume - 0.005f) > 0.000001f) return 1;
    for (i = 0u; i < 512u; ++i) {
        audio[0] = sinf(6.2831855f * (float)i / 47.0f) * 0.001f;
        if (!gp100_lazaro_process(&effect, 70u, 60u, 70u,
                                       audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) return 2;
    }
    if (gp100_lazaro_reset(NULL, 0u, 0u, 0u) ||
        gp100_lazaro_reset(&effect, 100u, 0u, 0u) ||
        gp100_lazaro_process(&effect, 100u, 0u, 0u,
                                  NULL, 0u, true)) return 3;
    return 0;
}
