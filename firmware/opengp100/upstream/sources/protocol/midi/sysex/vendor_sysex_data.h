#ifndef GP100_VENDOR_SYSEX_DATA_H
#define GP100_VENDOR_SYSEX_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/preset_record.h"
#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_DATA_SESSION_BEGIN UINT32_C(0x12000000)
#define GP100_SYSEX_DATA_PRESET_STATUS UINT32_C(0x12000001)
#define GP100_SYSEX_DATA_SESSION_END UINT32_C(0x12000002)
#define GP100_SYSEX_DATA_SAVE_USER_PRESET UINT32_C(0x12000002)

#define GP100_SYSEX_DATA_READ_SELECTED_PRESET UINT32_C(0x13010000)
#define GP100_SYSEX_DATA_SELECTED_PRESET_READY UINT32_C(0x13010001)
#define GP100_SYSEX_DATA_READ_CURRENT_PRESET UINT32_C(0x13020000)
#define GP100_SYSEX_DATA_CURRENT_PRESET_READY UINT32_C(0x13020001)
#define GP100_SYSEX_DATA_RUNTIME_STATUS UINT32_C(0x13020007)

#define GP100_SYSEX_PRESET_TRANSFER_RAW_BYTES \
    GP100_PRESET_CRC_OFFSET
#define GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES \
    (GP100_SYSEX_PRESET_TRANSFER_RAW_BYTES * 2u)
#define GP100_SYSEX_PRESET_TRANSFER_CRC_BYTES 8u
#define GP100_SYSEX_PRESET_TRANSFER_BYTES \
    (GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES + \
     GP100_SYSEX_PRESET_TRANSFER_CRC_BYTES)
#define GP100_SYSEX_PRESET_TRANSFER_CHUNK_BYTES 0xc0u
#define GP100_SYSEX_PRESET_DESCRIPTOR_BYTES 6u
#define GP100_SYSEX_RUNTIME_STATUS_RAW_BYTES 10u
#define GP100_SYSEX_RUNTIME_STATUS_BYTES 28u
#define GP100_SYSEX_RUNTIME_STATUS_RESERVED UINT16_C(0x0012)

typedef struct gp100_sysex_runtime_status {
    uint16_t rhythm_enabled;
    uint16_t volume_pedal_value;
    uint16_t rhythm_volume;
    uint16_t preset_modified;
} gp100_sysex_runtime_status_t;

typedef struct gp100_sysex_save_preset_action {
    uint16_t selector;
    uint16_t user_index;
} gp100_sysex_save_preset_action_t;

typedef struct gp100_sysex_preset_descriptor {
    uint8_t bank;
    uint8_t program;
    uint16_t transfer_bytes;
    uint16_t chunk_bytes;
} gp100_sysex_preset_descriptor_t;

typedef enum gp100_sysex_data_result {
    GP100_SYSEX_DATA_INVALID = -1,
    GP100_SYSEX_DATA_UNHANDLED = 0,
    GP100_SYSEX_DATA_RESPONSE = 1,
    GP100_SYSEX_DATA_ACTION = 2
} gp100_sysex_data_result_t;

bool gp100_sysex_preset_location(
    uint16_t logical_index,
    uint8_t *bank,
    uint8_t *program);

gp100_sysex_data_result_t gp100_sysex_data_handle_control(
    const gp100_sysex_view_t *request,
    uint16_t current_logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

gp100_sysex_data_result_t gp100_sysex_data_decode_save_preset(
    const gp100_sysex_view_t *request,
    gp100_sysex_save_preset_action_t *action);

bool gp100_sysex_encode_preset_transfer(
    const gp100_preset_record_t *record,
    uint16_t logical_index,
    uint8_t *encoded,
    size_t encoded_capacity,
    size_t *encoded_length);

bool gp100_sysex_build_preset_descriptor(
    uint32_t request_address,
    uint16_t logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_decode_preset_descriptor(
    const uint8_t *payload,
    size_t payload_length,
    gp100_sysex_preset_descriptor_t *descriptor);

gp100_sysex_data_result_t gp100_sysex_data_build_runtime_status(
    const gp100_sysex_view_t *request,
    const gp100_sysex_runtime_status_t *status,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_data_decode_runtime_status(
    const uint8_t *payload,
    size_t payload_length,
    gp100_sysex_runtime_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
