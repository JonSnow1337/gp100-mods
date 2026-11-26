#include "audio/effects/pitch/octaver.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

int main(void)
{
    gp100_octaver_t octaver;
    float audio[2] = {1.0f, -1.0f};
    size_t saved_index;

    if (!gp100_octaver_reset(&octaver, 50u, 50u, 50u) ||
        octaver.low_phase[1] != UINT32_C(0x55555) ||
        octaver.low_phase[2] != UINT32_C(0xaaaaa) ||
        octaver.high_phase[1] != UINT32_C(0x80000) ||
        octaver.window[127] != 32767u || octaver.window[255] != 201u ||
        !gp100_octaver_process(
            &octaver, 50u, 50u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.50009155f, 0.000001f) ||
        audio[0] != audio[1] ||
        octaver.write_index != 1u || octaver.low_phase[0] != 128u ||
        octaver.high_phase[0] != UINT32_C(0xfff00)) {
        fprintf(stderr,
                "octaver signal test failed: out=%g window=%u/%u write=%zu "
                "low=%#x high=%#x\n",
                (double)audio[0], octaver.window[127], octaver.window[255],
                octaver.write_index, octaver.low_phase[0],
                octaver.high_phase[0]);
        return 1;
    }
    saved_index = octaver.write_index;
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    if (!gp100_octaver_process(
            &octaver, 99u, 99u, 99u, audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        octaver.write_index != saved_index ||
        !close_enough(octaver.low_mix, 0.99f, 0.000001f) ||
        gp100_octaver_reset(NULL, 0u, 0u, 0u) ||
        gp100_octaver_process(
            &octaver, 100u, 0u, 0u, audio, 1u, true) ||
        gp100_octaver_process(
            &octaver, 0u, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "octaver bypass/validation test failed\n");
        return 2;
    }
    return 0;
}
