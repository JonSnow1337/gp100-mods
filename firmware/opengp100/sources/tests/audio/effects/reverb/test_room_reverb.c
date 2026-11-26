#include "audio/effects/reverb/room_reverb.h"

#include <math.h>

static gp100_room_reverb_t state;

int main(void)
{
    float audio[2] = {0.25f, -0.125f};
    int wet_seen = 0;
    int stereo_seen = 0;
    size_t frame;

    if (!gp100_room_reverb_reset(&state, 20u, 50u, 49u, true) ||
        fabsf(state.current_mix - 0.2f) > 0.000001f ||
        fabsf(state.current_predelay - 0.5f) > 0.000001f ||
        fabsf(state.decay - 0.522825f) > 0.000001f ||
        !gp100_room_reverb_process(&state, 99u, 100u, 99u, true,
                                        audio, 1u, true) ||
        !(state.current_mix > 0.2f) ||
        !(state.current_predelay > 0.5f) ||
        fabsf(state.decay - 0.7485f) > 0.000001f) {
        return 1;
    }

    if (!gp100_room_reverb_reset(&state, 80u, 0u, 70u, true)) {
        return 2;
    }
    for (frame = 0u; frame < 10000u; ++frame) {
        audio[0] = frame == 0u ? 1.0f : 0.0f;
        audio[1] = frame == 0u ? 1.0f : 0.0f;
        if (!gp100_room_reverb_process(&state, 80u, 0u, 70u, true,
                                            audio, 1u, true) ||
            !isfinite(audio[0]) || !isfinite(audio[1])) {
            return 3;
        }
        if (frame > 1300u && (fabsf(audio[0]) > 0.000001f ||
                              fabsf(audio[1]) > 0.000001f)) {
            wet_seen = 1;
        }
        if (fabsf(audio[0] - audio[1]) > 0.000001f) {
            stereo_seen = 1;
        }
    }
    if (!wet_seen || !stereo_seen) {
        return 4;
    }

    audio[0] = 0.125f;
    audio[1] = -0.25f;
    if (!gp100_room_reverb_process(&state, 20u, 50u, 50u, false,
                                        audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        state.history_written || state.lfo_phase != 0.0f ||
        state.predelay_write != 0u || state.fdn_write[0] != 0u ||
        gp100_room_reverb_process(&state, 100u, 50u, 50u, false,
                                       audio, 1u, true) ||
        gp100_room_reverb_process(&state, 20u, 101u, 50u, false,
                                       audio, 1u, true) ||
        gp100_room_reverb_process(&state, 20u, 50u, 100u, false,
                                       audio, 1u, true) ||
        gp100_room_reverb_process(NULL, 20u, 50u, 50u, false,
                                       audio, 1u, true)) {
        return 5;
    }
    if (!gp100_room_reverb_reset(&state, 20u, 50u, 50u, false) ||
        !gp100_room_reverb_process(&state, 20u, 50u, 50u, false,
                                        audio, 1u, false) ||
        audio[0] != 0.125f || audio[1] != -0.25f ||
        !state.bypass_cleared || state.history_written ||
        state.lfo_phase != 0.0f) {
        return 6;
    }
    return 0;
}
