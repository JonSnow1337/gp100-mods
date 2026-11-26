#ifndef GP100_USB_MIDI_PACKET_H
#define GP100_USB_MIDI_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_MIDI_EVENT_BYTES 4u
#define GP100_USB_MIDI_TRANSFER_BYTES 64u

size_t gp100_usb_midi_encoded_size(
    const uint8_t *message,
    size_t message_length);

/*
 * Encode one complete MIDI message into USB-MIDI 1.0 event packets. The caller
 * can split a long encoded SysEx range into 64-byte endpoint transfers.
 */
bool gp100_usb_midi_encode(
    uint8_t cable_number,
    const uint8_t *message,
    size_t message_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif
