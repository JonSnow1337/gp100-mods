#include "vendor_sysex_data.h"

#include <string.h>

static void encode_crc_most_significant_nibble_first(
    uint32_t crc,
    uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_CRC_BYTES])
{
    size_t index;

    for (index = 0u;
         index < GP100_SYSEX_PRESET_TRANSFER_CRC_BYTES;
         ++index) {
        unsigned shift = (unsigned)(28u - index * 4u);
        encoded[index] = (uint8_t)((crc >> shift) & 0x0fu);
    }
}

static uint16_t decode_u14(const uint8_t encoded[2])
{
    return (uint16_t)(((uint16_t)encoded[0] << 7u) | encoded[1]);
}

static void encode_u14(uint16_t value, uint8_t encoded[2])
{
    encoded[0] = (uint8_t)(value >> 7u);
    encoded[1] = (uint8_t)(value & 0x7fu);
}

bool gp100_sysex_preset_location(
    uint16_t logical_index,
    uint8_t *bank,
    uint8_t *program)
{
    if (bank == NULL || program == NULL ||
        logical_index >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    *bank = (uint8_t)(logical_index / GP100_PRESETS_PER_BANK);
    *program = (uint8_t)(logical_index % GP100_PRESETS_PER_BANK);
    return true;
}

gp100_sysex_data_result_t gp100_sysex_data_handle_control(
    const gp100_sysex_view_t *request,
    uint16_t current_logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[3];
    size_t payload_length;

    if (request == NULL || response_length == NULL) {
        return GP100_SYSEX_DATA_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS) {
        return GP100_SYSEX_DATA_UNHANDLED;
    }
    switch (request->address) {
    case GP100_SYSEX_DATA_SESSION_BEGIN:
        payload[0] = 1u;
        payload[1] = 0u;
        payload_length = 2u;
        break;
    case GP100_SYSEX_DATA_PRESET_STATUS:
        payload[0] = 1u;
        if (!gp100_sysex_preset_location(
                current_logical_index, &payload[1], &payload[2])) {
            return GP100_SYSEX_DATA_INVALID;
        }
        payload_length = 3u;
        break;
    case GP100_SYSEX_DATA_SESSION_END:
        payload_length = 0u;
        break;
    default:
        return GP100_SYSEX_DATA_UNHANDLED;
    }
    if (request->payload_length != 0u ||
        !gp100_sysex_build_response(
            request->address,
            payload_length == 0u ? NULL : payload,
            payload_length,
            response,
            response_capacity,
            response_length)) {
        return GP100_SYSEX_DATA_INVALID;
    }
    return GP100_SYSEX_DATA_RESPONSE;
}

gp100_sysex_data_result_t gp100_sysex_data_decode_save_preset(
    const gp100_sysex_view_t *request,
    gp100_sysex_save_preset_action_t *action)
{
    if (request == NULL || action == NULL) {
        return GP100_SYSEX_DATA_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        request->address != GP100_SYSEX_DATA_SAVE_USER_PRESET) {
        return GP100_SYSEX_DATA_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 8u ||
        !gp100_sysex_decode_u16(
            request->payload, &action->selector) ||
        !gp100_sysex_decode_u16(
            request->payload + 4u, &action->user_index) ||
        action->user_index >= GP100_PRESETS_PER_BANK) {
        return GP100_SYSEX_DATA_INVALID;
    }
    return GP100_SYSEX_DATA_ACTION;
}

bool gp100_sysex_encode_preset_transfer(
    const gp100_preset_record_t *record,
    uint16_t logical_index,
    uint8_t *encoded,
    size_t encoded_capacity,
    size_t *encoded_length)
{
    uint8_t raw[GP100_SYSEX_PRESET_TRANSFER_RAW_BYTES];
    uint32_t crc;
    size_t data_length;

    if (encoded_length != NULL) {
        *encoded_length = GP100_SYSEX_PRESET_TRANSFER_BYTES;
    }
    if (record == NULL || encoded == NULL || encoded_length == NULL ||
        logical_index >= GP100_TOTAL_LOGICAL_PRESETS ||
        encoded_capacity < GP100_SYSEX_PRESET_TRANSFER_BYTES) {
        return false;
    }
    memcpy(raw, record->bytes, sizeof(raw));
    raw[0] = (uint8_t)logical_index;
    raw[1] = (uint8_t)(logical_index >> 8);
    if (!gp100_sysex_nibble_encode(
            raw,
            sizeof(raw),
            encoded,
            GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES,
            &data_length) ||
        data_length != GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES) {
        return false;
    }
    crc = gp100_crc32(encoded, data_length);
    encode_crc_most_significant_nibble_first(
        crc, encoded + data_length);
    return true;
}

bool gp100_sysex_build_preset_descriptor(
    uint32_t request_address,
    uint16_t logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_PRESET_DESCRIPTOR_BYTES];
    uint32_t response_address;

    if (request_address == GP100_SYSEX_DATA_READ_SELECTED_PRESET) {
        response_address = GP100_SYSEX_DATA_SELECTED_PRESET_READY;
    } else if (request_address ==
               GP100_SYSEX_DATA_READ_CURRENT_PRESET) {
        response_address = GP100_SYSEX_DATA_CURRENT_PRESET_READY;
    } else {
        return false;
    }
    if (!gp100_sysex_preset_location(
            logical_index, &payload[0], &payload[1])) {
        return false;
    }
    encode_u14(GP100_SYSEX_PRESET_TRANSFER_BYTES, &payload[2]);
    encode_u14(GP100_SYSEX_PRESET_TRANSFER_CHUNK_BYTES, &payload[4]);
    return gp100_sysex_build_response(
        response_address,
        payload,
        sizeof(payload),
        response,
        response_capacity,
        response_length);
}

