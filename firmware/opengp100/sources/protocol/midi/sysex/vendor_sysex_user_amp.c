#include "vendor_sysex_user_amp.h"

#include <string.h>

#define DESCRIPTOR_TAG UINT32_C(0x002c1012)
#define STORAGE_DESCRIPTOR_MAGIC UINT32_C(0x5a0000a5)

static void store_u16_le(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static uint16_t load_u16_le(const uint8_t input[2])
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint16_t load_u16_be(const uint8_t input[2])
{
    return (uint16_t)(((uint16_t)input[0] << 8) | (uint16_t)input[1]);
}

static void store_u16_be(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)(value >> 8u);
    output[1] = (uint8_t)value;
}

static void store_u32_le(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
    output[2] = (uint8_t)(value >> 16u);
    output[3] = (uint8_t)(value >> 24u);
}

static uint32_t load_u32_le(const uint8_t input[4])
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) |
           ((uint32_t)input[2] << 16u) | ((uint32_t)input[3] << 24u);
}

static uint64_t load_u64_le(const uint8_t input[8])
{
    return (uint64_t)load_u32_le(input) |
           ((uint64_t)load_u32_le(input + 4u) << 32u);
}

static float load_f32_le(const uint8_t input[4])
{
    uint32_t bits = load_u32_le(input);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double load_f64_le(const uint8_t input[8])
{
    uint64_t bits = load_u64_le(input);
    double value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t descriptor_checksum(const uint8_t *record)
{
    uint32_t checksum = 0u;
    size_t index;

    for (index = 0u; index < 0x34u; ++index)
        checksum += record[index];
    return checksum;
}

bool gp100_user_amp_model_crc16(
    const uint8_t *blob, size_t blob_size, uint16_t *crc16)
{
    uint32_t declared_bytes;
    uint16_t crc = UINT16_C(0xffff);
    size_t index;
    unsigned bit;

    if (blob == NULL || crc16 == NULL || blob_size < 0x0cu)
    {
        return false;
    }
    declared_bytes = load_u32_le(blob + 0x04u);
    if (declared_bytes < 0x0cu || declared_bytes > blob_size)
    {
        return false;
    }
    for (index = 0x0cu; index < declared_bytes; ++index)
    {
        crc ^= blob[index];
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc & 1u) != 0u
                      ? (uint16_t)((crc >> 1u) ^ UINT16_C(0xa001))
                      : (uint16_t)(crc >> 1u);
        }
    }
    *crc16 = crc;
    return true;
}

bool gp100_user_amp_model_update_crc16(
    uint8_t *blob, size_t blob_size)
{
    uint16_t crc16;

    if (!gp100_user_amp_model_crc16(blob, blob_size, &crc16))
    {
        return false;
    }
    store_u16_be(blob + 0x08u, crc16);
    return true;
}

bool gp100_user_amp_model_layout(
    const uint8_t *blob, size_t blob_size,
    gp100_user_amp_model_layout_t *layout)
{
    gp100_user_amp_model_layout_t candidate;
    uint16_t computed_crc16;
    uint64_t first_end;
    uint64_t second_end;
    size_t index;

    if (blob == NULL || layout == NULL || sizeof(float) != 4u ||
        sizeof(double) != 8u ||
        blob_size < GP100_USER_AMP_MODEL_HEADER_BYTES ||
        load_u32_le(blob) != GP100_USER_AMP_MODEL_MAGIC ||
        blob[0x12u] != 0u || blob[0x13u] != 0u)
    {
        return false;
    }
    candidate.declared_bytes = load_u32_le(blob + 0x04u);
    candidate.crc16 = load_u16_be(blob + 0x08u);
    candidate.payload_bytes = load_u32_le(blob + 0x14u);
    if (candidate.declared_bytes < GP100_USER_AMP_MODEL_HEADER_BYTES ||
        candidate.declared_bytes > blob_size ||
        candidate.payload_bytes !=
            candidate.declared_bytes - GP100_USER_AMP_MODEL_HEADER_BYTES ||
        !gp100_user_amp_model_crc16(blob, blob_size, &computed_crc16) ||
        candidate.crc16 != computed_crc16)
    {
        return false;
    }
    for (index = 0u; index < 5u; ++index)
    {
        candidate.input_biquad[index] = load_f64_le(blob + 0x18u + index * 8u);
        candidate.output_biquad[index] = load_f64_le(blob + 0x40u + index * 8u);
    }
    candidate.positive_limit = load_f32_le(blob + 0x68u);
    candidate.negative_limit = load_f32_le(blob + 0x6cu);
    candidate.positive_rate = load_f32_le(blob + 0x70u);
    candidate.negative_rate = load_f32_le(blob + 0x74u);
    candidate.first_bank_offset_words = load_u32_le(blob + 0x78u);
    candidate.first_bank_value_count = load_u32_le(blob + 0x7cu);
    candidate.second_bank_offset_words = load_u32_le(blob + 0x80u);
    candidate.second_bank_value_count = load_u32_le(blob + 0x84u);
    first_end = GP100_USER_AMP_MODEL_HEADER_BYTES +
                ((uint64_t)candidate.first_bank_offset_words +
                 candidate.first_bank_value_count) *
                    4u;
    second_end = GP100_USER_AMP_MODEL_HEADER_BYTES +
                 ((uint64_t)candidate.second_bank_offset_words +
                  candidate.second_bank_value_count) *
                     4u;
    if (first_end > candidate.declared_bytes ||
        second_end > candidate.declared_bytes ||
        candidate.second_bank_value_count > 0x400u)
    {
        return false;
    }
    *layout = candidate;
    return true;
}

