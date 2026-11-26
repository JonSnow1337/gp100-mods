#ifndef GP100_VENDOR_SYSEX_SETTINGS_H
#define GP100_VENDOR_SYSEX_SETTINGS_H

#include "storage/settings_record.h"
#include "vendor_sysex.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_SETTINGS_SNAPSHOT UINT32_C(0x13000000)
#define GP100_SYSEX_SETTINGS_ENCODED_BYTES \
    (GP100_SETTINGS_PAYLOAD_SIZE * 2u)
#define GP100_SYSEX_SETTINGS_CRC_BYTES 8u
#define GP100_SYSEX_SETTINGS_SNAPSHOT_BYTES \
    (GP100_SYSEX_SETTINGS_ENCODED_BYTES + \
     GP100_SYSEX_SETTINGS_CRC_BYTES)

typedef enum gp100_sysex_settings_result {
    GP100_SYSEX_SETTINGS_INVALID = -1,
    GP100_SYSEX_SETTINGS_UNHANDLED = 0,
    GP100_SYSEX_SETTINGS_RESPONSE = 1
} gp100_sysex_settings_result_t;

gp100_sysex_settings_result_t gp100_sysex_settings_build_snapshot(
    const gp100_sysex_view_t *request,
    const gp100_settings_record_t *settings,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_settings_decode_snapshot(
    const uint8_t *payload,
    size_t payload_length,
    gp100_settings_record_t *settings);

#ifdef __cplusplus
}
#endif

#endif
