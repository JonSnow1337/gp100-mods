#include "vendor_sysex_transfer.h"

#include "vendor_sysex_data.h"

#include <limits.h>
#include <string.h>

static bool transfer_addresses(
    uint32_t request_address,
    bool *start,
    uint32_t *chunk_address,
    uint32_t *complete_address)
{
    if (request_address == GP100_SYSEX_SELECTED_START ||
        request_address == GP100_SYSEX_SELECTED_ADVANCE) {
        *start = request_address == GP100_SYSEX_SELECTED_START;
        *chunk_address = GP100_SYSEX_SELECTED_CHUNK;
        *complete_address = GP100_SYSEX_SELECTED_COMPLETE;
        return true;
    }
    if (request_address == GP100_SYSEX_CURRENT_START ||
        request_address == GP100_SYSEX_CURRENT_ADVANCE) {
        *start = request_address == GP100_SYSEX_CURRENT_START;
        *chunk_address = GP100_SYSEX_CURRENT_CHUNK;
        *complete_address = GP100_SYSEX_CURRENT_COMPLETE;
        return true;
    }
    return false;
}

static uint16_t decode_u14(const uint8_t encoded[2])
{
    return (uint16_t)(((uint16_t)encoded[0] << 7) | encoded[1]);
}

static void encode_u14(uint16_t value, uint8_t encoded[2])
{
    encoded[0] = (uint8_t)(value >> 7);
    encoded[1] = (uint8_t)(value & 0x7fu);
}

static bool upload_addresses(
    uint32_t request_address,
    bool *selected,
    unsigned *stage,
    uint32_t *response_address)
{
    switch (request_address) {
    case GP100_SYSEX_DATA_SELECTED_PRESET_READY:
        *selected = true;
        *stage = 0u;
        /* Both factory upload negotiations acknowledge on the current page. */
        *response_address = GP100_SYSEX_CURRENT_START;
        return true;
    case GP100_SYSEX_SELECTED_CHUNK:
        *selected = true;
        *stage = 1u;
        *response_address = GP100_SYSEX_SELECTED_ADVANCE;
        return true;
    case GP100_SYSEX_SELECTED_COMPLETE:
        *selected = true;
        *stage = 2u;
        *response_address = GP100_SYSEX_SELECTED_FINAL;
        return true;
    case GP100_SYSEX_DATA_CURRENT_PRESET_READY:
        *selected = false;
        *stage = 0u;
        *response_address = GP100_SYSEX_CURRENT_START;
        return true;
    case GP100_SYSEX_CURRENT_CHUNK:
        *selected = false;
        *stage = 1u;
        *response_address = GP100_SYSEX_CURRENT_ADVANCE;
        return true;
    case GP100_SYSEX_CURRENT_COMPLETE:
        *selected = false;
        *stage = 2u;
        *response_address = GP100_SYSEX_CURRENT_FINAL;
        return true;
    default:
        return false;
    }
}

bool gp100_sysex_transfer_init(
    gp100_sysex_transfer_t *transfer,
    const uint8_t *data,
    size_t data_length)
{
    size_t chunk_count;

    if (transfer == NULL || data == NULL || data_length == 0u ||
        data_length > SIZE_MAX -
                      (GP100_SYSEX_TRANSFER_CHUNK_BYTES - 1u)) {
        return false;
    }
    chunk_count = (data_length +
                   GP100_SYSEX_TRANSFER_CHUNK_BYTES - 1u) /
                  GP100_SYSEX_TRANSFER_CHUNK_BYTES;
    if (chunk_count > 0x3fffu) {
        return false;
    }
    transfer->data = data;
    transfer->data_length = data_length;
    transfer->chunk_count = (uint16_t)chunk_count;
    transfer->next_chunk = 0u;
    return true;
}

size_t gp100_sysex_transfer_chunk_size(
    const gp100_sysex_transfer_t *transfer,
    uint16_t chunk_index)
{
    size_t offset;
    size_t remaining;

    if (transfer == NULL || transfer->data == NULL ||
        chunk_index >= transfer->chunk_count) {
        return 0u;
    }
    offset = (size_t)chunk_index *
             GP100_SYSEX_TRANSFER_CHUNK_BYTES;
    remaining = transfer->data_length - offset;
    return remaining < GP100_SYSEX_TRANSFER_CHUNK_BYTES
               ? remaining
               : GP100_SYSEX_TRANSFER_CHUNK_BYTES;
}

