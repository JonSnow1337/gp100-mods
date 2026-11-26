#include "protocol/midi/sysex/vendor_sysex_data.h"

#include <stdio.h>
#include <string.h>

static int test_control_messages(void)
{
    gp100_sysex_view_t request = {
        1u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_DATA_PRESET_STATUS, NULL, 0u
    };
    const uint8_t expected_status[] = {
        0xf0u, 0x21u, 0x25u, 0x7fu,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x12u, 0x12u, 0x00u, 0x00u, 0x01u,
        1u, 1u, 1u, 0xf7u
    };
    uint8_t response[32];
    size_t response_length;

    if (gp100_sysex_data_handle_control(
            &request, 100u, response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_RESPONSE ||
        response_length != sizeof(expected_status) ||
        memcmp(response, expected_status, sizeof(expected_status)) != 0) {
        return 1;
    }
    request.address = GP100_SYSEX_DATA_SESSION_BEGIN;
    if (gp100_sysex_data_handle_control(
            &request, 0u, response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_RESPONSE ||
        response_length != 16u || response[13] != 1u ||
        response[14] != 0u || response[15] != 0xf7u) {
        return 2;
    }
    request.address = GP100_SYSEX_DATA_SESSION_END;
    if (gp100_sysex_data_handle_control(
            &request, 0u, response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_RESPONSE ||
        response_length != GP100_SYSEX_MIN_MESSAGE_BYTES ||
        response[13] != 0xf7u) {
        return 3;
    }
    request.address = UINT32_C(0xffffffff);
    if (gp100_sysex_data_handle_control(
            &request, 0u, response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_UNHANDLED) {
        return 4;
    }
    return 0;
}

static int test_preset_transfer(void)
{
    const uint8_t expected_crc[] = {
        0x0fu, 0x04u, 0x03u, 0x01u,
        0x07u, 0x04u, 0x03u, 0x07u
    };
    gp100_preset_record_t record;
    uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    size_t encoded_length;

    memset(&record, 0, sizeof(record));
    if (!gp100_sysex_encode_preset_transfer(
            &record, 100u, encoded, sizeof(encoded), &encoded_length) ||
        encoded_length != sizeof(encoded) ||
        encoded[0] != 6u || encoded[1] != 4u ||
        encoded[2] != 0u || encoded[3] != 0u ||
        memcmp(encoded + GP100_SYSEX_PRESET_TRANSFER_DATA_BYTES,
               expected_crc, sizeof(expected_crc)) != 0) {
        return 1;
    }
    return 0;
}

static int test_descriptor(void)
{
    const uint8_t expected_payload[] = {1u, 1u, 0x0cu, 0x1cu, 1u, 0x40u};
    uint8_t invalid_payload[sizeof(expected_payload)];
    uint8_t response[32];
    size_t response_length;
    gp100_sysex_view_t view;
    gp100_sysex_preset_descriptor_t descriptor;

    if (!gp100_sysex_build_preset_descriptor(
            GP100_SYSEX_DATA_READ_CURRENT_PRESET,
            100u, response, sizeof(response), &response_length) ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_DATA_CURRENT_PRESET_READY ||
        view.payload_length != sizeof(expected_payload) ||
        memcmp(view.payload, expected_payload, sizeof(expected_payload)) != 0 ||
        !gp100_sysex_decode_preset_descriptor(
            view.payload, view.payload_length, &descriptor) ||
        descriptor.bank != 1u || descriptor.program != 1u ||
        descriptor.transfer_bytes != GP100_SYSEX_PRESET_TRANSFER_BYTES ||
        descriptor.chunk_bytes !=
            GP100_SYSEX_PRESET_TRANSFER_CHUNK_BYTES) {
        return 1;
    }
    if (gp100_sysex_build_preset_descriptor(
            UINT32_C(0x13030000), 100u,
            response, sizeof(response), &response_length)) {
        return 2;
    }
    if (gp100_sysex_decode_preset_descriptor(
            expected_payload, sizeof(expected_payload) - 1u, &descriptor)) {
        return 3;
    }
    memcpy(invalid_payload, expected_payload, sizeof(invalid_payload));
    invalid_payload[2] = 0x80u;
    if (gp100_sysex_decode_preset_descriptor(
            invalid_payload, sizeof(invalid_payload), &descriptor)) {
        return 4;
    }
    memcpy(invalid_payload, expected_payload, sizeof(invalid_payload));
    invalid_payload[0] = 2u;
    if (gp100_sysex_decode_preset_descriptor(
            invalid_payload, sizeof(invalid_payload), &descriptor)) {
        return 5;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_sysex_view_t request = {
        1u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_DATA_PRESET_STATUS, NULL, 1u
    };
    uint8_t response[32];
    size_t response_length;
    uint8_t bank;
    uint8_t program;

    if (gp100_sysex_preset_location(
            GP100_TOTAL_LOGICAL_PRESETS, &bank, &program) ||
        gp100_sysex_data_handle_control(
            &request, 0u, response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_INVALID) {
        return 1;
    }
    return 0;
}

static int test_runtime_status(void)
{
    gp100_sysex_runtime_status_t source = {
        .rhythm_enabled = 1u,
        .volume_pedal_value = 75u,
        .rhythm_volume = 99u,
        .preset_modified = 1u
    };
    gp100_sysex_runtime_status_t decoded;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_DATA_RUNTIME_STATUS, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    uint8_t response[64];
    size_t response_length;

    if (gp100_sysex_data_build_runtime_status(
            &request, &source,
            response, sizeof(response), &response_length) !=
            GP100_SYSEX_DATA_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.payload_length !=
            GP100_SYSEX_RUNTIME_STATUS_BYTES ||
        !gp100_sysex_data_decode_runtime_status(
            response_view.payload, response_view.payload_length, &decoded) ||
        decoded.rhythm_enabled != source.rhythm_enabled ||
        decoded.volume_pedal_value != source.volume_pedal_value ||
        decoded.rhythm_volume != source.rhythm_volume ||
        decoded.preset_modified != source.preset_modified) {
        return 1;
    }
    response[13] ^= 1u;
    if (gp100_sysex_data_decode_runtime_status(
            response_view.payload, response_view.payload_length, &decoded)) {
        return 2;
    }
    return 0;
}

static int test_save_user_preset(void)
{
    uint8_t payload[8];
    gp100_sysex_save_preset_action_t action;
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_DATA_SAVE_USER_PRESET,
        payload, sizeof(payload)
    };

    gp100_sysex_encode_u16(1u, payload);
    gp100_sysex_encode_u16(98u, payload + 4u);
    if (gp100_sysex_data_decode_save_preset(&command, &action) !=
            GP100_SYSEX_DATA_ACTION ||
        action.selector != 1u || action.user_index != 98u) {
        return 1;
    }
    gp100_sysex_encode_u16(99u, payload + 4u);
    if (gp100_sysex_data_decode_save_preset(&command, &action) !=
            GP100_SYSEX_DATA_INVALID) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_control_messages();
    if (result != 0) {
        fprintf(stderr, "control-message test failed: %d\n", result);
        return 10 + result;
    }
    result = test_preset_transfer();
    if (result != 0) {
        fprintf(stderr, "preset-transfer test failed: %d\n", result);
        return 20 + result;
    }
    result = test_descriptor();
    if (result != 0) {
        fprintf(stderr, "descriptor test failed: %d\n", result);
        return 30 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 40 + result;
    }
    result = test_runtime_status();
    if (result != 0) {
        fprintf(stderr, "runtime-status test failed: %d\n", result);
        return 50 + result;
    }
    result = test_save_user_preset();
    if (result != 0) {
        fprintf(stderr, "save-user-preset test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
