#ifndef GP100_USB_AUDIO_PACKET_H
#define GP100_USB_AUDIO_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_usb_audio_format {
    GP100_USB_AUDIO_PCM16_STEREO = 1,
    GP100_USB_AUDIO_PCM32_STEREO = 2
} gp100_usb_audio_format_t;

typedef struct gp100_usb_playback_controls {
    bool master_mute;
    bool left_mute;
    bool right_mute;
    uint16_t master_volume_8_8;
    uint16_t left_volume_8_8;
    uint16_t right_volume_8_8;
    int16_t current_master_q7;
    int16_t current_left_q7;
    int16_t current_right_q7;
} gp100_usb_playback_controls_t;

typedef struct gp100_usb_capture_controls {
    bool master_mute;
    bool left_mute;
    bool right_mute;
    uint16_t master_volume_8_8;
    uint16_t left_volume_8_8;
    uint16_t right_volume_8_8;
    int16_t current_master_q7;
    int16_t current_left_q7;
    int16_t current_right_q7;
} gp100_usb_capture_controls_t;

float gp100_usb_volume_db(uint16_t encoded_8_8);

void gp100_usb_playback_controls_reset(
    gp100_usb_playback_controls_t *controls);

void gp100_usb_capture_controls_reset(
    gp100_usb_capture_controls_t *controls);

/* Decodes one little-endian USB OUT packet to interleaved signed 24-bit data. */
bool gp100_usb_playback_decode_packet(
    gp100_usb_playback_controls_t *controls,
    gp100_usb_audio_format_t format,
    const uint8_t *packet,
    size_t packet_bytes,
    int32_t *interleaved_stereo,
    size_t output_capacity_frames,
    size_t *output_frames);

/* Applies capture controls and packs signed 24-bit data for USB IN. */
bool gp100_usb_capture_encode_packet(
    gp100_usb_capture_controls_t *controls,
    gp100_usb_audio_format_t format,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    uint8_t *packet,
    size_t packet_capacity_bytes,
    size_t *packet_bytes);

#ifdef __cplusplus
}
#endif

#endif
