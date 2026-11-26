#ifndef GP100_DEVICE_IDENTITY_H
#define GP100_DEVICE_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DEVICE_RECORD_FLASH_OFFSET UINT32_C(0x007ff000)
#define GP100_DEVICE_RECORD_SIZE 136u
#define GP100_DEVICE_ID_SIZE 8u
#define GP100_DEVICE_SUBSTITUTION_SIZE 128u

bool gp100_decode_device_identity(
    const uint8_t record[GP100_DEVICE_RECORD_SIZE],
    uint8_t identity[GP100_DEVICE_ID_SIZE]);

bool gp100_device_identity_matches(
    const uint8_t record[GP100_DEVICE_RECORD_SIZE],
    const uint8_t ocotp_identity[GP100_DEVICE_ID_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