bool gp100_user_amp_flash_offsets(uint8_t user_index,
                                       uint32_t *descriptor_offset,
                                       uint32_t *blob_offset)
{
    if (user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT ||
        descriptor_offset == NULL || blob_offset == NULL)
    {
        return false;
    }
    *descriptor_offset =
        GP100_USER_AMP_DESCRIPTOR_FLASH_BASE +
        (uint32_t)user_index * GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES;
    *blob_offset = GP100_USER_AMP_BLOB_FLASH_BASE +
                   (uint32_t)user_index * GP100_SYSEX_USER_AMP_BLOB_BYTES;
    return true;
}

bool gp100_user_amp_build_descriptor_record(
    uint8_t user_index, bool present,
    const uint8_t metadata[GP100_SYSEX_USER_AMP_METADATA_BYTES],
    uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES])
{
    uint32_t descriptor_offset;
    uint32_t blob_offset;

    if (metadata == NULL || record == NULL ||
        !gp100_user_amp_flash_offsets(user_index, &descriptor_offset,
                                           &blob_offset))
    {
        return false;
    }
    memset(record, 0, GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES);
    store_u32_le(record, STORAGE_DESCRIPTOR_MAGIC);
    store_u32_le(record + 4u, user_index);
    store_u32_le(record + 8u, present ? 1u : 0u);
    memcpy(record + 0x0cu, metadata, GP100_SYSEX_USER_AMP_METADATA_BYTES);
    store_u32_le(record + 0x2cu, descriptor_offset);
    store_u32_le(record + 0x30u, blob_offset);
    store_u32_le(record + 0x34u, descriptor_checksum(record));
    return true;
}

bool gp100_user_amp_validate_descriptor_record(
    uint8_t user_index,
    const uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES])
{
    uint32_t descriptor_offset;
    uint32_t blob_offset;

    return record != NULL &&
           gp100_user_amp_flash_offsets(user_index, &descriptor_offset,
                                             &blob_offset) &&
           load_u32_le(record) == STORAGE_DESCRIPTOR_MAGIC &&
           load_u32_le(record + 4u) == user_index &&
           load_u32_le(record + 8u) <= 1u &&
           load_u32_le(record + 0x2cu) == descriptor_offset &&
           load_u32_le(record + 0x30u) == blob_offset &&
           load_u32_le(record + 0x34u) == descriptor_checksum(record);
}

bool gp100_user_amp_decode_descriptor_record(
    uint8_t user_index,
    const uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES],
    gp100_sysex_user_amp_record_t *decoded)
{
    if (decoded == NULL ||
        !gp100_user_amp_validate_descriptor_record(user_index, record))
    {
        return false;
    }
    decoded->present = load_u32_le(record + 8u) != 0u;
    memcpy(decoded->metadata, record + 0x0cu, sizeof(decoded->metadata));
    return true;
}

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_build_descriptor(
    const gp100_sysex_view_t *request,
    const gp100_sysex_user_amp_record_t *record, uint8_t *response,
    size_t response_capacity, size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_USER_AMP_DESCRIPTOR_BYTES] = {0};

    if (request == NULL || record == NULL || response_length == NULL)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        request->address != GP100_SYSEX_USER_AMP_DESCRIPTOR_READ)
    {
        return GP100_SYSEX_USER_AMP_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 1u ||
        request->payload[0] >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    payload[0] = (uint8_t)DESCRIPTOR_TAG;
    payload[1] = (uint8_t)(DESCRIPTOR_TAG >> 8);
    payload[2] = (uint8_t)(DESCRIPTOR_TAG >> 16);
    payload[3] = (uint8_t)(DESCRIPTOR_TAG >> 24);
    store_u16_le(payload + 4u, request->payload[0]);
    store_u16_le(payload + 6u, record->present ? 0u : 1u);
    if (record->present)
    {
        memcpy(payload + 12u, record->metadata, sizeof(record->metadata));
    }
    if (!gp100_sysex_build_response(
            GP100_SYSEX_USER_AMP_DESCRIPTOR_READ, payload, sizeof(payload),
            response, response_capacity, response_length))
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    return GP100_SYSEX_USER_AMP_RESPONSE;
}

