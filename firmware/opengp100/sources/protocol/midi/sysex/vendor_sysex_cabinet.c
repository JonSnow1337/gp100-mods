#include "vendor_sysex_cabinet.h"

#include "storage/preset_record.h"

#include <string.h>

#define METADATA_NIBBLE_BYTES \
    (GP100_SYSEX_CABINET_METADATA_BYTES * 2u)
#define INFO_PREFIX_BYTES (1u + METADATA_NIBBLE_BYTES + 2u)

uint8_t gp100_cabinet_impulse_nibble_checksum(
    const uint8_t record[GP100_CABINET_RECORD_SIZE])
{
    uint8_t checksum = 0u;
    size_t index;

    if (record == NULL) {
        return 0u;
    }
    for (index = GP100_SYSEX_CABINET_METADATA_BYTES;
         index < GP100_CABINET_RECORD_SIZE;
         ++index) {
        checksum = (uint8_t)(checksum +
                             (record[index] >> 4) +
                             (record[index] & 0x0fu));
    }
    return checksum;
}

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_decode_metadata_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_cabinet_metadata_action_t *action)
{
    if (request == NULL || action == NULL) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_CABINET_METADATA_UPDATE) {
        return GP100_SYSEX_CABINET_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length != 4u + GP100_SYSEX_CABINET_METADATA_BYTES ||
        !gp100_sysex_decode_u16(
            request->payload, &action->user_index) ||
        action->user_index >= GP100_CABINET_USER_COUNT) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    memcpy(action->metadata,
           request->payload + 4u,
           sizeof(action->metadata));
    return GP100_SYSEX_CABINET_ACTION;
}

bool gp100_sysex_cabinet_apply_metadata(
    uint8_t record[GP100_CABINET_RECORD_SIZE],
    const gp100_sysex_cabinet_metadata_action_t *action)
{
    if (record == NULL || action == NULL ||
        action->user_index >= GP100_CABINET_USER_COUNT) {
        return false;
    }
    memcpy(record, action->metadata, sizeof(action->metadata));
    return true;
}

bool gp100_sysex_cabinet_apply_reset(
    uint8_t record[GP100_CABINET_RECORD_SIZE],
    const gp100_sysex_cabinet_reset_action_t *action)
{
    if (record == NULL || action == NULL ||
        action->user_index >= GP100_CABINET_USER_COUNT) {
        return false;
    }
    /* V2.1.0 writes its zero-initialized 32-byte default metadata buffer and
     * preserves the impulse body. The UI supplies localized "User IR" names
     * separately rather than storing them in this record. */
    memset(record, 0, GP100_SYSEX_CABINET_METADATA_BYTES);
    return true;
}

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_decode_reset(
    const gp100_sysex_view_t *request,
    gp100_sysex_cabinet_reset_action_t *action)
{
    if (request == NULL || action == NULL) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_CABINET_RESET) {
        return GP100_SYSEX_CABINET_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 4u ||
        !gp100_sysex_decode_u16(
            request->payload, &action->user_index) ||
        action->user_index >= GP100_CABINET_USER_COUNT) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    return GP100_SYSEX_CABINET_ACTION;
}

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_build_info(
    const gp100_sysex_view_t *request,
    const uint8_t record[GP100_CABINET_RECORD_SIZE],
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_CABINET_INFO_BYTES];
    uint8_t checksum;
    uint32_t crc;
    size_t encoded_length;

    if (request == NULL || record == NULL || response_length == NULL) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        request->address != GP100_SYSEX_CABINET_METADATA_READ) {
        return GP100_SYSEX_CABINET_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 1u ||
        request->payload[0] >= GP100_CABINET_USER_COUNT) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    payload[0] = request->payload[0];
    if (!gp100_sysex_nibble_encode(
            record,
            GP100_SYSEX_CABINET_METADATA_BYTES,
            payload + 1u,
            METADATA_NIBBLE_BYTES,
            &encoded_length) ||
        encoded_length != METADATA_NIBBLE_BYTES) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    checksum = gp100_cabinet_impulse_nibble_checksum(record);
    payload[1u + METADATA_NIBBLE_BYTES] = (uint8_t)(checksum >> 4);
    payload[2u + METADATA_NIBBLE_BYTES] = (uint8_t)(checksum & 0x0fu);
    crc = gp100_crc32(payload, INFO_PREFIX_BYTES);
    gp100_sysex_encode_u32(crc, payload + INFO_PREFIX_BYTES);
    if (!gp100_sysex_build_response(
            request->address,
            payload,
            sizeof(payload),
            response,
            response_capacity,
            response_length)) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    return GP100_SYSEX_CABINET_RESPONSE;
}

