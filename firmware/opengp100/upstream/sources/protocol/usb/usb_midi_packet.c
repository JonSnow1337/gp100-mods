#include "usb_midi_packet.h"

#include <stddef.h>

static size_t channel_message_length(uint8_t status)
{
    uint8_t kind = status & 0xf0u;

    if (kind == 0xc0u || kind == 0xd0u) {
        return 2u;
    }
    if (kind >= 0x80u && kind <= 0xe0u) {
        return 3u;
    }
    return 0u;
}

size_t gp100_usb_midi_encoded_size(
    const uint8_t *message,
    size_t message_length)
{
    size_t expected;

    if (message == NULL || message_length == 0u) {
        return 0u;
    }
    expected = channel_message_length(message[0]);
    if (expected != 0u) {
        return message_length == expected
            ? GP100_USB_MIDI_EVENT_BYTES
            : 0u;
    }
    if (message[0] == 0xf0u && message_length >= 2u &&
        message[message_length - 1u] == 0xf7u) {
        return ((message_length + 2u) / 3u) *
               GP100_USB_MIDI_EVENT_BYTES;
    }
    return 0u;
}

static uint8_t sysex_cin(size_t remaining)
{
    if (remaining == 1u) {
        return 0x05u;
    }
    if (remaining == 2u) {
        return 0x06u;
    }
    if (remaining == 3u) {
        return 0x07u;
    }
    return 0x04u;
}

bool gp100_usb_midi_encode(
    uint8_t cable_number,
    const uint8_t *message,
    size_t message_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    size_t required;
    size_t input_offset = 0u;
    size_t output_offset = 0u;
    size_t channel_length;

    if (output_length != NULL) {
        *output_length = 0u;
    }
    if (cable_number > 0x0fu || message == NULL || output == NULL ||
        output_length == NULL) {
        return false;
    }

    required = gp100_usb_midi_encoded_size(message, message_length);
    if (required == 0u || output_capacity < required) {
        *output_length = required;
        return false;
    }

    channel_length = channel_message_length(message[0]);
    if (channel_length != 0u) {
        output[0] = (uint8_t)((cable_number << 4) | (message[0] >> 4));
        output[1] = message[0];
        output[2] = message[1];
        output[3] = channel_length == 3u ? message[2] : 0u;
        *output_length = GP100_USB_MIDI_EVENT_BYTES;
        return true;
    }

    while (input_offset < message_length) {
        size_t remaining = message_length - input_offset;
        size_t packet_bytes = remaining < 3u ? remaining : 3u;
        size_t packet_offset;

        output[output_offset] = (uint8_t)(
            (cable_number << 4) | sysex_cin(remaining));
        for (packet_offset = 0u; packet_offset < 3u; ++packet_offset) {
            output[output_offset + 1u + packet_offset] =
                packet_offset < packet_bytes
                    ? message[input_offset + packet_offset]
                    : 0u;
        }
        input_offset += packet_bytes;
        output_offset += GP100_USB_MIDI_EVENT_BYTES;
    }

    *output_length = output_offset;
    return true;
}
