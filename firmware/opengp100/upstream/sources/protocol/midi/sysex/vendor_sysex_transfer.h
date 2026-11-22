#ifndef GP100_VENDOR_SYSEX_TRANSFER_H
#define GP100_VENDOR_SYSEX_TRANSFER_H

#include <stddef.h>
#include <stdint.h>

#include "vendor_sysex_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_TRANSFER_CHUNK_BYTES \
    GP100_SYSEX_PRESET_TRANSFER_CHUNK_BYTES
#define GP100_SYSEX_TRANSFER_PREFIX_BYTES 4u

#define GP100_SYSEX_SELECTED_START UINT32_C(0x13010002)
#define GP100_SYSEX_SELECTED_CHUNK UINT32_C(0x13010003)
#define GP100_SYSEX_SELECTED_ADVANCE UINT32_C(0x13010004)
#define GP100_SYSEX_SELECTED_COMPLETE UINT32_C(0x13010005)
#define GP100_SYSEX_SELECTED_FINAL UINT32_C(0x13010006)

#define GP100_SYSEX_CURRENT_START UINT32_C(0x13020002)
#define GP100_SYSEX_CURRENT_CHUNK UINT32_C(0x13020003)
#define GP100_SYSEX_CURRENT_ADVANCE UINT32_C(0x13020004)
#define GP100_SYSEX_CURRENT_COMPLETE UINT32_C(0x13020005)
#define GP100_SYSEX_CURRENT_FINAL UINT32_C(0x13020006)

typedef enum gp100_sysex_transfer_result {
    GP100_SYSEX_TRANSFER_INVALID = -1,
    GP100_SYSEX_TRANSFER_UNHANDLED = 0,
    GP100_SYSEX_TRANSFER_RESPONSE = 1,
    GP100_SYSEX_TRANSFER_DONE = 2,
    GP100_SYSEX_TRANSFER_REJECTED = 3
} gp100_sysex_transfer_result_t;

typedef struct gp100_sysex_transfer {
    const uint8_t *data;
    size_t data_length;
    uint16_t chunk_count;
    uint16_t next_chunk;
} gp100_sysex_transfer_t;

typedef struct gp100_sysex_upload {
    uint8_t *data;
    size_t capacity;
    uint16_t received_chunks;
    uint16_t received_mask;
    uint8_t bank;
    uint8_t program;
    bool active;
    bool selected;
} gp100_sysex_upload_t;

bool gp100_sysex_transfer_init(
    gp100_sysex_transfer_t *transfer,
    const uint8_t *data,
    size_t data_length);

size_t gp100_sysex_transfer_chunk_size(
    const gp100_sysex_transfer_t *transfer,
    uint16_t chunk_index);

gp100_sysex_transfer_result_t
gp100_sysex_transfer_handle_download(
    gp100_sysex_transfer_t *transfer,
    const gp100_sysex_view_t *request,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_upload_init(
    gp100_sysex_upload_t *upload,
    uint8_t *data,
    size_t capacity);

gp100_sysex_transfer_result_t
gp100_sysex_transfer_handle_upload(
    gp100_sysex_upload_t *upload,
    const gp100_sysex_view_t *request,
    gp100_preset_record_t *completed_record,
    uint16_t *completed_logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