bool gp100_sysex_cabinet_decode_info(
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *user_index,
    uint8_t metadata[GP100_SYSEX_CABINET_METADATA_BYTES],
    uint8_t *impulse_checksum)
{
    uint32_t expected_crc;
    size_t decoded_length;

    if (payload == NULL || user_index == NULL || metadata == NULL ||
        impulse_checksum == NULL ||
        payload_length != GP100_SYSEX_CABINET_INFO_BYTES ||
        payload[0] >= GP100_CABINET_USER_COUNT ||
        !gp100_sysex_decode_u32(
            payload + INFO_PREFIX_BYTES, &expected_crc) ||
        expected_crc != gp100_crc32(payload, INFO_PREFIX_BYTES) ||
        !gp100_sysex_nibble_decode(
            payload + 1u,
            METADATA_NIBBLE_BYTES,
            metadata,
            GP100_SYSEX_CABINET_METADATA_BYTES,
            &decoded_length) ||
        decoded_length != GP100_SYSEX_CABINET_METADATA_BYTES) {
        return false;
    }
    *user_index = payload[0];
    *impulse_checksum = (uint8_t)(
        (payload[1u + METADATA_NIBBLE_BYTES] << 4) |
        payload[2u + METADATA_NIBBLE_BYTES]);
    return true;
}

bool gp100_sysex_cabinet_upload_init(
    gp100_sysex_cabinet_upload_t *upload,
    uint8_t *workspace,
    size_t workspace_capacity)
{
    if (upload == NULL || workspace == NULL ||
        workspace_capacity < GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES) {
        return false;
    }
    upload->encoded = workspace;
    upload->capacity = workspace_capacity;
    upload->next_chunk = 0u;
    upload->user_index = 0u;
    upload->active = false;
    return true;
}

bool gp100_sysex_cabinet_build_upload_payload(
    uint8_t user_index,
    uint16_t chunk_index,
    const uint8_t record[GP100_CABINET_RECORD_SIZE],
    uint8_t payload[GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES])
{
    size_t encoded_offset;
    size_t index;
    uint8_t checksum = 0u;

    if (record == NULL || payload == NULL ||
        user_index >= GP100_CABINET_USER_COUNT ||
        chunk_index >= GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT) {
        return false;
    }
    payload[0] = user_index;
    payload[1] = (uint8_t)(chunk_index >> 7u);
    payload[2] = (uint8_t)(chunk_index & 0x7fu);
    encoded_offset =
        (size_t)chunk_index * GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
    for (index = 0u;
         index < GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
         ++index) {
        size_t nibble_index = encoded_offset + index;
        uint8_t nibble = GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE;

        if (nibble_index < GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES) {
            uint8_t value = record[nibble_index / 2u];
            nibble = (nibble_index & 1u) == 0u
                         ? (uint8_t)(value >> 4u)
                         : (uint8_t)(value & 0x0fu);
        }
        payload[3u + index] = nibble;
        checksum = (uint8_t)(checksum + nibble);
    }
    payload[31] = (uint8_t)(checksum >> 4u);
    payload[32] = (uint8_t)(checksum & 0x0fu);
    return true;
}

