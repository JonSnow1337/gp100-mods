#ifndef GP100_USB_DEVICE_STATE_H
#define GP100_USB_DEVICE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb_audio_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_CONFIGURATION_NONE 0u
#define GP100_USB_CONFIGURATION_ACTIVE 1u
#define GP100_USB_INTERFACE_COUNT 4u
#define GP100_USB_INTERFACE_AUDIO_CONTROL 0u
#define GP100_USB_INTERFACE_CAPTURE 1u
#define GP100_USB_INTERFACE_PLAYBACK 2u
#define GP100_USB_INTERFACE_MIDI 3u
#define GP100_USB_DEFAULT_SAMPLE_RATE 44100u

typedef enum gp100_usb_endpoint_change {
    GP100_USB_ENDPOINT_CHANGE_NONE = 0u,
    GP100_USB_ENDPOINT_MIDI_START = 1u << 0,
    GP100_USB_ENDPOINT_MIDI_STOP = 1u << 1,
    GP100_USB_ENDPOINT_CAPTURE_START = 1u << 2,
    GP100_USB_ENDPOINT_CAPTURE_STOP = 1u << 3,
    GP100_USB_ENDPOINT_PLAYBACK_START = 1u << 4,
    GP100_USB_ENDPOINT_PLAYBACK_STOP = 1u << 5,
    GP100_USB_ENDPOINT_RATE_CHANGED = 1u << 6
} gp100_usb_endpoint_change_t;

typedef struct gp100_usb_device_state {
    uint32_t sample_rate;
    uint8_t configuration;
    uint8_t alternate[GP100_USB_INTERFACE_COUNT];
} gp100_usb_device_state_t;

void gp100_usb_device_state_reset(
    gp100_usb_device_state_t *state);

/* Models SET_CONFIGURATION 0/1 and reports endpoint work for the adapter. */
bool gp100_usb_device_set_configuration(
    gp100_usb_device_state_t *state,
    uint8_t configuration,
    uint32_t *endpoint_changes);

/* Models SET_INTERFACE. Audio streaming interfaces accept alternates 0..2. */
bool gp100_usb_device_set_interface(
    gp100_usb_device_state_t *state,
    uint8_t interface_number,
    uint8_t alternate_setting,
    uint32_t *endpoint_changes);

bool gp100_usb_device_set_sample_rate(
    gp100_usb_device_state_t *state,
    uint32_t sample_rate,
    uint32_t *endpoint_changes);

bool gp100_usb_sample_rate_supported(uint32_t sample_rate);

/* Alternate 1 is PCM16; alternate 2 is 24 significant bits in 32 bits. */
bool gp100_usb_device_audio_format(
    const gp100_usb_device_state_t *state,
    uint8_t interface_number,
    gp100_usb_audio_format_t *format);

#ifdef __cplusplus
}
#endif

#endif
