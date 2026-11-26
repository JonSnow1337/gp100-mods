#include "audio/effects/drive/penesas.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_penesas_t effect;
    gp100_penesas_filter_coefficients_t tone_coefficients;
    gp100_penesas_fourth_order_coefficients_t feedback_coefficients;
    float audio[2] = {1.0f, -1.0f};
    size_t frame;

    if (!gp100_penesas_reset(&effect, 50u, 50u, 50u)) {
        fprintf(stderr, "Penesas reset failed\n");
        return 1;
    }
    tone_coefficients = effect.tone_filter_coefficients;
    feedback_coefficients = effect.feedback_coefficients[0];
    if (!close_enough(effect.gain, 0.7057f, 0.0001f) ||
        !close_enough(effect.tone, 0.708643f, 0.0002f) ||
        !gp100_penesas_process(
            &effect, 99u, 50u, 50u, audio, 1u, true) ||
        audio[0] != audio[1] || !isfinite(audio[0]) ||
        !close_enough(effect.gain, 0.7058f, 0.00001f) ||
        memcmp(&tone_coefficients, &effect.tone_filter_coefficients,
               sizeof(tone_coefficients)) != 0 ||
        memcmp(&feedback_coefficients, &effect.feedback_coefficients[0],
               sizeof(feedback_coefficients)) == 0) {
        fprintf(stderr, "Penesas signal/control test failed\n");
        return 1;
    }
    for (frame = 0u; frame < 4096u; ++frame) {
        audio[0] = sinf(6.2831855f * (float)frame / 32.0f) * 0.25f;
        audio[1] = 0.0f;
        if (!gp100_penesas_process(
                &effect, 99u, 99u, 99u, audio, 1u, true) ||
            !isfinite(audio[0]) || audio[0] != audio[1]) {
            fprintf(stderr, "Penesas stability test failed\n");
            return 2;
        }
    }
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    if (!gp100_penesas_process(
            &effect, 0u, 0u, 0u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        gp100_penesas_reset(NULL, 0u, 0u, 0u) ||
        gp100_penesas_reset(&effect, 100u, 0u, 0u) ||
        gp100_penesas_process(
            &effect, 0u, 0u, 100u, audio, 1u, true) ||
        gp100_penesas_process(
            &effect, 0u, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "Penesas bypass/validation test failed\n");
        return 3;
    }
    return 0;
}