bool gp100_sysex_user_amp_decode_descriptor(
    const uint8_t *payload, size_t payload_length, uint8_t *user_index,
    gp100_sysex_user_amp_record_t *record)
{
    uint32_t tag;
    uint16_t slot;
    uint16_t absent;

    if (payload == NULL || user_index == NULL || record == NULL ||
        payload_length != GP100_SYSEX_USER_AMP_DESCRIPTOR_BYTES)
    {
        return false;
    }
    tag = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
          ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    slot = load_u16_le(payload + 4u);
    absent = load_u16_le(payload + 6u);
    if (tag != DESCRIPTOR_TAG || slot >= GP100_SYSEX_USER_AMP_SLOT_COUNT ||
        absent > 1u || payload[8] != 0u || payload[9] != 0u ||
        payload[10] != 0u || payload[11] != 0u)
    {
        return false;
    }
    *user_index = (uint8_t)slot;
    record->present = absent == 0u;
    memcpy(record->metadata, payload + 12u, sizeof(record->metadata));
    return true;
}

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_decode_reset(
    const gp100_sysex_view_t *request,
    gp100_sysex_user_amp_reset_action_t *action)
{
    uint8_t slot;

    if (request == NULL || action == NULL)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_USER_AMP_RESET)
    {
        return GP100_SYSEX_USER_AMP_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 2u ||
        request->payload[0] > 0x0fu || request->payload[1] > 0x0fu)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    slot = (uint8_t)((request->payload[0] << 4) | request->payload[1]);
    if (slot >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    action->user_index = slot;
    return GP100_SYSEX_USER_AMP_ACTION;
}

gp100_sysex_user_amp_result_t
gp100_sysex_user_amp_decode_name_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_user_amp_name_action_t *action)
{
    uint16_t slot;

    if (request == NULL || action == NULL)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_USER_AMP_NAME_UPDATE)
    {
        return GP100_SYSEX_USER_AMP_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length !=
            4u + GP100_SYSEX_USER_AMP_NAME_BYTES ||
        !gp100_sysex_decode_u16(request->payload, &slot) ||
        slot >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    action->user_index = (uint8_t)slot;
    memcpy(action->name, request->payload + 4u, sizeof(action->name));
    return GP100_SYSEX_USER_AMP_ACTION;
}

bool gp100_sysex_user_amp_apply_name(
    gp100_sysex_user_amp_record_t *record,
    const gp100_sysex_user_amp_name_action_t *action)
{
    if (record == NULL || action == NULL ||
        action->user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return false;
    }
    memset(record->metadata, 0, sizeof(record->metadata));
    memcpy(record->metadata, action->name, sizeof(action->name));
    return true;
}

bool gp100_sysex_user_amp_apply_reset(
    gp100_sysex_user_amp_record_t *record,
    const gp100_sysex_user_amp_reset_action_t *action)
{
    if (record == NULL || action == NULL ||
        action->user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return false;
    }
    return gp100_sysex_user_amp_make_factory_record(
        action->user_index, record);
}

