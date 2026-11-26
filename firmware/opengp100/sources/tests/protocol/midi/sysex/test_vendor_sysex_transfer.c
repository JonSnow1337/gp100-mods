#include "protocol/midi/sysex/vendor_sysex_transfer.h"
#include "protocol/midi/sysex/vendor_sysex_data.h"

#include <stdio.h>
#include <string.h>

static void fill_data(uint8_t *data, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        data[index] = (uint8_t)index;
    }
}

static int test_download(void)
{
    uint8_t data[0x61cu];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t start_payload[] = {1u, 1u};
    uint8_t advance_payload[] = {1u, 1u, 0u, 0u, 1u};
    gp100_sysex_view_t request = {
        0x7fu, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_SELECTED_START,
        start_payload, sizeof(start_payload)
    };
    gp100_sysex_view_t view;
    gp100_sysex_transfer_t transfer;
    size_t response_length;

    fill_data(data, sizeof(data));
    if (!gp100_sysex_transfer_init(&transfer, data, sizeof(data)) ||
        transfer.chunk_count != 9u ||
        gp100_sysex_transfer_chunk_size(&transfer, 0u) != 0xc0u ||
        gp100_sysex_transfer_chunk_size(&transfer, 8u) != 0x1cu) {
        return 1;
    }
    if (gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_SELECTED_CHUNK ||
        view.payload_length != 0xc4u ||
        view.payload[0] != 1u || view.payload[1] != 1u ||
        view.payload[2] != 0u || view.payload[3] != 0u ||
        view.payload[4] != data[0] || view.payload[0xc3] != data[0xbfu]) {
        return 2;
    }

    request.address = GP100_SYSEX_SELECTED_ADVANCE;
    request.payload = advance_payload;
    request.payload_length = sizeof(advance_payload);
    if (gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, 17u, &response_length) != GP100_SYSEX_TRANSFER_INVALID ||
        transfer.next_chunk != 0u) {
        return 3;
    }
    if (gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_RESPONSE ||
        transfer.next_chunk != 1u ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.payload[2] != 0u || view.payload[3] != 1u ||
        view.payload[4] != data[0xc0u]) {
        return 3;
    }

    transfer.next_chunk = 8u;
    advance_payload[4] = 0u;
    if (gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.payload_length != 0x20u ||
        view.payload[3] != 8u || view.payload[4] != data[0x600u]) {
        return 4;
    }

    advance_payload[4] = 1u;
    if (gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_DONE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_SELECTED_COMPLETE ||
        view.payload_length != 4u ||
        view.payload[2] != 0u || view.payload[3] != 9u) {
        return 5;
    }
    return 0;
}

static int test_validation(void)
{
    uint8_t data[1] = {0u};
    uint8_t response[32];
    uint8_t payload[1] = {0u};
    gp100_sysex_transfer_t transfer;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_SELECTED_START, payload, sizeof(payload)
    };
    size_t response_length;

    if (!gp100_sysex_transfer_init(&transfer, data, sizeof(data)) ||
        gp100_sysex_transfer_handle_download(
            &transfer, &request,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_UNHANDLED ||
        gp100_sysex_transfer_init(&transfer, NULL, 0u)) {
        return 1;
    }
    return 0;
}

static int send_upload_chunks(
    gp100_sysex_upload_t *upload,
    const uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES],
    uint8_t bank,
    uint8_t program,
    uint32_t chunk_address,
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES],
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_TRANSFER_PREFIX_BYTES +
                    GP100_SYSEX_TRANSFER_CHUNK_BYTES];
    gp100_sysex_view_t request = {
        0x7fu, GP100_SYSEX_RESPONSE_CLASS,
        chunk_address, payload, 0u
    };
    uint16_t chunk_index;

    payload[0] = bank;
    payload[1] = program;
    for (chunk_index = 0u; chunk_index < 9u; ++chunk_index) {
        size_t chunk_size = chunk_index < 8u ? 0xc0u : 0x1cu;
        size_t offset = (size_t)chunk_index * 0xc0u;

        payload[2] = (uint8_t)(chunk_index >> 7);
        payload[3] = (uint8_t)(chunk_index & 0x7fu);
        memcpy(payload + GP100_SYSEX_TRANSFER_PREFIX_BYTES,
               encoded + offset, chunk_size);
        request.payload_length =
            GP100_SYSEX_TRANSFER_PREFIX_BYTES + chunk_size;
        if (gp100_sysex_transfer_handle_upload(
                upload, &request, NULL, NULL,
                response, GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES,
                response_length) != GP100_SYSEX_TRANSFER_RESPONSE) {
            return 1;
        }
    }
    return 0;
}