gp100_sysex_transfer_result_t
gp100_sysex_transfer_handle_download(
    gp100_sysex_transfer_t *transfer,
    const gp100_sysex_view_t *request,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_TRANSFER_PREFIX_BYTES +
                    GP100_SYSEX_TRANSFER_CHUNK_BYTES];
    uint32_t chunk_address;
    uint32_t complete_address;
    size_t payload_length = GP100_SYSEX_TRANSFER_PREFIX_BYTES;
    uint16_t chunk_index;
    uint16_t response_index;
    bool complete;
    bool start;

    if (transfer == NULL || request == NULL || response_length == NULL ||
        transfer->data == NULL || transfer->chunk_count == 0u) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !transfer_addresses(
            request->address, &start, &chunk_address, &complete_address)) {
        return GP100_SYSEX_TRANSFER_UNHANDLED;
    }
    if ((start && request->payload_length != 2u) ||
        (!start && request->payload_length != 5u) ||
        request->payload == NULL) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    /* START sends chunk zero without changing the saved advance cursor. */
    chunk_index = start ? 0u : transfer->next_chunk;
    if (!start && request->payload[4] != 0u &&
        chunk_index < transfer->chunk_count) {
        ++chunk_index;
    }
    complete = chunk_index >= transfer->chunk_count;
    response_index = complete ? transfer->chunk_count : chunk_index;

    payload[0] = request->payload[0];
    payload[1] = request->payload[1];
    encode_u14(response_index, &payload[2]);
    if (!complete) {
        size_t chunk_size = gp100_sysex_transfer_chunk_size(
            transfer, chunk_index);
        size_t offset = (size_t)chunk_index *
                        GP100_SYSEX_TRANSFER_CHUNK_BYTES;
        memcpy(payload + payload_length, transfer->data + offset, chunk_size);
        payload_length += chunk_size;
    }

    if (!gp100_sysex_build_response(
            complete ? complete_address : chunk_address, payload,
            payload_length, response, response_capacity, response_length)) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    if (!start) {
        transfer->next_chunk = chunk_index;
    }
    return complete ? GP100_SYSEX_TRANSFER_DONE
                    : GP100_SYSEX_TRANSFER_RESPONSE;
}

bool gp100_sysex_upload_init(
    gp100_sysex_upload_t *upload,
    uint8_t *data,
    size_t capacity)
{
    if (upload == NULL || data == NULL ||
        capacity < GP100_SYSEX_PRESET_TRANSFER_BYTES) {
        return false;
    }
    upload->data = data;
    upload->capacity = capacity;
    upload->received_chunks = 0u;
    upload->received_mask = 0u;
    upload->bank = 0u;
    upload->program = 0u;
    upload->active = false;
    upload->selected = false;
    return true;
}

static bool finish_upload(
    const gp100_sysex_upload_t *upload,
    gp100_preset_record_t *record,
    uint16_t *logical_index)
{
    uint8_t decoded[GP100_SYSEX_PRESET_TRANSFER_RAW_BYTES];
    uint32_t expected_crc = 0u;
    uint32_t actual_crc;
    size_t decoded_length;
    size_t index;

    if (upload->received_chunks != 9u || upload->received_mask != 0x1ffu) {
        return false;
    }
    actual_crc = gp100_crc32(
        upload->data, GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES);
    for (index = 0u;
         index < GP100_SYSEX_PRESET_TRANSFER_CRC_BYTES;
         ++index) {
        uint8_t nibble = upload->data[
            GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES + index];
        if (nibble > 0x0fu) {
            return false;
        }
        expected_crc = (expected_crc << 4) | nibble;
    }
    if (expected_crc != actual_crc ||
        !gp100_sysex_nibble_decode(
            upload->data,
            GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES,
            decoded,
            sizeof(decoded),
            &decoded_length) ||
        decoded_length != sizeof(decoded)) {
        return false;
    }
    *logical_index = (uint16_t)((uint16_t)decoded[0] |
                                (uint16_t)((uint16_t)decoded[1] << 8));
    if (*logical_index >= GP100_TOTAL_LOGICAL_PRESETS ||
        *logical_index !=
            (uint16_t)((uint16_t)upload->bank *
                       GP100_PRESETS_PER_BANK + upload->program)) {
        return false;
    }
    memcpy(record->bytes, decoded, sizeof(decoded));
    gp100_preset_update_crc(record);
    return true;
}

