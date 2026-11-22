#include "usb_device_state.h"

#include <stddef.h>
#include <string.h>

void gp100_usb_device_state_reset(
    gp100_usb_device_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->sample_rate = GP100_USB_DEFAULT_SAMPLE_RATE;
    }
}

static uint32_t stream_stop_changes(
    const gp100_usb_device_state_t *state)
{
    uint32_t changes = GP100_USB_ENDPOINT_MIDI_STOP;

    if (state->alternate[GP100_USB_INTERFACE_CAPTURE] != 0u) {
        changes |= GP100_USB_ENDPOINT_CAPTURE_STOP;
    }
    if (state->alternate[GP100_USB_INTERFACE_PLAYBACK] != 0u) {
        changes |= GP100_USB_ENDPOINT_PLAYBACK_STOP;
    }
    return changes;
}

bool gp100_usb_device_set_configuration(
    gp100_usb_device_state_t *state,
    uint8_t configuration,
    uint32_t *endpoint_changes)
{
    uint32_t changes = GP100_USB_ENDPOINT_CHANGE_NONE;

    if (state == NULL || endpoint_changes == NULL ||
        configuration > GP100_USB_CONFIGURATION_ACTIVE) {
        return false;
    }
    if (state->configuration == configuration) {
        *endpoint_changes = changes;
        return true;
    }
    if (state->configuration == GP100_USB_CONFIGURATION_ACTIVE) {
        changes = stream_stop_changes(state);
    }
    state->configuration = configuration;
    memset(state->alternate, 0, sizeof(state->alternate));
    if (configuration == GP100_USB_CONFIGURATION_ACTIVE) {
        changes |= GP100_USB_ENDPOINT_MIDI_START;
    }
    *endpoint_changes = changes;
    return true;
}

static bool alternate_valid(uint8_t interface_number, uint8_t alternate)
{
    if (interface_number == GP100_USB_INTERFACE_CAPTURE ||
        interface_number == GP100_USB_INTERFACE_PLAYBACK) {
        return alternate <= 2u;
    }
    return alternate == 0u;
}

bool gp100_usb_device_set_interface(
    gp100_usb_device_state_t *state,
    uint8_t interface_number,
    uint8_t alternate_setting,
    uint32_t *endpoint_changes)
{
    uint8_t previous;
    uint32_t changes = GP100_USB_ENDPOINT_CHANGE_NONE;
    uint32_t start_change;
    uint32_t stop_change;

    if (state == NULL || endpoint_changes == NULL ||
        state->configuration != GP100_USB_CONFIGURATION_ACTIVE ||
        interface_number >= GP100_USB_INTERFACE_COUNT ||
        !alternate_valid(interface_number, alternate_setting)) {
        return false;
    }
    previous = state->alternate[interface_number];
    if (previous == alternate_setting) {
        *endpoint_changes = changes;
        return true;
    }

    if (interface_number == GP100_USB_INTERFACE_CAPTURE) {
        start_change = GP100_USB_ENDPOINT_CAPTURE_START;
        stop_change = GP100_USB_ENDPOINT_CAPTURE_STOP;
    } else if (interface_number == GP100_USB_INTERFACE_PLAYBACK) {
        start_change = GP100_USB_ENDPOINT_PLAYBACK_START;
        stop_change = GP100_USB_ENDPOINT_PLAYBACK_STOP;
    } else {
        return false;
    }
    if (previous != 0u) {
        changes |= stop_change;
    }
    if (alternate_setting != 0u) {
        changes |= start_change;
    }
    state->alternate[interface_number] = alternate_setting;
    *endpoint_changes = changes;
    return true;
}

bool gp100_usb_sample_rate_supported(uint32_t sample_rate)
{
    switch (sample_rate) {
    case 44100u:
    case 48000u:
    case 88200u:
    case 96000u:
    case 176400u:
    case 192000u:
        return true;
    default:
        return false;
    }
}

bool gp100_usb_device_set_sample_rate(
    gp100_usb_device_state_t *state,
    uint32_t sample_rate,
    uint32_t *endpoint_changes)
{
    if (state == NULL || endpoint_changes == NULL ||
        !gp100_usb_sample_rate_supported(sample_rate)) {
        return false;
    }
    *endpoint_changes = GP100_USB_ENDPOINT_CHANGE_NONE;
    if (state->sample_rate != sample_rate) {
        state->sample_rate = sample_rate;
        *endpoint_changes = GP100_USB_ENDPOINT_RATE_CHANGED;
    }
    return true;
}

bool gp100_usb_device_audio_format(
    const gp100_usb_device_state_t *state,
    uint8_t interface_number,
    gp100_usb_audio_format_t *format)
{
    uint8_t alternate;

    if (state == NULL || format == NULL ||
        state->configuration != GP100_USB_CONFIGURATION_ACTIVE ||
        (interface_number != GP100_USB_INTERFACE_CAPTURE &&
         interface_number != GP100_USB_INTERFACE_PLAYBACK)) {
        return false;
    }
    alternate = state->alternate[interface_number];
    if (alternate == 1u) {
        *format = GP100_USB_AUDIO_PCM16_STEREO;
        return true;
    }
    if (alternate == 2u) {
        *format = GP100_USB_AUDIO_PCM32_STEREO;
        return true;
    }
    return false;
}
