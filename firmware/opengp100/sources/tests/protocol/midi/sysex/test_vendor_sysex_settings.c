#include "protocol/midi/sysex/vendor_sysex_settings.h"

#include <stdio.h>
#include <string.h>

static int test_snapshot_round_trip(void)
{
    gp100_settings_record_t source = {{0}};
    gp100_settings_record_t original;
    gp100_settings_record_t expected;
    gp100_settings_record_t decoded;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_SETTINGS_SNAPSHOT, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    size_t response_length;
    size_t index;

    for (index = 0u; index < GP100_SETTINGS_PAYLOAD_SIZE; ++index) {
        source.bytes[index] = (uint8_t)(index * 37u + 11u);
    }
    memset(source.bytes + GP100_SETTINGS_PAYLOAD_SIZE,
           0xa5,
           GP100_SETTINGS_RECORD_SIZE -
               GP100_SETTINGS_PAYLOAD_SIZE);
    original = source;
    expected = source;
    if (!gp100_settings_stamp_v210_snapshot(&expected)) {
        return 1;
    }
    if (gp100_sysex_settings_build_snapshot(
            &request, &source, response, sizeof(response), &response_length) !=
            GP100_SYSEX_SETTINGS_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.address != GP100_SYSEX_SETTINGS_SNAPSHOT ||
        response_view.payload_length !=
            GP100_SYSEX_SETTINGS_SNAPSHOT_BYTES ||
        !gp100_sysex_settings_decode_snapshot(
            response_view.payload, response_view.payload_length, &decoded) ||
        memcmp(expected.bytes, decoded.bytes,
               GP100_SETTINGS_PAYLOAD_SIZE) != 0 ||
        memcmp(source.bytes, original.bytes, sizeof(source.bytes)) != 0) {
        return 2;
    }
    for (index = GP100_SETTINGS_PAYLOAD_SIZE;
         index < GP100_SETTINGS_RECORD_SIZE;
         ++index) {
        if (decoded.bytes[index] != 0u) {
            return 3;
        }
    }
    response[(response_length - 1u) - 3u] ^= 1u;
    if (!gp100_sysex_parse(response, response_length, &response_view) ||
        gp100_sysex_settings_decode_snapshot(
            response_view.payload, response_view.payload_length, &decoded)) {
        return 4;
    }
    return 0;
}

static int test_dispatch_boundary(void)
{
    gp100_settings_record_t settings = {{0}};
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_SETTINGS_SNAPSHOT + 1u, NULL, 0u
    };
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    size_t response_length;

    if (gp100_sysex_settings_build_snapshot(
            &request, &settings, response, sizeof(response), &response_length) !=
        GP100_SYSEX_SETTINGS_UNHANDLED) {
        return 1;
    }
    request.address = GP100_SYSEX_SETTINGS_SNAPSHOT;
    request.payload_length = 1u;
    if (gp100_sysex_settings_build_snapshot(
            &request, &settings, response, sizeof(response), &response_length) !=
        GP100_SYSEX_SETTINGS_INVALID) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_snapshot_round_trip();
    if (result != 0) {
        fprintf(stderr, "settings snapshot test failed: %d\n", result);
        return 10 + result;
    }
    result = test_dispatch_boundary();
    if (result != 0) {
        fprintf(stderr, "settings dispatch test failed: %d\n", result);
        return 20 + result;
    }
    return 0;
}
