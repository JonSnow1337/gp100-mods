#include "audio/effects/drive/saturate.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_saturate_t saturate;
    float audio[2] = {0.25f, -0.5f};
    size_t index;

    if (!gp100_saturate_reset(&saturate, 50u, 50u, 50u, 50u) ||
        !close_enough(saturate.gain, 0.1235f, 0.000001f) ||
        !close_enough(saturate.mix, 0.1235f, 0.000001f) ||
        !close_enough(saturate.output_level, 0.5f, 0.000001f) ||
        !close_enough(saturate.high_cut_control, 0.18872f, 0.000001f) ||
        !gp100_saturate_process(
            &saturate, 99u, 0u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        !close_enough(saturate.gain, 0.1236f, 0.000001f) ||
        !close_enough(saturate.mix, 0.1234f, 0.000001f) ||
        !close_enough(saturate.output_level, 0.5001f, 0.000001f)) {
        fprintf(stderr, "Saturate control/bypass test failed\n");
        return 1;
    }
    for (index = 0u; index < 128u; ++index) {
        audio[0] = index == 0u ? 1.0f : 0.0f;
        audio[1] = -1.0f;
        if (!gp100_saturate_process(
                &saturate, 99u, 99u, 99u, 0u, audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Saturate signal test failed at %lu\n",
                    (unsigned long)index);
            return 2;
        }
    }
    if (saturate.high_cut_control != 1.0f ||
        gp100_saturate_reset(NULL, 0u, 0u, 0u, 0u) ||
        gp100_saturate_reset(&saturate, 100u, 0u, 0u, 0u) ||
        gp100_saturate_process(
            &saturate, 0u, 0u, 0u, 100u, audio, 1u, true) ||
        gp100_saturate_process(
            &saturate, 0u, 0u, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "Saturate validation test failed\n");
        return 3;
    }
    return 0;
}
