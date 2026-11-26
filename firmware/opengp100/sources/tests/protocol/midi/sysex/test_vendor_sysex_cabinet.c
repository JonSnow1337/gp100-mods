#include "protocol/midi/sysex/vendor_sysex_cabinet.h"

#include <stdio.h>
#include <string.h>

static int test_info_round_trip(void)
{
    uint8_t record[GP100_CABINET_RECORD_SIZE] = {0};
    uint8_t request_payload[] = {7u};
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_CABINET_METADATA_READ,
        request_payload, sizeof(request_payload)
    };
    gp100_sysex_view_t response_view;
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t metadata[GP100_SYSEX_CABINET_METADATA_BYTES];
    uint8_t user_index;
    uint8_t checksum;
    size_t response_length;

    memcpy(record, "Studio IR", 9u);
    record[GP100_SYSEX_CABINET_METADATA_BYTES] = 0xab;
    record[GP100_CABINET_RECORD_SIZE - 1u] = 0x19;
    if (gp100_sysex_cabinet_build_info(
            &request, record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_CABINET_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        !gp100_sysex_cabinet_decode_info(
            response_view.payload,
            response_view.payload_length,
            &user_index,
            metadata,
            &checksum) ||
        user_index != 7u ||
        memcmp(metadata, record, sizeof(metadata)) != 0 ||
        checksum != (uint8_t)(0x0au + 0x0bu + 1u + 9u)) {
        return 1;
    }
    return 0;
}

static int test_metadata_update(void)
{
    uint8_t payload[4u + GP100_SYSEX_CABINET_METADATA_BYTES] = {0};
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_CABINET_METADATA_UPDATE,
        payload, sizeof(payload)
    };
    gp100_sysex_cabinet_metadata_action_t action;

    memset(record, 0x5a, sizeof(record));
    gp100_sysex_encode_u16(19u, payload);
    memcpy(payload + 4u, "Last user IR", 12u);
    if (gp100_sysex_cabinet_decode_metadata_update(
            &command, &action) != GP100_SYSEX_CABINET_ACTION ||
        action.user_index != 19u ||
        !gp100_sysex_cabinet_apply_metadata(record, &action) ||
        memcmp(record, payload + 4u,
               GP100_SYSEX_CABINET_METADATA_BYTES) != 0 ||
        record[GP100_SYSEX_CABINET_METADATA_BYTES] != 0x5a) {
        return 1;
    }
    gp100_sysex_encode_u16(20u, payload);
    if (gp100_sysex_cabinet_decode_metadata_update(
            &command, &action) != GP100_SYSEX_CABINET_INVALID) {
        return 2;
    }
    return 0;
}

static int test_full_upload(void)
{
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    uint8_t completed[GP100_CABINET_RECORD_SIZE];
    uint8_t workspace[GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES];
    uint8_t payload[GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES];
    uint8_t response[32];
    gp100_sysex_cabinet_upload_t upload;
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_CABINET_METADATA_READ,
        payload, sizeof(payload)
    };
    size_t response_length;
    size_t chunk;
    size_t index;
    uint8_t completed_index = 0xffu;

    for (index = 0u; index < sizeof(record); ++index) {
        record[index] = (uint8_t)(index * 19u + 7u);
    }
    if (!gp100_sysex_cabinet_upload_init(
            &upload, workspace, sizeof(workspace))) {
        return 1;
    }
    for (chunk = 0u;
         chunk < GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT;
         ++chunk) {
        if (!gp100_sysex_cabinet_build_upload_payload(
                12u, (uint16_t)chunk, record, payload) ||
            gp100_sysex_cabinet_handle_upload(
                &upload,
                &command,
                completed,
                &completed_index,
                response,
                sizeof(response),
                &response_length) !=
            (chunk + 1u == GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT
                 ? GP100_SYSEX_CABINET_DONE
                 : GP100_SYSEX_CABINET_RESPONSE)) {
            return 2;
        }
    }
    if (completed_index != 12u ||
        memcmp(completed, record, sizeof(record)) != 0 ||
        payload[27] != GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE ||
        payload[28] != GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE ||
        payload[29] != GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE ||
        payload[30] != GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE) {
        return 3;
    }
    if (gp100_sysex_cabinet_build_upload_payload(
            GP100_CABINET_USER_COUNT, 0u, record, payload) ||
        gp100_sysex_cabinet_build_upload_payload(
            0u, GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT,
            record, payload) ||
        gp100_sysex_cabinet_build_upload_payload(
            0u, 0u, NULL, payload)) {
        return 4;
    }
    return 0;
}

static int test_reset(void)
{
    uint8_t payload[4];
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_CABINET_RESET,
        payload, sizeof(payload)
    };
    gp100_sysex_cabinet_reset_action_t action;

    memset(record, 0x5au, sizeof(record));
    gp100_sysex_encode_u16(19u, payload);
    if (gp100_sysex_cabinet_decode_reset(&command, &action) !=
            GP100_SYSEX_CABINET_ACTION ||
        action.user_index != 19u ||
        !gp100_sysex_cabinet_apply_reset(record, &action)) {
        return 1;
    }
    for (size_t index = 0u;
         index < GP100_SYSEX_CABINET_METADATA_BYTES; ++index) {
        if (record[index] != 0u)
            return 2;
    }
    if (record[GP100_SYSEX_CABINET_METADATA_BYTES] != 0x5au)
        return 3;
    gp100_sysex_encode_u16(20u, payload);
    if (gp100_sysex_cabinet_decode_reset(&command, &action) !=
            GP100_SYSEX_CABINET_INVALID) {
        return 4;
    }
    return 0;
}

int main(void)
{
    int result = test_info_round_trip();
    if (result != 0) {
        fprintf(stderr, "cabinet info test failed: %d\n", result);
        return 10 + result;
    }
    result = test_metadata_update();
    if (result != 0) {
        fprintf(stderr, "cabinet metadata test failed: %d\n", result);
        return 20 + result;
    }
    result = test_full_upload();
    if (result != 0) {
        fprintf(stderr, "cabinet upload test failed: %d\n", result);
        return 30 + result;
    }
    result = test_reset();
    if (result != 0) {
        fprintf(stderr, "cabinet reset test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
