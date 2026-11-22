#ifndef GP100_USB_DESCRIPTORS_H
#define GP100_USB_DESCRIPTORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_VENDOR_ID UINT16_C(0x84ef)
#define GP100_USB_PRODUCT_ID UINT16_C(0x0021)
#define GP100_USB_DEVICE_DESCRIPTOR_BYTES 18u
#define GP100_USB_CONFIGURATION_DESCRIPTOR_BYTES 413u
#define GP100_USB_STRING_COUNT 7u

#define GP100_USB_AUDIO_CAPTURE_ENDPOINT UINT8_C(0x82)
#define GP100_USB_AUDIO_PLAYBACK_ENDPOINT UINT8_C(0x02)
#define GP100_USB_AUDIO_CAPTURE_FEATURE UINT8_C(0x09)
#define GP100_USB_AUDIO_PLAYBACK_FEATURE UINT8_C(0x0c)
#define GP100_USB_MIDI_IN_ENDPOINT UINT8_C(0x83)
#define GP100_USB_MIDI_OUT_ENDPOINT UINT8_C(0x03)

typedef struct gp100_usb_descriptor_view {
    const uint8_t *bytes;
    size_t length;
} gp100_usb_descriptor_view_t;

/* Exact high-speed descriptors initialized by the factory scatter record. */
gp100_usb_descriptor_view_t gp100_usb_device_descriptor(void);
gp100_usb_descriptor_view_t gp100_usb_configuration_descriptor(void);

/* UAC2 clock-source GET_RANGE response from the initialized factory image. */
gp100_usb_descriptor_view_t gp100_usb_clock_rate_range(void);

/*
 * Builds USB string descriptor 0..6 as UTF-16LE. Index zero is the
 * LANGID descriptor (US English). Returns false for an unknown index or a
 * short output buffer. The required size is always reported when non-null.
 */
bool gp100_usb_build_string_descriptor(
    uint8_t index,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif
