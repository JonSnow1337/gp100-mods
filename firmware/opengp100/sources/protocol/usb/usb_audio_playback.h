#ifndef GP100_USB_AUDIO_PLAYBACK_H
#define GP100_USB_AUDIO_PLAYBACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb_audio_level.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_PLAYBACK_RING_FRAMES 320u

typedef struct gp100_usb_playback_ring {
    int32_t left[GP100_USB_PLAYBACK_RING_FRAMES];
    int32_t right[GP100_USB_PLAYBACK_RING_FRAMES];
    size_t write_index;
    size_t read_index;
    uint16_t phase;
    float current_gain;
    bool started;
} gp100_usb_playback_ring_t;

typedef enum gp100_usb_playback_read_result {
    GP100_USB_PLAYBACK_READ_ERROR = -1,
    GP100_USB_PLAYBACK_READ_BUFFERING = 0,
    GP100_USB_PLAYBACK_READ_AUDIO = 1
} gp100_usb_playback_read_result_t;

void gp100_usb_playback_reset(
    gp100_usb_playback_ring_t *playback);

/* Clear a live stream's history while keeping its product-level gain slew. */
void gp100_usb_playback_clear_buffer(
    gp100_usb_playback_ring_t *playback);

/* Publishes converted USB OUT samples in the factory's 24-bit value domain. */
bool gp100_usb_playback_publish(
    gp100_usb_playback_ring_t *playback,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    int16_t gain_db);

size_t gp100_usb_playback_available(
    const gp100_usb_playback_ring_t *playback);

gp100_usb_playback_read_result_t gp100_usb_playback_read(
    gp100_usb_playback_ring_t *playback,
    int32_t *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
