#include "protocol/midi/sysex/vendor_sysex.h"

#include <stdio.h>
#include <string.h>

static int test_response_and_parse(void)
{
    const uint8_t payload[] = {1u, 2u, 3u};
    const uint8_t expected[] = {
        0xf0u, 0x21u, 0x25u, 0x7fu,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x12u, 0x13u, 0x02u, 0x00u, 0x07u,
        1u, 2u, 3u, 0xf7u
    };
    uint8_t message[32];
    size_t message_length;
    gp100_sysex_view_t view;

    if (!gp100_sysex_build_response(
            0x13020007u, payload, sizeof(payload),
            message, sizeof(message), &message_length) ||
        message_length != sizeof(expected) ||
        memcmp(message, expected, sizeof(expected)) != 0 ||
        !gp100_sysex_parse(message, message_length, &view) ||
        view.device_id != 0x7fu ||
        view.message_class != GP100_SYSEX_RESPONSE_CLASS ||
        view.address != 0x13020007u ||
        view.payload_length != sizeof(payload) ||
        memcmp(view.payload, payload, sizeof(payload)) != 0) {
        return 1;
    }
    return 0;
}

static int test_request_parse(void)
{
    const uint8_t request[] = {
        0xf0u, 0x21u, 0x25u, 0x01u,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x11u, 0x12u, 0x00u, 0x00u, 0x01u,
        0xf7u
    };
    gp100_sysex_view_t view;

    if (!gp100_sysex_parse(request, sizeof(request), &view) ||
        view.device_id != 1u ||
        view.message_class != GP100_SYSEX_REQUEST_CLASS ||
        view.address != 0x12000001u || view.payload_length != 0u) {
        return 1;
    }
    return 0;
}

static int test_address_router(void)
{
    static const struct {
        uint32_t address;
        gp100_sysex_domain_t domain;
    } cases[] = {
        {0x00000000u, GP100_SYSEX_DOMAIN_CONTROL},
        {0x00000005u, GP100_SYSEX_DOMAIN_CONTROL},
        {0x00001008u, GP100_SYSEX_DOMAIN_CONTROL},
        {0x00001009u, GP100_SYSEX_DOMAIN_CONTROL},
        {0x0000100au, GP100_SYSEX_DOMAIN_CONTROL},
        {0x0000100du, GP100_SYSEX_DOMAIN_CONTROL},
        {0x0000100eu, GP100_SYSEX_DOMAIN_CONTROL},
        {0x00010103u, GP100_SYSEX_DOMAIN_PERFORMANCE},
        {0x00010204u, GP100_SYSEX_DOMAIN_PERFORMANCE},
        {0x00020000u, GP100_SYSEX_DOMAIN_SYSTEM},
        {0x00020008u, GP100_SYSEX_DOMAIN_SYSTEM},
        {0x00020010u, GP100_SYSEX_DOMAIN_GLOBAL},
        {0x0002001fu, GP100_SYSEX_DOMAIN_GLOBAL},
        {0x10010000u, GP100_SYSEX_DOMAIN_EFFECT},
        {0x10090002u, GP100_SYSEX_DOMAIN_EFFECT},
        {0x11000001u, GP100_SYSEX_DOMAIN_PRESET},
        {0x11000007u, GP100_SYSEX_DOMAIN_PRESET},
        {0x11000006u, GP100_SYSEX_DOMAIN_GENRE},
        {0x11000008u, GP100_SYSEX_DOMAIN_GENRE},
        {0x12000000u, GP100_SYSEX_DOMAIN_DATA},
        {0x12000002u, GP100_SYSEX_DOMAIN_DATA},
        {0x12001000u, GP100_SYSEX_DOMAIN_CABINET},
        {0x12001002u, GP100_SYSEX_DOMAIN_CABINET},
        {0x12001012u, GP100_SYSEX_DOMAIN_USER_AMP},
        {0x12001015u, GP100_SYSEX_DOMAIN_USER_AMP},
        {0x12002000u, GP100_SYSEX_DOMAIN_SYSTEM},
        {0x13000000u, GP100_SYSEX_DOMAIN_SETTINGS},
        {0x13010000u, GP100_SYSEX_DOMAIN_DATA},
        {0x13010001u, GP100_SYSEX_DOMAIN_DATA},
        {0x13010002u, GP100_SYSEX_DOMAIN_TRANSFER},
        {0x13010006u, GP100_SYSEX_DOMAIN_TRANSFER},
        {0x13020000u, GP100_SYSEX_DOMAIN_DATA},
        {0x13020001u, GP100_SYSEX_DOMAIN_DATA},
        {0x13020002u, GP100_SYSEX_DOMAIN_TRANSFER},
        {0x13020006u, GP100_SYSEX_DOMAIN_TRANSFER},
        {0x13020007u, GP100_SYSEX_DOMAIN_DATA}
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (gp100_sysex_domain_for_address(cases[index].address) !=
                cases[index].domain ||
            strcmp(gp100_sysex_domain_name(cases[index].domain),
                   "Unknown") == 0) {
            return 1;
        }
    }
    if (gp100_sysex_domain_for_address(0x00000006u) !=
            GP100_SYSEX_DOMAIN_UNKNOWN ||
        gp100_sysex_domain_for_address(0x0000100bu) !=
            GP100_SYSEX_DOMAIN_UNKNOWN ||
        gp100_sysex_domain_for_address(0x10010003u) !=
            GP100_SYSEX_DOMAIN_UNKNOWN ||
        gp100_sysex_domain_for_address(0x11000000u) !=
            GP100_SYSEX_DOMAIN_UNKNOWN ||
        gp100_sysex_domain_for_address(0x13020008u) !=
            GP100_SYSEX_DOMAIN_UNKNOWN ||
        strcmp(gp100_sysex_domain_name(
                   (gp100_sysex_domain_t)99), "Unknown") != 0) {
        return 2;
    }
    return 0;
}

