#include "vendor_sysex_settings.h"

#include "storage/preset_record.h"

#include <string.h>

gp100_sysex_settings_result_t gp100_sysex_settings_build_snapshot(
    const gp100_sysex_view_t *request,
    const gp100_settings_record_t *settings,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    gp100_settings_record_t snapshot;
    uint8_t payload[GP100_SYSEX_SETTINGS_SNAPSHOT_BYTES];
    uint32_t crc;
    size_t encoded_length;

    if (request == NULL || settings == NULL || response_length == NULL) {
        return GP100_SYSEX_SETTINGS_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        request->address != GP100_SYSEX_SETTINGS_SNAPSHOT) {
        return GP100_SYSEX_SETTINGS_UNHANDLED;
    }
    snapshot = *settings;
    if (request->payload_length != 0u ||
        !gp100_settings_stamp_v210_snapshot(&snapshot) ||
        !gp100_sysex_nibble_encode(
            snapshot.bytes,
            GP100_SETTINGS_PAYLOAD_SIZE,
            payload,
            GP100_SYSEX_SETTINGS_ENCODED_BYTES,
            &encoded_length) ||
        encoded_length != GP100_SYSEX_SETTINGS_ENCODED_BYTES) {
        return GP100_SYSEX_SETTINGS_INVALID;
    }
    crc = gp100_crc32(payload, encoded_length);
    gp100_sysex_encode_u32(crc, payload + encoded_length);
    if (!gp100_sysex_build_response(
            request->address,
            payload,
            sizeof(payload),
            response,
            response_capacity,
            response_length)) {
        return GP100_SYSEX_SETTINGS_INVALID;
    }
    return GP100_SYSEX_SETTINGS_RESPONSE;
}

bool gp100_sysex_settings_decode_snapshot(
    const uint8_t *payload,
    size_t payload_length,
    gp100_settings_record_t *settings)
{
    uint32_t expected_crc;
    uint32_t actual_crc;
    size_t decoded_length;

    if (payload == NULL || settings == NULL ||
        payload_length != GP100_SYSEX_SETTINGS_SNAPSHOT_BYTES ||
        !gp100_sysex_decode_u32(
            payload + GP100_SYSEX_SETTINGS_ENCODED_BYTES,
            &expected_crc)) {
        return false;
    }
    actual_crc = gp100_crc32(
        payload, GP100_SYSEX_SETTINGS_ENCODED_BYTES);
    if (expected_crc != actual_crc ||
        !gp100_sysex_nibble_decode(
            payload,
            GP100_SYSEX_SETTINGS_ENCODED_BYTES,
            settings->bytes,
            GP100_SETTINGS_PAYLOAD_SIZE,
            &decoded_length) ||
        decoded_length != GP100_SETTINGS_PAYLOAD_SIZE) {
        return false;
    }
    memset(settings->bytes + GP100_SETTINGS_PAYLOAD_SIZE,
           0,
           GP100_SETTINGS_RECORD_SIZE -
               GP100_SETTINGS_PAYLOAD_SIZE);
    return true;
}
