#include "protocol/usb/usb_audio_playback.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.000001f;
}

static int test_publish(void)
{
    gp100_usb_playback_ring_t playback;
    const int32_t samples[4] = {
        8388608, -8388608,
        INT32_MAX, INT32_MIN
    };

    gp100_usb_playback_reset(&playback);
    if (!gp100_usb_playback_publish(&playback, samples, 1u, 0) ||
        playback.left[0] != 0 || playback.right[0] != 0 ||
        playback.write_index != 1u ||
        !close_enough(playback.current_gain, 0.01f)) {
        return 1;
    }

    playback.current_gain = 10.0f;
    playback.write_index = 319u;
    if (!gp100_usb_playback_publish(&playback, samples, 2u, 20) ||
        playback.left[319] != 8388607 ||
        playback.right[319] != -8388607 ||
        playback.left[0] != 8388607 || playback.right[0] != -8388607 ||
        playback.write_index != 1u) {
        return 2;
    }
    return 0;
}

static int test_startup_and_interpolation(void)
{
    gp100_usb_playback_ring_t playback;
    int32_t output[4] = {1, 2, 3, 4};

    gp100_usb_playback_reset(&playback);
    playback.write_index = 192u;
    if (gp100_usb_playback_read(&playback, output, 2u) !=
            GP100_USB_PLAYBACK_READ_BUFFERING ||
        !playback.started || output[0] != 0 || output[3] != 0) {
        return 1;
    }

    playback.left[319] = 640;
    playback.right[319] = -640;
    playback.left[0] = 1280;
    playback.right[0] = -1280;
    if (gp100_usb_playback_read(&playback, output, 2u) !=
            GP100_USB_PLAYBACK_READ_AUDIO ||
        output[0] != 640 || output[1] != -640 ||
        output[2] != 1270 || output[3] != -1270 ||
        playback.read_index != 1u || playback.phase != 62u) {
        return 2;
    }
    return 0;
}

static int test_occupancy_phase_steps(void)
{
    gp100_usb_playback_ring_t playback;
    int32_t output[2];

    gp100_usb_playback_reset(&playback);
    playback.started = true;
    playback.write_index = 193u;
    if (gp100_usb_playback_read(&playback, output, 1u) !=
            GP100_USB_PLAYBACK_READ_AUDIO ||
        playback.read_index != 1u || playback.phase != 0u) {
        return 1;
    }

    playback.read_index = 0u;
    playback.write_index = 256u;
    playback.phase = 63u;
    if (gp100_usb_playback_read(&playback, output, 1u) !=
            GP100_USB_PLAYBACK_READ_AUDIO ||
        playback.read_index != 2u || playback.phase != 0u) {
        return 2;
    }
    return 0;
}

static int test_occupancy_phase_boundaries(void)
{
    static const struct {
        size_t occupancy;
        size_t expected_index;
        uint16_t expected_phase;
    } cases[] = {
        {192u, 0u, 63u},
        {193u, 1u, 0u},
        {255u, 1u, 0u},
        {256u, 1u, 1u}
    };
    gp100_usb_playback_ring_t playback;
    int32_t output[2];
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        gp100_usb_playback_reset(&playback);
        playback.started = true;
        playback.write_index = cases[index].occupancy;
        if (gp100_usb_playback_read(&playback, output, 1u) !=
                GP100_USB_PLAYBACK_READ_AUDIO ||
            playback.read_index != cases[index].expected_index ||
            playback.phase != cases[index].expected_phase) {
            return (int)index + 1;
        }
    }
    return 0;
}

static int test_clear_live_buffer(void)
{
    gp100_usb_playback_ring_t playback;
    const int32_t first_frame[] = {2000, -2000};
    size_t index;

    gp100_usb_playback_reset(&playback);
    playback.current_gain = 0.5f;
    playback.left[319] = 1;
    playback.right[319] = -1;
    playback.write_index = 23u;
    playback.read_index = 5u;
    playback.phase = 12u;
    playback.started = true;
    gp100_usb_playback_clear_buffer(&playback);
    if (playback.write_index != 0u || playback.read_index != 0u ||
        playback.phase != 0u || playback.started ||
        playback.current_gain != 0.5f) {
        return 1;
    }
    for (index = 0u; index < GP100_USB_PLAYBACK_RING_FRAMES; ++index) {
        if (playback.left[index] != 0 || playback.right[index] != 0) {
            return 2;
        }
    }
    if (!gp100_usb_playback_publish(&playback, first_frame, 1u, 0) ||
        playback.left[0] != 999 || playback.right[0] != -999) {
        return 3;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_usb_playback_ring_t playback;
    const int32_t input[2] = {0, 0};
    int32_t output[2];

    gp100_usb_playback_reset(&playback);
    if (gp100_usb_playback_publish(NULL, input, 1u, 0) ||
        gp100_usb_playback_publish(&playback, NULL, 1u, 0) ||
        gp100_usb_playback_publish(&playback, input, SIZE_MAX, 0) ||
        gp100_usb_playback_publish(&playback, input, 1u, 21) ||
        gp100_usb_playback_read(NULL, output, 1u) !=
            GP100_USB_PLAYBACK_READ_ERROR ||
        gp100_usb_playback_read(&playback, NULL, 1u) !=
            GP100_USB_PLAYBACK_READ_ERROR ||
        gp100_usb_playback_read(&playback, output, SIZE_MAX) !=
            GP100_USB_PLAYBACK_READ_ERROR) {
        return 1;
    }
    playback.phase = 64u;
    if (gp100_usb_playback_read(&playback, output, 1u) !=
            GP100_USB_PLAYBACK_READ_ERROR) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_publish();
    if (result != 0) {
        fprintf(stderr, "publish test failed: %d\n", result);
        return 10 + result;
    }
    result = test_startup_and_interpolation();
    if (result != 0) {
        fprintf(stderr, "startup/interpolation test failed: %d\n", result);
        return 20 + result;
    }
    result = test_occupancy_phase_steps();
    if (result != 0) {
        fprintf(stderr, "phase-step test failed: %d\n", result);
        return 30 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 40 + result;
    }
    result = test_occupancy_phase_boundaries();
    if (result != 0) {
        fprintf(stderr, "phase-boundary test failed: %d\n", result);
        return 50 + result;
    }
    result = test_clear_live_buffer();
    if (result != 0) {
        fprintf(stderr, "clear live buffer test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