static uint16_t decode_u14(const uint8_t bytes[2])
{
    return (uint16_t)(((uint16_t)bytes[0] << 7) | bytes[1]);
}

static gp100_sysex_cabinet_result_t build_upload_ack(
    const gp100_sysex_view_t *request,
    bool accepted,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[4];

    payload[0] = request->payload[0];
    payload[1] = request->payload[1];
    payload[2] = request->payload[2];
    payload[3] = accepted ? 1u : 0u;
    if (!gp100_sysex_build_response(
            request->address,
            payload,
            sizeof(payload),
            response,
            response_capacity,
            response_length)) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    return accepted ? GP100_SYSEX_CABINET_RESPONSE
                    : GP100_SYSEX_CABINET_REJECTED;
}

gp100_sysex_cabinet_result_t
gp100_sysex_cabinet_handle_upload(
    gp100_sysex_cabinet_upload_t *upload,
    const gp100_sysex_view_t *request,
    uint8_t completed_record[GP100_CABINET_RECORD_SIZE],
    uint8_t *completed_user_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint16_t chunk_index;
    uint8_t actual_checksum = 0u;
    uint8_t expected_checksum;
    size_t copy_length;
    size_t offset;
    size_t index;
    size_t decoded_length;
    gp100_sysex_cabinet_result_t result;

    if (upload == NULL || request == NULL || response_length == NULL ||
        upload->encoded == NULL ||
        upload->capacity < GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_CABINET_METADATA_READ) {
        return GP100_SYSEX_CABINET_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length !=
            GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES ||
        request->payload[0] >= GP100_CABINET_USER_COUNT ||
        request->payload[1] > 0x7fu || request->payload[2] > 0x7fu ||
        request->payload[31] > 0x0fu || request->payload[32] > 0x0fu) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    chunk_index = decode_u14(request->payload + 1u);
    if (chunk_index >= GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT) {
        return GP100_SYSEX_CABINET_INVALID;
    }
    for (index = 0u;
         index < GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
         ++index) {
        uint8_t nibble = request->payload[3u + index];
        if (nibble > 0x0fu) {
            return GP100_SYSEX_CABINET_INVALID;
        }
        actual_checksum = (uint8_t)(actual_checksum + nibble);
    }
    expected_checksum = (uint8_t)(
        (request->payload[31] << 4) | request->payload[32]);
    if (actual_checksum != expected_checksum) {
        return build_upload_ack(
            request, false,
            response, response_capacity, response_length);
    }
    if (chunk_index == 0u) {
        upload->next_chunk = 0u;
        upload->user_index = request->payload[0];
        upload->active = true;
    }
    if (!upload->active || upload->user_index != request->payload[0] ||
        chunk_index != upload->next_chunk) {
        return build_upload_ack(
            request, false,
            response, response_capacity, response_length);
    }
    offset = (size_t)chunk_index *
             GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
    copy_length = GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES - offset;
    if (copy_length > GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES) {
        copy_length = GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
    }
    memcpy(upload->encoded + offset, request->payload + 3u, copy_length);
    ++upload->next_chunk;
    result = build_upload_ack(
        request, true, response, response_capacity, response_length);
    if (result != GP100_SYSEX_CABINET_RESPONSE) {
        return result;
    }
    if (upload->next_chunk < GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT) {
        return GP100_SYSEX_CABINET_RESPONSE;
    }
    if (completed_record == NULL || completed_user_index == NULL ||
        !gp100_sysex_nibble_decode(
            upload->encoded,
            GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES,
            completed_record,
            GP100_CABINET_RECORD_SIZE,
            &decoded_length) ||
        decoded_length != GP100_CABINET_RECORD_SIZE) {
        upload->active = false;
        return GP100_SYSEX_CABINET_INVALID;
    }
    *completed_user_index = upload->user_index;
    upload->active = false;
    return GP100_SYSEX_CABINET_DONE;
}
