#include "usb_audio_capture.h"

#include <string.h>

#define GP100_USB_CAPTURE_GAIN_STEP 0.01f
#define GP100_AUDIO24_SCALE_IN (1.0f / 8388608.0f)
#define GP100_AUDIO24_SCALE_OUT 8388607.0f

void gp100_usb_capture_reset(
    gp100_usb_capture_ring_t *capture)
{
    if (capture != NULL) {
        memset(capture, 0, sizeof(*capture));
    }
}

void gp100_usb_capture_clear_buffer(
    gp100_usb_capture_ring_t *capture)
{
    if (capture != NULL) {
        float gain = capture->current_gain;

        gp100_usb_capture_reset(capture);
        capture->current_gain = gain;
    }
}

bool gp100_usb_capture_gain_for_db(
    int16_t gain_db,
    float *gain)
{
    return gp100_usb_audio_gain_for_db(gain_db, gain);
}

static int32_t capture_sample(int32_t sample, float gain)
{
    float normalized = (float)sample * GP100_AUDIO24_SCALE_IN * gain;

    if (normalized > 1.0f) {
        normalized = 1.0f;
    } else if (normalized < -1.0f) {
        normalized = -1.0f;
    }
    return (int32_t)(normalized * GP100_AUDIO24_SCALE_OUT);
}

bool gp100_usb_capture_publish(
    gp100_usb_capture_ring_t *capture,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    int16_t gain_db)
{
    float target_gain;
    size_t frame;

    if (capture == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        capture->write_index >= GP100_USB_CAPTURE_RING_FRAMES ||
        !gp100_usb_capture_gain_for_db(gain_db, &target_gain)) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;
        size_t ring_index = capture->write_index;

        capture->left[ring_index] = capture_sample(
            interleaved_stereo[sample], capture->current_gain);
        capture->right[ring_index] = capture_sample(
            interleaved_stereo[sample + 1u], capture->current_gain);

        /* The shipped code performs these as two sequential comparisons. */
        if (capture->current_gain < target_gain) {
            capture->current_gain += GP100_USB_CAPTURE_GAIN_STEP;
        }
        if (capture->current_gain > target_gain) {
            capture->current_gain -= GP100_USB_CAPTURE_GAIN_STEP;
        }

        ++capture->write_index;
        if (capture->write_index >= GP100_USB_CAPTURE_RING_FRAMES) {
            capture->write_index = 0u;
        }
    }
    return true;
}

bool gp100_usb_capture_select_sources(
    const int32_t *dry_interleaved_stereo,
    const int32_t *effect_interleaved_stereo,
    int32_t *output_interleaved_stereo,
    size_t frame_count,
    gp100_usb_record_source_t left_source,
    gp100_usb_record_source_t right_source)
{
    size_t frame;

    if ((left_source != GP100_USB_RECORD_SOURCE_DRY &&
         left_source != GP100_USB_RECORD_SOURCE_EFFECT) ||
        (right_source != GP100_USB_RECORD_SOURCE_DRY &&
         right_source != GP100_USB_RECORD_SOURCE_EFFECT) ||
        frame_count > SIZE_MAX / 2u ||
        (frame_count != 0u &&
         (dry_interleaved_stereo == NULL ||
          effect_interleaved_stereo == NULL ||
          output_interleaved_stereo == NULL))) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        const size_t sample = frame * 2u;
        const int32_t left = left_source == GP100_USB_RECORD_SOURCE_EFFECT
            ? effect_interleaved_stereo[sample]
            : dry_interleaved_stereo[sample];
        const int32_t right =
            right_source == GP100_USB_RECORD_SOURCE_EFFECT
            ? effect_interleaved_stereo[sample + 1u]
            : dry_interleaved_stereo[sample + 1u];

        output_interleaved_stereo[sample] = left;
        output_interleaved_stereo[sample + 1u] = right;
    }
    return true;
}

size_t gp100_usb_capture_available(
    const gp100_usb_capture_ring_t *capture)
{
    if (capture == NULL ||
        capture->write_index >= GP100_USB_CAPTURE_RING_FRAMES ||
        capture->read_index >= GP100_USB_CAPTURE_RING_FRAMES) {
        return 0u;
    }
    if (capture->write_index >= capture->read_index) {
        return capture->write_index - capture->read_index;
    }
    return GP100_USB_CAPTURE_RING_FRAMES - capture->read_index +
           capture->write_index;
}

static void zero_frames(int32_t *interleaved_stereo, size_t frame_count)
{
    memset(interleaved_stereo, 0, frame_count * 2u * sizeof(int32_t));
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

gp100_usb_capture_read_result_t gp100_usb_capture_read(
    gp100_usb_capture_ring_t *capture,
    int32_t *interleaved_stereo,
    size_t frame_count)
{
    size_t available;
    uint16_t phase_step;
    size_t frame;

    if (capture == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / (2u * sizeof(int32_t)) ||
        capture->write_index >= GP100_USB_CAPTURE_RING_FRAMES ||
        capture->read_index >= GP100_USB_CAPTURE_RING_FRAMES ||
        capture->phase >= 64u) {
        return GP100_USB_CAPTURE_READ_ERROR;
    }

    available = gp100_usb_capture_available(capture);
    if (!capture->started) {
        if (available >= 192u) {
            capture->started = true;
        }
        if (frame_count != 0u) {
            zero_frames(interleaved_stereo, frame_count);
        }
        return GP100_USB_CAPTURE_READ_BUFFERING;
    }

    if (available >= 256u) {
        phase_step = 65u;
    } else if (available <= 128u) {
        phase_step = 63u;
    } else {
        phase_step = 64u;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        size_t previous_index = capture->read_index == 0u
            ? GP100_USB_CAPTURE_RING_FRAMES - 1u
            : capture->read_index - 1u;
        size_t sample = frame * 2u;
        uint16_t advanced_phase;
        size_t advance;

        interleaved_stereo[sample] = interpolate_sample(
            capture->left[previous_index],
            capture->left[capture->read_index],
            capture->phase);
        interleaved_stereo[sample + 1u] = interpolate_sample(
            capture->right[previous_index],
            capture->right[capture->read_index],
            capture->phase);

        advanced_phase = (uint16_t)(capture->phase + phase_step);
        advance = advanced_phase / 64u;
        capture->phase = (uint16_t)(advanced_phase % 64u);
        capture->read_index =
            (capture->read_index + advance) %
            GP100_USB_CAPTURE_RING_FRAMES;
    }
    return GP100_USB_CAPTURE_READ_AUDIO;
}
