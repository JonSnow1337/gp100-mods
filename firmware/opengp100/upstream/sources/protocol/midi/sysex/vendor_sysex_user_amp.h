#ifndef GP100_VENDOR_SYSEX_USER_AMP_H
#define GP100_VENDOR_SYSEX_USER_AMP_H

#include "vendor_sysex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define GP100_SYSEX_USER_AMP_DESCRIPTOR_READ UINT32_C(0x12001012)
#define GP100_SYSEX_USER_AMP_UPLOAD UINT32_C(0x12001013)
#define GP100_SYSEX_USER_AMP_RESET UINT32_C(0x12001014)
#define GP100_SYSEX_USER_AMP_NAME_UPDATE UINT32_C(0x12001015)
#define GP100_SYSEX_USER_AMP_METADATA_UPDATE \
    GP100_SYSEX_USER_AMP_NAME_UPDATE

#define GP100_SYSEX_USER_AMP_SLOT_COUNT 5u
#define GP100_SYSEX_USER_AMP_METADATA_BYTES 0x20u
#define GP100_SYSEX_USER_AMP_NAME_BYTES 0x10u
#define GP100_SYSEX_USER_AMP_EDITABLE_METADATA_BYTES \
    GP100_SYSEX_USER_AMP_NAME_BYTES
#define GP100_SYSEX_USER_AMP_BLOB_BYTES 0x2000u
#define GP100_SYSEX_USER_AMP_PACKAGE_BYTES 0x2020u
#define GP100_SYSEX_USER_AMP_DESCRIPTOR_BYTES 0x2cu
#define GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES 28u
#define GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT 588u
#define GP100_SYSEX_USER_AMP_UPLOAD_PADDING_NIBBLE 0x0fu
#define GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES 0x4040u
#define GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES 33u
#define GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES 0x38u
#define GP100_USER_AMP_DESCRIPTOR_FLASH_BASE UINT32_C(0x006c0000)
#define GP100_USER_AMP_BLOB_FLASH_BASE UINT32_C(0x006c1000)
#define GP100_USER_AMP_MODEL_MAGIC UINT32_C(0x49535456)
#define GP100_USER_AMP_MODEL_HEADER_BYTES 0x88u

typedef struct gp100_user_amp_model_layout
{
    uint32_t declared_bytes;
    uint16_t crc16;
    uint32_t payload_bytes;
    double input_biquad[5];
    double output_biquad[5];
    float positive_limit;
    float negative_limit;
    float positive_rate;
    float negative_rate;
    uint32_t first_bank_offset_words;
    uint32_t first_bank_value_count;
    uint32_t second_bank_offset_words;
    uint32_t second_bank_value_count;
} gp100_user_amp_model_layout_t;

typedef enum gp100_sysex_user_amp_result
{
    GP100_SYSEX_USER_AMP_INVALID = -1,
    GP100_SYSEX_USER_AMP_UNHANDLED = 0,
    GP100_SYSEX_USER_AMP_ACTION = 1,
    GP100_SYSEX_USER_AMP_RESPONSE = 2,
    GP100_SYSEX_USER_AMP_DONE = 3,
    GP100_SYSEX_USER_AMP_REJECTED = 4
} gp100_sysex_user_amp_result_t;

typedef struct gp100_sysex_user_amp_record
{
    bool present;
    uint8_t metadata[GP100_SYSEX_USER_AMP_METADATA_BYTES];
} gp100_sysex_user_amp_record_t;

typedef struct gp100_sysex_user_amp_reset_action
{
    uint8_t user_index;
} gp100_sysex_user_amp_reset_action_t;

typedef struct gp100_sysex_user_amp_name_action
{
    uint8_t user_index;
    uint8_t name[GP100_SYSEX_USER_AMP_NAME_BYTES];
} gp100_sysex_user_amp_name_action_t;

typedef struct gp100_sysex_user_amp_upload
{
    uint8_t *encoded;
    size_t capacity;
    uint16_t next_chunk;
    uint8_t user_index;
    bool active;
} gp100_sysex_user_amp_upload_t;

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_build_descriptor(
    const gp100_sysex_view_t *request,
    const gp100_sysex_user_amp_record_t *record, uint8_t *response,
    size_t response_capacity, size_t *response_length);

bool gp100_sysex_user_amp_decode_descriptor(
    const uint8_t *payload, size_t payload_length, uint8_t *user_index,
    gp100_sysex_user_amp_record_t *record);

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_decode_reset(
    const gp100_sysex_view_t *request,
    gp100_sysex_user_amp_reset_action_t *action);

gp100_sysex_user_amp_result_t
gp100_sysex_user_amp_decode_name_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_user_amp_name_action_t *action);

bool gp100_sysex_user_amp_apply_name(
    gp100_sysex_user_amp_record_t *record,
    const gp100_sysex_user_amp_name_action_t *action);

bool gp100_sysex_user_amp_apply_reset(
    gp100_sysex_user_amp_record_t *record,
    const gp100_sysex_user_amp_reset_action_t *action);

/* Build the absent in-memory descriptor produced for an erased slot. */
bool gp100_sysex_user_amp_make_factory_record(
    uint8_t user_index,
    gp100_sysex_user_amp_record_t *record);

bool gp100_sysex_user_amp_upload_init(
    gp100_sysex_user_amp_upload_t *upload, uint8_t *workspace,
    size_t workspace_capacity);

/* Build one official-editor-compatible 33-byte upload payload. */
bool gp100_sysex_user_amp_build_upload_payload(
    uint8_t user_index, uint16_t chunk_index,
    const uint8_t package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES],
    uint8_t payload[GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES]);

bool gp100_user_amp_flash_offsets(uint8_t user_index,
                                       uint32_t *descriptor_offset,
                                       uint32_t *blob_offset);

bool gp100_user_amp_build_descriptor_record(
    uint8_t user_index, bool present,
    const uint8_t metadata[GP100_SYSEX_USER_AMP_METADATA_BYTES],
    uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES]);

bool gp100_user_amp_validate_descriptor_record(
    uint8_t user_index,
    const uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES]);

bool gp100_user_amp_decode_descriptor_record(
    uint8_t user_index,
    const uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES],
    gp100_sysex_user_amp_record_t *decoded);

bool gp100_user_amp_model_layout(
    const uint8_t *blob, size_t blob_size,
    gp100_user_amp_model_layout_t *layout);

/*
 * Calculate the CRC-16/MODBUS value over model bytes +0x0c through the
 * declared end. The VTSI header stores the result high byte first at +0x08.
 */
bool gp100_user_amp_model_crc16(
    const uint8_t *blob, size_t blob_size, uint16_t *crc16);

/* Recalculate and store the high-byte-first CRC field at model offset +0x08. */
bool gp100_user_amp_model_update_crc16(
    uint8_t *blob, size_t blob_size);

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_handle_upload(
    gp100_sysex_user_amp_upload_t *upload,
    const gp100_sysex_view_t *request,
    uint8_t completed_package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES],
    uint8_t *completed_user_index, uint8_t *response, size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