bool gp100_sysex_decode_preset_descriptor(
    const uint8_t *payload,
    size_t payload_length,
    gp100_sysex_preset_descriptor_t *descriptor)
{
    uint16_t logical_index;

    if (payload == NULL || descriptor == NULL ||
        payload_length != GP100_SYSEX_PRESET_DESCRIPTOR_BYTES ||
        payload[2] > 0x7fu || payload[3] > 0x7fu ||
        payload[4] > 0x7fu || payload[5] > 0x7fu ||
        payload[1] >= GP100_PRESETS_PER_BANK) {
        return false;
    }
    logical_index = (uint16_t)(
        (uint16_t)payload[0] * GP100_PRESETS_PER_BANK + payload[1]);
    if (logical_index >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    descriptor->bank = payload[0];
    descriptor->program = payload[1];
    descriptor->transfer_bytes = decode_u14(&payload[2]);
    descriptor->chunk_bytes = decode_u14(&payload[4]);
    return descriptor->transfer_bytes != 0u && descriptor->chunk_bytes != 0u;
}

static void store_u16_le(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static uint16_t load_u16_le(const uint8_t input[2])
{
    return (uint16_t)((uint16_t)input[0] |
                      ((uint16_t)input[1] << 8));
}

gp100_sysex_data_result_t gp100_sysex_data_build_runtime_status(
    const gp100_sysex_view_t *request,
    const gp100_sysex_runtime_status_t *status,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t raw[GP100_SYSEX_RUNTIME_STATUS_RAW_BYTES];
    uint8_t payload[GP100_SYSEX_RUNTIME_STATUS_BYTES];
    uint32_t crc;
    size_t encoded_length;

    if (request == NULL || status == NULL || response_length == NULL) {
        return GP100_SYSEX_DATA_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        request->address != GP100_SYSEX_DATA_RUNTIME_STATUS) {
        return GP100_SYSEX_DATA_UNHANDLED;
    }
    if (request->payload_length != 0u) {
        return GP100_SYSEX_DATA_INVALID;
    }
    store_u16_le(raw, status->rhythm_enabled);
    store_u16_le(raw + 2u, status->volume_pedal_value);
    store_u16_le(raw + 4u, status->rhythm_volume);
    store_u16_le(raw + 6u, status->preset_modified);
    store_u16_le(raw + 8u, GP100_SYSEX_RUNTIME_STATUS_RESERVED);
    if (!gp100_sysex_nibble_encode(
            raw, sizeof(raw), payload, sizeof(raw) * 2u,
            &encoded_length) ||
        encoded_length != sizeof(raw) * 2u) {
        return GP100_SYSEX_DATA_INVALID;
    }
    crc = gp100_crc32(payload, encoded_length);
    encode_crc_most_significant_nibble_first(
        crc, payload + encoded_length);
    if (!gp100_sysex_build_response(
            request->address, payload, sizeof(payload),
            response, response_capacity, response_length)) {
        return GP100_SYSEX_DATA_INVALID;
    }
    return GP100_SYSEX_DATA_RESPONSE;
}

bool gp100_sysex_data_decode_runtime_status(
    const uint8_t *payload,
    size_t payload_length,
    gp100_sysex_runtime_status_t *status)
{
    uint8_t raw[GP100_SYSEX_RUNTIME_STATUS_RAW_BYTES];
    uint32_t expected_crc = 0u;
    uint32_t actual_crc;
    size_t raw_length;
    size_t index;

    if (payload == NULL || status == NULL ||
        payload_length != GP100_SYSEX_RUNTIME_STATUS_BYTES) {
        return false;
    }
    for (index = sizeof(raw) * 2u; index < payload_length; ++index) {
        if (payload[index] > 0x0fu) {
            return false;
        }
        expected_crc = (expected_crc << 4) | payload[index];
    }
    actual_crc = gp100_crc32(payload, sizeof(raw) * 2u);
    if (expected_crc != actual_crc ||
        !gp100_sysex_nibble_decode(
            payload, sizeof(raw) * 2u,
            raw, sizeof(raw), &raw_length) ||
        raw_length != sizeof(raw) ||
        load_u16_le(raw + 8u) !=
            GP100_SYSEX_RUNTIME_STATUS_RESERVED) {
        return false;
    }
    status->rhythm_enabled = load_u16_le(raw);
    status->volume_pedal_value = load_u16_le(raw + 2u);
    status->rhythm_volume = load_u16_le(raw + 4u);
    status->preset_modified = load_u16_le(raw + 6u);
    return true;
}
