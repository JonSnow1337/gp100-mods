#include "audio_route.h"

#include <string.h>

static int32_t signed_from_bits(uint32_t bits)
{
    int32_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int32_t wrapped_sum(
    int32_t live,
    int32_t usb,
    int32_t pre_looper,
    int32_t rhythm,
    unsigned int rhythm_scale)
{
    uint32_t sum = (uint32_t)live + (uint32_t)usb;

    sum += (uint32_t)pre_looper;
    sum += (uint32_t)rhythm * rhythm_scale;
    return signed_from_bits(sum);
}

static bool buffers_valid(
    gp100_audio_route_state_t route_state,
    const gp100_audio_route_buffers_t *buffers,
    size_t frame_count)
{
    if (!gp100_audio_route_state_valid(route_state) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    if (frame_count == 0u) {
        return true;
    }
    if (buffers == NULL || buffers->live_stereo == NULL ||
        buffers->usb_playback_stereo == NULL ||
        buffers->float_stereo == NULL) {
        return false;
    }
    if (route_state == GP100_AUDIO_ROUTE_NORMAL &&
        buffers->pre_looper_stereo == NULL) {
        return false;
    }
    return route_state == GP100_AUDIO_ROUTE_TUNER_MUTE ||
           buffers->rhythm_mono != NULL;
}

bool gp100_audio_route_mix(
    gp100_audio_route_state_t route_state,
    const gp100_audio_route_buffers_t *buffers,
    size_t frame_count)
{
    size_t frame;

    if (!buffers_valid(route_state, buffers, frame_count)) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        const size_t left = frame * 2u;
        const size_t right = left + 1u;
        int32_t output_left;
        int32_t output_right;

        if (route_state == GP100_AUDIO_ROUTE_TUNER_MUTE) {
            output_left = buffers->usb_playback_stereo[left];
            output_right = buffers->usb_playback_stereo[right];
        } else if (route_state == GP100_AUDIO_ROUTE_NORMAL) {
            output_left = wrapped_sum(
                buffers->live_stereo[left],
                buffers->usb_playback_stereo[left],
                buffers->pre_looper_stereo[left],
                buffers->rhythm_mono[frame],
                4u);
            output_right = wrapped_sum(
                buffers->live_stereo[right],
                buffers->usb_playback_stereo[right],
                buffers->pre_looper_stereo[right],
                buffers->rhythm_mono[frame],
                4u);
        } else {
            output_left = wrapped_sum(
                buffers->live_stereo[left],
                buffers->usb_playback_stereo[left],
                0,
                buffers->rhythm_mono[frame],
                2u);
            output_right = wrapped_sum(
                buffers->live_stereo[right],
                buffers->usb_playback_stereo[right],
                0,
                buffers->rhythm_mono[frame],
                2u);
        }
        buffers->live_stereo[left] = output_left;
        buffers->live_stereo[right] = output_right;
        buffers->float_stereo[left] =
            (float)output_left * GP100_AUDIO_ROUTE_INT_TO_FLOAT;
        buffers->float_stereo[right] =
            (float)output_right * GP100_AUDIO_ROUTE_INT_TO_FLOAT;
    }
    return true;
}
