#include "usb_audio_schedule.h"

#include <stddef.h>

void gp100_usb_audio_schedule_reset(
    gp100_usb_audio_schedule_t *schedule)
{
    if (schedule != NULL) {
        schedule->sample_rate = 0u;
        schedule->index = 0u;
    }
}

static uint16_t schedule_period(uint32_t sample_rate)
{
    switch (sample_rate) {
    case 48000u:
    case 96000u:
    case 192000u:
        return 1u;
    case 88200u:
        return 40u;
    case 176400u:
        return 20u;
    case 44100u:
    default:
        return 80u;
    }
}

static uint8_t scheduled_frames(uint32_t sample_rate, uint16_t index)
{
    switch (sample_rate) {
    case 48000u:
        return 6u;
    case 96000u:
        return 12u;
    case 192000u:
        return 24u;
    case 88200u:
        return index == 39u ? 12u : 11u;
    case 176400u:
        return index == 19u ? 23u : 22u;
    case 44100u:
    default:
        if (index >= 78u) {
            return 6u;
        }
        return (index & 1u) == 0u ? 5u : 6u;
    }
}

bool gp100_usb_audio_next_frame_count(
    gp100_usb_audio_schedule_t *schedule,
    uint32_t sample_rate,
    uint8_t *frame_count)
{
    uint16_t period;

    if (schedule == NULL || frame_count == NULL) {
        return false;
    }
    period = schedule_period(sample_rate);
    if (schedule->sample_rate != sample_rate) {
        schedule->sample_rate = sample_rate;
        schedule->index = 0u;
    } else if (schedule->index >= period) {
        schedule->index = 0u;
    }

    *frame_count = scheduled_frames(sample_rate, schedule->index);
    ++schedule->index;
    if (schedule->index >= period) {
        schedule->index = 0u;
    }
    return true;
}