gp100_sysex_transfer_result_t
gp100_sysex_transfer_handle_upload(
    gp100_sysex_upload_t *upload,
    const gp100_sysex_view_t *request,
    gp100_preset_record_t *completed_record,
    uint16_t *completed_logical_index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[5];
    uint32_t response_address;
    uint16_t chunk_index;
    size_t chunk_size;
    size_t offset;
    bool selected;
    bool valid;
    unsigned stage;

    if (upload == NULL || request == NULL || response_length == NULL ||
        upload->data == NULL ||
        upload->capacity < GP100_SYSEX_PRESET_TRANSFER_BYTES) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !upload_addresses(
            request->address, &selected, &stage, &response_address)) {
        return GP100_SYSEX_TRANSFER_UNHANDLED;
    }
    if (request->payload == NULL) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    if (stage == 0u) {
        if (request->payload_length != 6u || request->payload[0] > 1u ||
            request->payload[1] >= GP100_PRESETS_PER_BANK ||
            request->payload[2] > 0x7fu ||
            request->payload[3] > 0x7fu ||
            request->payload[4] > 0x7fu ||
            request->payload[5] > 0x7fu ||
            decode_u14(&request->payload[2]) !=
                GP100_SYSEX_PRESET_TRANSFER_BYTES ||
            decode_u14(&request->payload[4]) !=
                GP100_SYSEX_TRANSFER_CHUNK_BYTES) {
            return GP100_SYSEX_TRANSFER_INVALID;
        }
        upload->bank = request->payload[0];
        upload->program = request->payload[1];
        upload->selected = selected;
        upload->received_chunks = 0u;
        upload->received_mask = 0u;
        upload->active = true;
        payload[0] = upload->bank;
        payload[1] = upload->program;
        payload[2] = 1u;
        if (!gp100_sysex_build_response(
                response_address, payload, 3u,
                response, response_capacity, response_length)) {
            return GP100_SYSEX_TRANSFER_INVALID;
        }
        return GP100_SYSEX_TRANSFER_RESPONSE;
    }
    if (!upload->active || upload->selected != selected ||
        request->payload_length < 2u ||
        request->payload[0] != upload->bank ||
        request->payload[1] != upload->program) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    if (stage == 1u) {
        if (request->payload_length < 4u ||
            request->payload[2] > 0x7fu ||
            request->payload[3] > 0x7fu) {
            return GP100_SYSEX_TRANSFER_INVALID;
        }
        chunk_index = decode_u14(&request->payload[2]);
        chunk_size = chunk_index < 8u ?
                     GP100_SYSEX_TRANSFER_CHUNK_BYTES : 0x1cu;
        if (chunk_index >= 9u ||
            request->payload_length !=
                GP100_SYSEX_TRANSFER_PREFIX_BYTES + chunk_size) {
            return GP100_SYSEX_TRANSFER_INVALID;
        }
        offset = (size_t)chunk_index *
                 GP100_SYSEX_TRANSFER_CHUNK_BYTES;
        memcpy(upload->data + offset,
               request->payload + GP100_SYSEX_TRANSFER_PREFIX_BYTES,
               chunk_size);
        if ((upload->received_mask & (uint16_t)(1u << chunk_index)) == 0u) {
            upload->received_mask |= (uint16_t)(1u << chunk_index);
            ++upload->received_chunks;
        }
        memcpy(payload, request->payload, 4u);
        payload[4] = 1u;
        if (!gp100_sysex_build_response(
                response_address, payload, sizeof(payload),
                response, response_capacity, response_length)) {
            return GP100_SYSEX_TRANSFER_INVALID;
        }
        return GP100_SYSEX_TRANSFER_RESPONSE;
    }

    if (request->payload_length != 2u || completed_record == NULL ||
        completed_logical_index == NULL) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    valid = finish_upload(upload, completed_record, completed_logical_index);
    payload[0] = upload->bank;
    payload[1] = upload->program;
    encode_u14(9u, &payload[2]);
    payload[4] = valid ? 1u : 0u;
    upload->active = false;
    if (!gp100_sysex_build_response(
            response_address, payload, sizeof(payload),
            response, response_capacity, response_length)) {
        return GP100_SYSEX_TRANSFER_INVALID;
    }
    return valid ? GP100_SYSEX_TRANSFER_DONE
                 : GP100_SYSEX_TRANSFER_REJECTED;
}