static int test_route_message(void)
{
    const uint8_t request[] = {
        0xf0u, 0x21u, 0x25u, 0x01u,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x11u, 0x10u, 0x05u, 0x00u, 0x02u,
        0xf7u
    };
    gp100_sysex_view_t view;
    gp100_sysex_domain_t domain;

    if (!gp100_sysex_route(
            request, sizeof(request), &view, &domain) ||
        view.address != 0x10050002u ||
        domain != GP100_SYSEX_DOMAIN_EFFECT ||
        gp100_sysex_route(
            request, sizeof(request) - 1u, &view, &domain) ||
        domain != GP100_SYSEX_DOMAIN_UNKNOWN) {
        return 1;
    }
    return 0;
}

static int test_nibble_codec(void)
{
    const uint8_t raw[] = {0x00u, 0xabu, 0xffu, 0x42u};
    const uint8_t expected[] = {0u, 0u, 0xau, 0xbu, 0xfu, 0xfu, 4u, 2u};
    uint8_t encoded[8];
    uint8_t decoded[4];
    size_t length;

    if (!gp100_sysex_nibble_encode(
            raw, sizeof(raw), encoded, sizeof(encoded), &length) ||
        length != sizeof(expected) ||
        memcmp(encoded, expected, sizeof(expected)) != 0 ||
        !gp100_sysex_nibble_decode(
            encoded, sizeof(encoded), decoded, sizeof(decoded), &length) ||
        length != sizeof(raw) || memcmp(decoded, raw, sizeof(raw)) != 0) {
        return 1;
    }
    encoded[3] = 0x10u;
    if (gp100_sysex_nibble_decode(
            encoded, sizeof(encoded), decoded, sizeof(decoded), &length)) {
        return 2;
    }
    return 0;
}

static int test_integer_codec(void)
{
    const uint8_t expected16[] = {0x3u, 0x4u, 0x1u, 0x2u};
    const uint8_t expected32[] = {
        0x7u, 0x8u, 0x5u, 0x6u, 0x3u, 0x4u, 0x1u, 0x2u
    };
    uint8_t encoded16[4];
    uint8_t encoded32[8];
    uint16_t value16;
    uint32_t value32;

    gp100_sysex_encode_u16(0x1234u, encoded16);
    gp100_sysex_encode_u32(0x12345678u, encoded32);
    if (memcmp(encoded16, expected16, sizeof(expected16)) != 0 ||
        memcmp(encoded32, expected32, sizeof(expected32)) != 0 ||
        !gp100_sysex_decode_u16(encoded16, &value16) ||
        !gp100_sysex_decode_u32(encoded32, &value32) ||
        value16 != 0x1234u || value32 != 0x12345678u) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    uint8_t message[GP100_SYSEX_MIN_MESSAGE_BYTES];
    uint8_t payload[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES] = {0u};
    size_t length;
    gp100_sysex_view_t view;

    if (gp100_sysex_build_response(
            0u, payload, sizeof(payload),
            message, sizeof(message), &length) ||
        length != sizeof(payload) + GP100_SYSEX_MIN_MESSAGE_BYTES ||
        gp100_sysex_parse(message, 0u, &view)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_response_and_parse();
    if (result != 0) {
        fprintf(stderr, "response/parse test failed: %d\n", result);
        return 10 + result;
    }
    result = test_request_parse();
    if (result != 0) {
        fprintf(stderr, "request parse test failed: %d\n", result);
        return 20 + result;
    }
    result = test_nibble_codec();
    if (result != 0) {
        fprintf(stderr, "nibble codec test failed: %d\n", result);
        return 30 + result;
    }
    result = test_integer_codec();
    if (result != 0) {
        fprintf(stderr, "integer codec test failed: %d\n", result);
        return 40 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 50 + result;
    }
    result = test_address_router();
    if (result != 0) {
        fprintf(stderr, "address router test failed: %d\n", result);
        return 60 + result;
    }
    result = test_route_message();
    if (result != 0) {
        fprintf(stderr, "message router test failed: %d\n", result);
        return 70 + result;
    }
    return 0;
}
