#include "protocol/midi/sysex/vendor_sysex_preset.h"

#include <stdio.h>
#include <string.h>

static int test_scalar_updates(void)
{
    gp100_preset_record_t record = {{0}};
    uint8_t payload[4];
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_PRESET_TEMPO, payload, sizeof(payload)
    };
    gp100_sysex_preset_action_t action;

    gp100_sysex_encode_u16(120u, payload);
    if (gp100_sysex_preset_decode_update(&request, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action) ||
        gp100_preset_tempo_bpm(&record) != 120u) {
        return 1;
    }
    request.address = GP100_SYSEX_PRESET_LEVEL;
    gp100_sysex_encode_u16(100u, payload);
    if (gp100_sysex_preset_decode_update(&request, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action) ||
        gp100_preset_level(&record) != 100u) {
        return 2;
    }
    request.address = GP100_SYSEX_PRESET_GENRE;
    gp100_sysex_encode_u16(GP100_PRESET_GENRE_COUNT, payload);
    if (gp100_sysex_preset_decode_update(&request, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action) ||
        gp100_preset_genre_index(&record) !=
            GP100_PRESET_GENRE_COUNT) {
        return 3;
    }
    gp100_sysex_encode_u16(
        GP100_PRESET_GENRE_COUNT + 1u, payload);
    if (gp100_sysex_preset_decode_update(&request, &action) !=
            GP100_SYSEX_PRESET_INVALID) {
        return 4;
    }
    return 0;
}

static int test_chain_update_and_read(void)
{
    static const uint16_t positions[GP100_EFFECT_FAMILY_COUNT] = {
        2u, 0u, 1u, 3u, 4u, 8u, 7u, 6u, 5u
    };
    gp100_preset_record_t record = {{0}};
    uint8_t payload[GP100_SYSEX_PRESET_CHAIN_BYTES];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_PRESET_CHAIN, payload, sizeof(payload)
    };
    gp100_sysex_view_t query = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_PRESET_CHAIN, NULL, 0u
    };
    gp100_sysex_preset_action_t action;
    gp100_sysex_view_t response_view;
    uint8_t response[64];
    size_t response_length;
    size_t index;

    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        gp100_sysex_encode_u16(positions[index], payload + index * 4u);
    }
    if (gp100_sysex_preset_decode_update(&command, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action) ||
        gp100_sysex_preset_build_state(
            &query, &record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_PRESET_RESPONSE ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.payload_length != sizeof(payload)) {
        return 1;
    }
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        uint16_t decoded;
        if (!gp100_sysex_decode_u16(
                response_view.payload + index * 4u, &decoded) ||
            decoded != positions[index]) {
            return 2;
        }
    }
    gp100_sysex_encode_u16(positions[0], payload + 4u);
    if (gp100_sysex_preset_decode_update(&command, &action) !=
            GP100_SYSEX_PRESET_INVALID) {
        return 3;
    }
    return 0;
}

static int test_metadata_update_and_read(void)
{
    static const uint8_t author[] = "Readable firmware";
    static const uint8_t notes[] = "Recovered live-patch metadata";
    uint8_t location[2] = {1u, 42u};
    gp100_preset_record_t record = {{0}};
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_PRESET_AUTHOR,
        author, sizeof(author) - 1u
    };
    gp100_sysex_view_t query = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_PRESET_AUTHOR,
        location, sizeof(location)
    };
    gp100_sysex_preset_action_t action;
    gp100_sysex_view_t response_view;
    uint8_t response[80];
    size_t response_length;
    char text[GP100_PRESET_NOTES_SIZE + 1u];

    if (gp100_sysex_preset_decode_update(&command, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action)) {
        return 1;
    }
    gp100_preset_author(&record, text);
    if (strcmp(text, (const char *)author) != 0 ||
        gp100_sysex_preset_build_state(
            &query, &record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_PRESET_RESPONSE ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.payload_length != GP100_PRESET_AUTHOR_SIZE ||
        memcmp(response_view.payload, location, sizeof(location)) != 0 ||
        memcmp(response_view.payload + sizeof(location),
               author, sizeof(author) - 1u) != 0) {
        return 2;
    }
    command.address = GP100_SYSEX_PRESET_NOTES;
    command.payload = notes;
    command.payload_length = sizeof(notes) - 1u;
    query.address = GP100_SYSEX_PRESET_NOTES;
    if (gp100_sysex_preset_decode_update(&command, &action) !=
            GP100_SYSEX_PRESET_ACTION ||
        !gp100_sysex_preset_apply(&record, &action) ||
        gp100_sysex_preset_build_state(
            &query, &record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_PRESET_RESPONSE ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.payload_length != GP100_PRESET_NOTES_SIZE ||
        memcmp(response_view.payload, location, sizeof(location)) != 0 ||
        memcmp(response_view.payload + sizeof(location),
               notes, sizeof(notes) - 1u) != 0) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_scalar_updates();
    if (result != 0) {
        fprintf(stderr, "scalar update test failed: %d\n", result);
        return 10 + result;
    }
    result = test_chain_update_and_read();
    if (result != 0) {
        fprintf(stderr, "chain test failed: %d\n", result);
        return 20 + result;
    }
    result = test_metadata_update_and_read();
    if (result != 0) {
        fprintf(stderr, "metadata test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