bool gp100_sysex_user_amp_make_factory_record(
    uint8_t user_index,
    gp100_sysex_user_amp_record_t *record)
{
    static const char default_names[GP100_SYSEX_USER_AMP_SLOT_COUNT][12] =
        {"SnapTone1", "SnapTone2", "SnapTone3", "SnapTone4", "SnapTone5"};

    if (record == NULL ||
        user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
    {
        return false;
    }
    record->present = false;
    memset(record->metadata, 0, sizeof(record->metadata));
    memcpy(record->metadata, default_names[user_index],
           sizeof(default_names[user_index]));
    return true;
}

bool gp100_sysex_user_amp_upload_init(
    gp100_sysex_user_amp_upload_t *upload, uint8_t *workspace,
    size_t workspace_capacity)
{
    if (upload == NULL || workspace == NULL ||
        workspace_capacity < GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES)
    {
        return false;
    }
    upload->encoded = workspace;
    upload->capacity = workspace_capacity;
    upload->next_chunk = 0u;
    upload->user_index = 0u;
    upload->active = false;
    return true;
}

bool gp100_sysex_user_amp_build_upload_payload(
    uint8_t user_index, uint16_t chunk_index,
    const uint8_t package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES],
    uint8_t payload[GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES])
{
    size_t encoded_offset;
    size_t index;
    uint8_t checksum = 0u;

    if (package == NULL || payload == NULL ||
        user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT ||
        chunk_index >= GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT)
    {
        return false;
    }
    payload[0] = user_index;
    payload[1] = (uint8_t)(chunk_index >> 7u);
    payload[2] = (uint8_t)(chunk_index & 0x7fu);
    encoded_offset =
        (size_t)chunk_index * GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
    for (index = 0u;
         index < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
         ++index)
    {
        size_t nibble_index = encoded_offset + index;
        uint8_t nibble = GP100_SYSEX_USER_AMP_UPLOAD_PADDING_NIBBLE;

        if (nibble_index < GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES)
        {
            uint8_t value = package[nibble_index / 2u];
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

static gp100_sysex_user_amp_result_t
build_upload_ack(const gp100_sysex_view_t *request, bool accepted,
                 uint8_t *response, size_t response_capacity,
                 size_t *response_length)
{
    uint8_t payload[4];

    payload[0] = request->payload[0];
    payload[1] = request->payload[1];
    payload[2] = request->payload[2];
    payload[3] = accepted ? 1u : 0u;
    if (!gp100_sysex_build_response(request->address, payload,
                                         sizeof(payload), response,
                                         response_capacity, response_length))
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    return accepted ? GP100_SYSEX_USER_AMP_RESPONSE
                    : GP100_SYSEX_USER_AMP_REJECTED;
}

gp100_sysex_user_amp_result_t gp100_sysex_user_amp_handle_upload(
    gp100_sysex_user_amp_upload_t *upload,
    const gp100_sysex_view_t *request,
    uint8_t completed_package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES],
    uint8_t *completed_user_index, uint8_t *response, size_t response_capacity,
    size_t *response_length)
{
    uint16_t chunk_index;
    uint8_t actual_checksum = 0u;
    uint8_t expected_checksum;
    size_t copy_length;
    size_t offset;
    size_t index;
    size_t decoded_length;
    gp100_sysex_user_amp_result_t result;

    if (upload == NULL || request == NULL || response_length == NULL ||
        upload->encoded == NULL ||
        upload->capacity < GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_USER_AMP_UPLOAD)
    {
        return GP100_SYSEX_USER_AMP_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length !=
            GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES ||
        request->payload[0] >= GP100_SYSEX_USER_AMP_SLOT_COUNT ||
        request->payload[1] > 0x7fu || request->payload[2] > 0x7fu ||
        request->payload[31] > 0x0fu || request->payload[32] > 0x0fu)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    chunk_index = decode_u14(request->payload + 1u);
    if (chunk_index >= GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT)
    {
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    for (index = 0u; index < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
         ++index)
    {
        uint8_t nibble = request->payload[3u + index];
        if (nibble > 0x0fu)
        {
            return GP100_SYSEX_USER_AMP_INVALID;
        }
        actual_checksum = (uint8_t)(actual_checksum + nibble);
    }
    expected_checksum =
        (uint8_t)((request->payload[31] << 4) | request->payload[32]);
    if (actual_checksum != expected_checksum)
    {
        return build_upload_ack(request, false, response, response_capacity,
                                response_length);
    }
    if (chunk_index == 0u)
    {
        upload->next_chunk = 0u;
        upload->user_index = request->payload[0];
        upload->active = true;
    }
    if (!upload->active || upload->user_index != request->payload[0] ||
        chunk_index != upload->next_chunk)
    {
        return build_upload_ack(request, false, response, response_capacity,
                                response_length);
    }
    offset =
        (size_t)chunk_index * GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
    copy_length = GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES - offset;
    if (copy_length > GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES)
    {
        copy_length = GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
    }
    memcpy(upload->encoded + offset, request->payload + 3u, copy_length);
    ++upload->next_chunk;
    result = build_upload_ack(request, true, response, response_capacity,
                              response_length);
    if (result != GP100_SYSEX_USER_AMP_RESPONSE)
    {
        return result;
    }
    if (upload->next_chunk < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT)
    {
        return GP100_SYSEX_USER_AMP_RESPONSE;
    }
    if (completed_package == NULL || completed_user_index == NULL ||
        !gp100_sysex_nibble_decode(
            upload->encoded, GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES,
            completed_package, GP100_SYSEX_USER_AMP_PACKAGE_BYTES,
            &decoded_length) ||
        decoded_length != GP100_SYSEX_USER_AMP_PACKAGE_BYTES)
    {
        upload->active = false;
        return GP100_SYSEX_USER_AMP_INVALID;
    }
    *completed_user_index = upload->user_index;
    upload->active = false;
    return GP100_SYSEX_USER_AMP_DONE;
}
