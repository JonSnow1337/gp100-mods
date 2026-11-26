#ifndef GP100_USB_AUDIO_CAPTURE_H
#define GP100_USB_AUDIO_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb_audio_level.h"
#include "protocol/midi/sysex/vendor_sysex_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_CAPTURE_RING_FRAMES 320u
#define GP100_USB_CAPTURE_GAIN_MIN_DB GP100_USB_AUDIO_GAIN_MIN_DB
#define GP100_USB_CAPTURE_GAIN_MAX_DB GP100_USB_AUDIO_GAIN_MAX_DB

typedef struct gp100_usb_capture_ring {
    int32_t left[GP100_USB_CAPTURE_RING_FRAMES];
    int32_t right[GP100_USB_CAPTURE_RING_FRAMES];
    size_t write_index;
    size_t read_index;
    uint16_t phase;
    float current_gain;
    bool started;
} gp100_usb_capture_ring_t;

typedef enum gp100_usb_capture_read_result {
    GP100_USB_CAPTURE_READ_ERROR = -1,
    GP100_USB_CAPTURE_READ_BUFFERING = 0,
    GP100_USB_CAPTURE_READ_AUDIO = 1
} gp100_usb_capture_read_result_t;

void gp100_usb_capture_reset(
    gp100_usb_capture_ring_t *capture);

/* Clear a live stream's history while keeping its product-level gain slew. */
void gp100_usb_capture_clear_buffer(
    gp100_usb_capture_ring_t *capture);

bool gp100_usb_capture_gain_for_db(
    int16_t gain_db,
    float *gain);

/* Publishes interleaved signed samples in the factory's 24-bit value domain. */
bool gp100_usb_capture_publish(
    gp100_usb_capture_ring_t *capture,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    int16_t gain_db);

/* Builds the device-to-host stereo stream from the independently selectable
 * factory Dry/Effect sources. The output may alias either input. */
bool gp100_usb_capture_select_sources(
    const int32_t *dry_interleaved_stereo,
    const int32_t *effect_interleaved_stereo,
    int32_t *output_interleaved_stereo,
    size_t frame_count,
    gp100_usb_record_source_t left_source,
    gp100_usb_record_source_t right_source);

size_t gp100_usb_capture_available(
    const gp100_usb_capture_ring_t *capture);

/* Reads interleaved stereo frames with the factory occupancy correction. */
gp100_usb_capture_read_result_t gp100_usb_capture_read(
    gp100_usb_capture_ring_t *capture,
    int32_t *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
