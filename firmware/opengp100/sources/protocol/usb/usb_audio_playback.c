#include "usb_audio_playback.h"

#include <string.h>

#define GP100_USB_PLAYBACK_GAIN_STEP 0.01f
#define GP100_AUDIO24_SCALE_IN (1.0f / 8388608.0f)
#define GP100_AUDIO24_SCALE_OUT 8388607.0f

void gp100_usb_playback_reset(
    gp100_usb_playback_ring_t *playback)
{
    if (playback != NULL) {
        memset(playback, 0, sizeof(*playback));
    }
}

void gp100_usb_playback_clear_buffer(
    gp100_usb_playback_ring_t *playback)
{
    if (playback != NULL) {
        float gain = playback->current_gain;

        gp100_usb_playback_reset(playback);
        playback->current_gain = gain;
    }
}

static int32_t playback_sample(int32_t sample, float gain)
{
    float normalized = (float)sample * GP100_AUDIO24_SCALE_IN * gain;

    if (normalized > 1.0f) {
        normalized = 1.0f;
    } else if (normalized < -1.0f) {
        normalized = -1.0f;
    }
    return (int32_t)(normalized * GP100_AUDIO24_SCALE_OUT);
}

bool gp100_usb_playback_publish(
    gp100_usb_playback_ring_t *playback,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    int16_t gain_db)
{
    float target_gain;
    size_t frame;

    if (playback == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        playback->write_index >= GP100_USB_PLAYBACK_RING_FRAMES ||
        !gp100_usb_audio_gain_for_db(gain_db, &target_gain)) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;
        size_t ring_index = playback->write_index;

        playback->left[ring_index] = playback_sample(
            interleaved_stereo[sample], playback->current_gain);
        playback->right[ring_index] = playback_sample(
            interleaved_stereo[sample + 1u], playback->current_gain);

        if (playback->current_gain < target_gain) {
            playback->current_gain += GP100_USB_PLAYBACK_GAIN_STEP;
        }
        if (playback->current_gain > target_gain) {
            playback->current_gain -= GP100_USB_PLAYBACK_GAIN_STEP;
        }

        ++playback->write_index;
        if (playback->write_index >= GP100_USB_PLAYBACK_RING_FRAMES) {
            playback->write_index = 0u;
        }
    }
    return true;
}

size_t gp100_usb_playback_available(
    const gp100_usb_playback_ring_t *playback)
{
    if (playback == NULL ||
        playback->write_index >= GP100_USB_PLAYBACK_RING_FRAMES ||
        playback->read_index >= GP100_USB_PLAYBACK_RING_FRAMES) {
        return 0u;
    }
    if (playback->write_index >= playback->read_index) {
        return playback->write_index - playback->read_index;
    }
    return GP100_USB_PLAYBACK_RING_FRAMES - playback->read_index +
           playback->write_index;
}

static int32_t interpolate_sample(
    int32_t previous,
    int32_t current,
    uint16_t phase)
{
    int64_t sum = (int64_t)current * phase +
                  (int64_t)previous * (64u - phase);

    return (int32_t)(sum >> 6);
}

gp100_usb_playback_read_result_t gp100_usb_playback_read(
    gp100_usb_playback_ring_t *playback,
    int32_t *interleaved_stereo,
    size_t frame_count)
{
    size_t available;
    uint16_t phase_step;
    size_t frame;

    if (playback == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / (2u * sizeof(int32_t)) ||
        playback->write_index >= GP100_USB_PLAYBACK_RING_FRAMES ||
        playback->read_index >= GP100_USB_PLAYBACK_RING_FRAMES ||
        playback->phase >= 64u) {
        return GP100_USB_PLAYBACK_READ_ERROR;
    }

    available = gp100_usb_playback_available(playback);
    if (!playback->started) {
        if (available >= 192u) {
            playback->started = true;
        }
        if (frame_count != 0u) {
            memset(
                interleaved_stereo,
                0,
                frame_count * 2u * sizeof(int32_t));
        }
        return GP100_USB_PLAYBACK_READ_BUFFERING;
    }

    if (available >= 256u) {
        phase_step = 65u;
    } else if (available <= 192u) {
        phase_step = 63u;
    } else {
        phase_step = 64u;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        size_t previous_index = playback->read_index == 0u
            ? GP100_USB_PLAYBACK_RING_FRAMES - 1u
            : playback->read_index - 1u;
        size_t sample = frame * 2u;
        uint16_t advanced_phase;
        size_t advance;

        interleaved_stereo[sample] = interpolate_sample(
            playback->left[previous_index],
            playback->left[playback->read_index],
            playback->phase);
        interleaved_stereo[sample + 1u] = interpolate_sample(
            playback->right[previous_index],
            playback->right[playback->read_index],
            playback->phase);

        advanced_phase = (uint16_t)(playback->phase + phase_step);
        advance = advanced_phase / 64u;
        playback->phase = (uint16_t)(advanced_phase % 64u);
        playback->read_index =
            (playback->read_index + advance) %
            GP100_USB_PLAYBACK_RING_FRAMES;
    }
    return GP100_USB_PLAYBACK_READ_AUDIO;
}
