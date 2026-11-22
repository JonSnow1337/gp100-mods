#ifndef GP100_USB_AUDIO_SCHEDULE_H
#define GP100_USB_AUDIO_SCHEDULE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_usb_audio_schedule {
    uint32_t sample_rate;
    uint16_t index;
} gp100_usb_audio_schedule_t;

void gp100_usb_audio_schedule_reset(
    gp100_usb_audio_schedule_t *schedule);

bool gp100_usb_audio_next_frame_count(
    gp100_usb_audio_schedule_t *schedule,
    uint32_t sample_rate,
    uint8_t *frame_count);

#ifdef __cplusplus
}
#endif

#endif
