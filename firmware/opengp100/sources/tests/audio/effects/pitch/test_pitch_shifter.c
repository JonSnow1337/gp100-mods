#include "audio/effects/pitch/pitch_shifter.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_pitch_shifter_t shifter;
    float audio[2] = {1.0f, -1.0f};
    size_t saved_high_index;
    size_t saved_low_index;

    if (!gp100_pitch_shifter_reset(
            &shifter, 12, -12, 50u, 50u, 50u) ||
        shifter.high.phase[1] != UINT32_C(0x80000) ||
        shifter.low.phase[1] != UINT32_C(0x80000) ||
        shifter.window[127] != 32767u || shifter.window[255] != 201u ||
        !gp100_pitch_shifter_process(
            &shifter, 12, -12, 50u, 50u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.50005144f, 0.000001f) ||
        audio[0] != audio[1] ||
        shifter.high.phase[0] != UINT32_C(0xfff00) ||
        shifter.low.phase[0] != 128u ||
        shifter.high.write_index != 1u || shifter.low.write_index != 1u) {
        fprintf(stderr, "Pitch signal test failed: out=%.9g\n",
                (double)audio[0]);
        return 1;
    }
    saved_high_index = shifter.high.write_index;
    saved_low_index = shifter.low.write_index;
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    if (!gp100_pitch_shifter_process(
            &shifter, 24, -24, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        shifter.high.write_index != saved_high_index ||
        shifter.low.write_index != saved_low_index ||
        shifter.high_pitch != 24 || shifter.low_pitch != -24 ||
        !close_enough(shifter.dry_mix, 0.99f, 0.000001f) ||
        gp100_pitch_shifter_reset(
            NULL, 0, 0, 0u, 0u, 0u) ||
        gp100_pitch_shifter_reset(
            &shifter, -1, 0, 0u, 0u, 0u) ||
        gp100_pitch_shifter_reset(
            &shifter, 0, 1, 0u, 0u, 0u) ||
        gp100_pitch_shifter_process(
            &shifter, 0, 0, 100u, 0u, 0u, audio, 1u, true) ||
        gp100_pitch_shifter_process(
            &shifter, 0, 0, 0u, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "Pitch bypass/validation test failed\n");
        return 2;
    }
    return 0;
}
