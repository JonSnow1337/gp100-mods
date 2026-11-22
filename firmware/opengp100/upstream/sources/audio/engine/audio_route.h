#ifndef GP100_AUDIO_ROUTE_H
#define GP100_AUDIO_ROUTE_H

#include "audio_transition.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exact float represented by factory literal 0x340d0cc1. */
#define GP100_AUDIO_ROUTE_INT_TO_FLOAT 0x1.1a1982p-23f

/*
 * Semantic names describe the recovered producers, not physical addresses:
 *
 * live_stereo          factory workspace at 0x20015160, updated in place
 * usb_playback_stereo  host-to-device stream at 0x20016d60
 * pre_looper_stereo    shared PRE/POST Looper wet buffer at 0x20017560
 * rhythm_mono          rhythm-player output at 0x20016960
 * float_stereo         downstream float workspace at 0x20016360
 */
typedef struct gp100_audio_route_buffers {
    int32_t *live_stereo;
    const int32_t *usb_playback_stereo;
    const int32_t *pre_looper_stereo;
    const int32_t *rhythm_mono;
    float *float_stereo;
} gp100_audio_route_buffers_t;

/*
 * Reproduces the observed integer route branch and its conversion to float.
 * Integer addition wraps modulo 2^32, matching the Cortex-M7 instructions.
 * The route-state names retain their UI correlation; see audio_route.md for
 * the important distinction between that correlation and branch arithmetic.
 */
bool gp100_audio_route_mix(
    gp100_audio_route_state_t route_state,
    const gp100_audio_route_buffers_t *buffers,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
