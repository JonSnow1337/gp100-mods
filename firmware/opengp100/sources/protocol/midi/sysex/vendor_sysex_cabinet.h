#ifndef GP100_VENDOR_SYSEX_CABINET_H
#define GP100_VENDOR_SYSEX_CABINET_H

#include "audio/effects/cab/cabinet.h"
#include "vendor_sysex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_CABINET_METADATA_UPDATE UINT32_C(0x12001000)
#define GP100_SYSEX_CABINET_RESET UINT32_C(0x12001001)
#define GP100_SYSEX_CABINET_METADATA_READ UINT32_C(0x12001002)
#define GP100_SYSEX_CABINET_METADATA_BYTES 32u
#define GP100_SYSEX_CABINET_INFO_BYTES 75u
#define GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES 28u
#define GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT 295u
#define GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE 0x0fu
#define GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES \
    (GP100_CABINET_RECORD_SIZE * 2u)
#define GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES 33u

typedef enum gp100_sysex_cabinet_result {
    GP100_SYSEX_CABINET_INVALID = -1,
    GP100_SYSEX_CABINET_UNHANDLED = 0,
    GP100_SYSEX_CABINET_ACTION = 1,
    GP100_SYSEX_CABINET_RESPONSE = 2,
    GP100_SYSEX_CABINET_DONE = 3,
    GP100_SYSEX_CABINET_REJECTED = 4
} gp100_sysex_cabinet_result_t;

typedef struct gp100_sysex_cabinet_metadata_action {
    uint16_t user_index;
    uint8_t metadata[GP100_SYSEX_CABINET_METADATA_BYTES];
} gp100_sysex_cabinet_metadata_action_t;

typedef struct gp100_sysex_cabinet_reset_action {
    uint16_t user_index;
} gp100_sysex_cabinet_reset_action_t;

typedef struct gp100_sysex_cabinet_upload {
    uint8_t *encoded;
    size_t capacity;
    uint16_t next_chunk;
    uint8_t user_index;
    bool active;
} gp100_sysex_cabinet_upload_t;

uint8_t gp100_cabinet_impulse_nibble_checksum(
    const uint8_t record[GP100_CABINET_RECORD_SIZE]);

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_decode_metadata_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_cabinet_metadata_action_t *action);

bool gp100_sysex_cabinet_apply_metadata(
    uint8_t record[GP100_CABINET_RECORD_SIZE],
    const gp100_sysex_cabinet_metadata_action_t *action);

bool gp100_sysex_cabinet_apply_reset(
    uint8_t record[GP100_CABINET_RECORD_SIZE],
    const gp100_sysex_cabinet_reset_action_t *action);

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_decode_reset(
    const gp100_sysex_view_t *request,
    gp100_sysex_cabinet_reset_action_t *action);

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_build_info(
    const gp100_sysex_view_t *request,
    const uint8_t record[GP100_CABINET_RECORD_SIZE],
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_cabinet_decode_info(
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *user_index,
    uint8_t metadata[GP100_SYSEX_CABINET_METADATA_BYTES],
    uint8_t *impulse_checksum);

bool gp100_sysex_cabinet_upload_init(
    gp100_sysex_cabinet_upload_t *upload,
    uint8_t *workspace,
    size_t workspace_capacity);

/* Build one official-editor-compatible 33-byte upload payload. */
bool gp100_sysex_cabinet_build_upload_payload(
    uint8_t user_index,
    uint16_t chunk_index,
    const uint8_t record[GP100_CABINET_RECORD_SIZE],
    uint8_t payload[GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES]);

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_handle_upload(
    gp100_sysex_cabinet_upload_t *upload,
    const gp100_sysex_view_t *request,
    uint8_t completed_record[GP100_CABINET_RECORD_SIZE],
    uint8_t *completed_user_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