static int test_upload(void)
{
    gp100_preset_record_t source;
    gp100_preset_record_t completed;
    uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    uint8_t upload_buffer[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t begin_payload[] = {1u, 1u, 0x0cu, 0x1cu, 1u, 0x40u};
    uint8_t finish_payload[] = {1u, 1u};
    gp100_sysex_view_t request = {
        0x7fu, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_DATA_CURRENT_PRESET_READY,
        begin_payload, sizeof(begin_payload)
    };
    gp100_sysex_view_t view;
    gp100_sysex_upload_t upload;
    uint16_t logical_index = UINT16_MAX;
    size_t encoded_length;
    size_t response_length;

    memset(&source, 0, sizeof(source));
    source.bytes[2] = (uint8_t)'T';
    source.bytes[3] = (uint8_t)'E';
    source.bytes[4] = (uint8_t)'S';
    source.bytes[5] = (uint8_t)'T';
    if (!gp100_sysex_encode_preset_transfer(
            &source, 100u, encoded, sizeof(encoded), &encoded_length) ||
        !gp100_sysex_upload_init(
            &upload, upload_buffer, sizeof(upload_buffer)) ||
        gp100_sysex_transfer_handle_upload(
            &upload, &request, &completed, &logical_index,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_START ||
        view.payload_length != 3u || view.payload[2] != 1u) {
        return 1;
    }
    if (send_upload_chunks(
            &upload, encoded, 1u, 1u,
            GP100_SYSEX_CURRENT_CHUNK,
            response, &response_length) != 0) {
        return 2;
    }
    request.address = GP100_SYSEX_CURRENT_COMPLETE;
    request.payload = finish_payload;
    request.payload_length = sizeof(finish_payload);
    if (gp100_sysex_transfer_handle_upload(
            &upload, &request, &completed, &logical_index,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_DONE ||
        logical_index != 100u ||
        gp100_preset_validate(&completed) != GP100_PRESET_VALID ||
        completed.bytes[2] != (uint8_t)'T' ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_FINAL ||
        view.payload_length != 5u || view.payload[4] != 1u) {
        return 3;
    }
    return 0;
}

static int test_upload_crc_rejection(void)
{
    gp100_preset_record_t source;
    gp100_preset_record_t completed;
    uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    uint8_t upload_buffer[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t begin_payload[] = {0u, 0u, 0x0cu, 0x1cu, 1u, 0x40u};
    uint8_t finish_payload[] = {0u, 0u};
    gp100_sysex_view_t request = {
        0x7fu, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_DATA_SELECTED_PRESET_READY,
        begin_payload, sizeof(begin_payload)
    };
    gp100_sysex_view_t view;
    gp100_sysex_upload_t upload;
    uint16_t logical_index;
    size_t encoded_length;
    size_t response_length;

    memset(&source, 0, sizeof(source));
    if (!gp100_sysex_encode_preset_transfer(
            &source, 0u, encoded, sizeof(encoded), &encoded_length) ||
        !gp100_sysex_upload_init(
            &upload, upload_buffer, sizeof(upload_buffer)) ||
        gp100_sysex_transfer_handle_upload(
            &upload, &request, &completed, &logical_index,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_RESPONSE) {
        return 1;
    }
    encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES - 1u] ^= 1u;
    if (send_upload_chunks(
            &upload, encoded, 0u, 0u,
            GP100_SYSEX_SELECTED_CHUNK,
            response, &response_length) != 0) {
        return 2;
    }
    request.address = GP100_SYSEX_SELECTED_COMPLETE;
    request.payload = finish_payload;
    request.payload_length = sizeof(finish_payload);
    if (gp100_sysex_transfer_handle_upload(
            &upload, &request, &completed, &logical_index,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_TRANSFER_REJECTED ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_SELECTED_FINAL ||
        view.payload[4] != 0u) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_download();
    if (result != 0) {
        fprintf(stderr, "download test failed: %d\n", result);
        return 10 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 20 + result;
    }
    result = test_upload();
    if (result != 0) {
        fprintf(stderr, "upload test failed: %d\n", result);
        return 30 + result;
    }
    result = test_upload_crc_rejection();
    if (result != 0) {
        fprintf(stderr, "upload CRC test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
