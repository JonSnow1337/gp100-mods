#include "audio/effects/pitch/pitch_bender.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_pitch_bender_t bender;
    float audio[2] = {1.0f, -1.0f};
    size_t saved_index;

    if (!gp100_pitch_bender_reset(
            &bender, 12, -12, 50u, 50u, 50u) ||
        bender.phase[1] != UINT32_C(0x80000) ||
        bender.window[127] != 32767u || bender.window[255] != 201u ||
        !gp100_pitch_bender_process(
            &bender, 12, -12, 50u, 50u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.5f, 0.000001f) ||
        audio[0] != audio[1] || bender.phase[0] != UINT32_C(0xfffc0) ||
        bender.phase[1] != UINT32_C(0x7ffc0) || bender.write_index != 1u) {
        fprintf(stderr, "P-Bend signal test failed: out=%.9g phase=%lu\n",
                (double)audio[0], (unsigned long)bender.phase[0]);
        return 1;
    }
    saved_index = bender.write_index;
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    if (!gp100_pitch_bender_process(
            &bender, 12, -12, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        bender.write_index != saved_index || bender.high_pitch != 12 ||
        bender.low_pitch != -12 ||
        !close_enough(bender.range_mix, 0.99f, 0.000001f) ||
        gp100_pitch_bender_reset(NULL, 0, 0, 0u, 0u, 0u) ||
        gp100_pitch_bender_reset(&bender, 13, 0, 0u, 0u, 0u) ||
        gp100_pitch_bender_reset(&bender, 0, -13, 0u, 0u, 0u) ||
        gp100_pitch_bender_process(
            &bender, 0, 0, 100u, 0u, 0u, audio, 1u, true) ||
        gp100_pitch_bender_process(
            &bender, 0, 0, 0u, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "P-Bend bypass/validation test failed\n");
        return 2;
    }
    return 0;
}
