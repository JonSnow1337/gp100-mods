#ifndef GP100_USB_AUDIO_LEVEL_H
#define GP100_USB_AUDIO_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_USB_AUDIO_GAIN_MIN_DB (-20)
#define GP100_USB_AUDIO_GAIN_MAX_DB 20

bool gp100_usb_audio_gain_for_db(int16_t gain_db, float *gain);

#ifdef __cplusplus
}
#endif

#endif
