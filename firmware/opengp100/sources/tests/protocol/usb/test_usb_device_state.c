#include "protocol/usb/usb_device_state.h"

#include <stdio.h>
#include <string.h>

static int test_reset_and_configuration(void)
{
    gp100_usb_device_state_t state;
    uint32_t changes;

    memset(&state, 0xff, sizeof(state));
    gp100_usb_device_state_reset(&state);
    if (state.sample_rate != 44100u || state.configuration != 0u ||
        state.alternate[0] != 0u || state.alternate[1] != 0u ||
        state.alternate[2] != 0u || state.alternate[3] != 0u ||
        !gp100_usb_device_set_configuration(&state, 1u, &changes) ||
        changes != GP100_USB_ENDPOINT_MIDI_START ||
        !gp100_usb_device_set_configuration(&state, 1u, &changes) ||
        changes != 0u ||
        gp100_usb_device_set_configuration(&state, 2u, &changes) ||
        gp100_usb_device_set_configuration(NULL, 1u, &changes) ||
        gp100_usb_device_set_configuration(&state, 1u, NULL)) {
        return 1;
    }
    return 0;
}

static int test_interface_lifecycle(void)
{
    gp100_usb_device_state_t state;
    gp100_usb_audio_format_t format;
    uint32_t changes;

    gp100_usb_device_state_reset(&state);
    if (gp100_usb_device_set_interface(&state, 1u, 1u, &changes) ||
        !gp100_usb_device_set_configuration(&state, 1u, &changes) ||
        !gp100_usb_device_set_interface(&state, 1u, 1u, &changes) ||
        changes != GP100_USB_ENDPOINT_CAPTURE_START ||
        !gp100_usb_device_audio_format(&state, 1u, &format) ||
        format != GP100_USB_AUDIO_PCM16_STEREO ||
        !gp100_usb_device_set_interface(&state, 1u, 2u, &changes) ||
        changes != (GP100_USB_ENDPOINT_CAPTURE_STOP |
                    GP100_USB_ENDPOINT_CAPTURE_START) ||
        !gp100_usb_device_audio_format(&state, 1u, &format) ||
        format != GP100_USB_AUDIO_PCM32_STEREO ||
        !gp100_usb_device_set_interface(&state, 2u, 2u, &changes) ||
        changes != GP100_USB_ENDPOINT_PLAYBACK_START ||
        !gp100_usb_device_set_interface(&state, 1u, 0u, &changes) ||
        changes != GP100_USB_ENDPOINT_CAPTURE_STOP ||
        gp100_usb_device_audio_format(&state, 1u, &format) ||
        gp100_usb_device_set_interface(&state, 0u, 1u, &changes) ||
        gp100_usb_device_set_interface(&state, 3u, 1u, &changes) ||
        gp100_usb_device_set_interface(&state, 4u, 0u, &changes)) {
        return 1;
    }

    if (!gp100_usb_device_set_configuration(&state, 0u, &changes) ||
        changes != (GP100_USB_ENDPOINT_MIDI_STOP |
                    GP100_USB_ENDPOINT_PLAYBACK_STOP) ||
        state.alternate[1] != 0u || state.alternate[2] != 0u) {
        return 2;
    }
    return 0;
}

static int test_sample_rates(void)
{
    static const uint32_t supported[] = {
        44100u, 48000u, 88200u, 96000u, 176400u, 192000u
    };
    gp100_usb_device_state_t state;
    uint32_t changes;
    size_t index;

    gp100_usb_device_state_reset(&state);
    for (index = 0u; index < sizeof(supported) / sizeof(supported[0]); ++index) {
        if (!gp100_usb_sample_rate_supported(supported[index]) ||
            !gp100_usb_device_set_sample_rate(
                &state, supported[index], &changes) ||
            changes != (index == 0u
                ? GP100_USB_ENDPOINT_CHANGE_NONE
                : GP100_USB_ENDPOINT_RATE_CHANGED)) {
            return 1;
        }
    }
    if (gp100_usb_sample_rate_supported(12345u) ||
        gp100_usb_device_set_sample_rate(&state, 12345u, &changes) ||
        gp100_usb_device_set_sample_rate(NULL, 44100u, &changes) ||
        gp100_usb_device_set_sample_rate(&state, 44100u, NULL)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_reset_and_configuration();

    if (result != 0) {
        fprintf(stderr, "configuration test failed: %d\n", result);
        return 10 + result;
    }
    result = test_interface_lifecycle();
    if (result != 0) {
        fprintf(stderr, "interface lifecycle test failed: %d\n", result);
        return 20 + result;
    }
    result = test_sample_rates();
    if (result != 0) {
        fprintf(stderr, "sample-rate test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
