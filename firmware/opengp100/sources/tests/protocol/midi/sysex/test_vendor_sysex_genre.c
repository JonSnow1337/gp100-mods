#include "protocol/midi/sysex/vendor_sysex_genre.h"

#include <stdio.h>
#include <string.h>

static int test_read_and_update(void)
{
    static const uint8_t name[] = "MY GENRE";
    gp100_genre_catalog_t catalog;
    gp100_preset_record_t preset = {{0}};
    uint8_t payload[GP100_SYSEX_GENRE_READ_BYTES] = {0};
    gp100_sysex_view_t message = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_GENRE_NAME, payload,
        GP100_SYSEX_GENRE_INDEX_BYTES + sizeof(name) - 1u
    };
    gp100_sysex_genre_update_t update;
    gp100_sysex_view_t response_view;
    uint8_t response[40];
    size_t response_length;
    char decoded_name[GP100_GENRE_NAME_SIZE + 1u];

    gp100_genre_catalog_reset(&catalog);
    gp100_sysex_encode_u16(11u, payload);
    memcpy(payload + GP100_SYSEX_GENRE_INDEX_BYTES,
           name, sizeof(name) - 1u);
    if (gp100_sysex_genre_decode_update(&message, &update) !=
            GP100_SYSEX_GENRE_UPDATE ||
        !gp100_sysex_genre_apply(&catalog, &preset, &update) ||
        gp100_preset_genre_index(&preset) != 11u ||
        !gp100_genre_name(&catalog, 11u, decoded_name) ||
        strcmp(decoded_name, (const char *)name) != 0) {
        return 1;
    }
    message.address = GP100_SYSEX_GENRE_NAME_CHANGED;
    if (gp100_sysex_genre_decode_update(&message, &update) !=
            GP100_SYSEX_GENRE_UPDATE ||
        update.index != 11u) {
        return 2;
    }
    message.message_class = GP100_SYSEX_REQUEST_CLASS;
    message.address = GP100_SYSEX_GENRE_NAME;
    message.payload_length = GP100_SYSEX_GENRE_INDEX_BYTES;
    if (gp100_sysex_genre_build_state(
            &message, &catalog, response, sizeof(response),
            &response_length) != GP100_SYSEX_GENRE_RESPONSE ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.payload_length != GP100_SYSEX_GENRE_READ_BYTES ||
        memcmp(response_view.payload, payload,
               GP100_SYSEX_GENRE_INDEX_BYTES) != 0 ||
        memcmp(response_view.payload + GP100_SYSEX_GENRE_INDEX_BYTES,
               name, sizeof(name) - 1u) != 0) {
        return 3;
    }
    return 0;
}

static int test_notification_and_validation(void)
{
    gp100_genre_catalog_t catalog;
    gp100_sysex_view_t response_view;
    uint8_t response[32];
    size_t response_length;
    uint16_t index;

    gp100_genre_catalog_reset(&catalog);
    memcpy(catalog.names[60], "0123456789", GP100_GENRE_NAME_SIZE);
    if (!gp100_sysex_genre_build_notification(
            &catalog, 60u, response, sizeof(response), &response_length) ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.address != GP100_SYSEX_GENRE_NAME_CHANGED ||
        response_view.payload_length !=
            GP100_SYSEX_GENRE_NOTIFICATION_BYTES ||
        !gp100_sysex_decode_u16(response_view.payload, &index) ||
        index != 60u ||
        memcmp(response_view.payload + GP100_SYSEX_GENRE_INDEX_BYTES,
               "012345", GP100_SYSEX_GENRE_NOTIFICATION_NAME_BYTES) !=
            0) {
        return 1;
    }
    if (gp100_sysex_genre_build_notification(
            &catalog, 61u, response, sizeof(response), &response_length)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_read_and_update();
    if (result != 0) {
        fprintf(stderr, "genre read/update test failed: %d\n", result);
        return 10 + result;
    }
    result = test_notification_and_validation();
    if (result != 0) {
        fprintf(stderr, "genre notification test failed: %d\n", result);
        return 20 + result;
    }
    return 0;
}
