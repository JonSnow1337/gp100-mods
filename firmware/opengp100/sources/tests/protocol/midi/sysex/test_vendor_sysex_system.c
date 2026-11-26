#include "protocol/midi/sysex/vendor_sysex_system.h"

#include <stdio.h>
#include <string.h>

static int test_language_catalog(void)
{
    gp100_language_catalog_t catalog;

    gp100_language_catalog_from_mask(0u, &catalog);
    if (catalog.count != 9u || catalog.identifiers[0] != 0u ||
        catalog.identifiers[8] != 8u) {
        return 1;
    }
    gp100_language_catalog_from_mask(
        (UINT32_C(1) << 0) | (UINT32_C(1) << 3) |
        (UINT32_C(1) << 9), &catalog);
    if (catalog.count != 3u || catalog.identifiers[0] != 0u ||
        catalog.identifiers[1] != 3u || catalog.identifiers[2] != 9u) {
        return 2;
    }
    gp100_language_catalog_from_mask(UINT32_C(0x3ff), &catalog);
    if (catalog.count != 9u || catalog.identifiers[8] != 8u) {
        return 3;
    }
    return 0;
}

static int decode_scalar(
    uint32_t address,
    int16_t value,
    gp100_sysex_system_action_t *action)
{
    uint8_t payload[4];
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS, address,
        payload, sizeof(payload)
    };

    gp100_sysex_encode_u16((uint16_t)value, payload);
    return gp100_sysex_system_decode_update(&request, action) ==
        GP100_SYSEX_SYSTEM_UPDATE ? 0 : 1;
}

static int test_scalar_updates(void)
{
    gp100_language_catalog_t languages;
    gp100_sysex_system_state_t state;
    gp100_sysex_system_action_t action;
    bool changed;

    memset(&state, 0, sizeof(state));
    gp100_language_catalog_from_mask(0u, &languages);
    if (decode_scalar(GP100_SYSEX_SYSTEM_FOOTSWITCH_MODE,
                      GP100_FOOTSWITCH_MODE_STOMP, &action) != 0 ||
        action.kind != GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, &changed) ||
        !changed || state.footswitch_mode != GP100_FOOTSWITCH_MODE_STOMP) {
        return 1;
    }
    if (!gp100_sysex_system_apply(
            &state, &languages, &action, &changed) || changed) {
        return 2;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_LANGUAGE, 4, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.language_identifier != 4u) {
        return 3;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_MASTER_VOLUME,
                      73, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.master_volume != 73) {
        return 4;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_INPUT_LEVEL,
                      -12, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.input_level_db != -12) {
        return 5;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL,
                      20, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.usb_record_level_db != 20) {
        return 6;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_USB_MONITOR_LEVEL,
                      6, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.usb_monitor_level_db != 6) {
        return 7;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT,
                      GP100_USB_RECORD_SOURCE_EFFECT, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.usb_record_source_right !=
            GP100_USB_RECORD_SOURCE_EFFECT) {
        return 8;
    }
    return 0;
}

static int test_preset_update_and_reads(void)
{
    uint8_t payload[8];
    uint8_t response[32];
    gp100_sysex_system_state_t state;
    gp100_sysex_system_action_t action;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_SYSTEM_PRESET, payload, sizeof(payload)
    };
    gp100_sysex_view_t response_view;
    uint16_t first;
    uint16_t second;
    size_t response_length;

    memset(&state, 0, sizeof(state));
    gp100_sysex_encode_u16(1u, payload);
    gp100_sysex_encode_u16(42u, payload + 4u);
    if (gp100_sysex_system_decode_update(&request, &action) !=
            GP100_SYSEX_SYSTEM_UPDATE ||
        action.kind != GP100_SYSEX_SYSTEM_RECALL_PRESET ||
        action.logical_preset != 141u ||
        !gp100_sysex_system_apply(&state, NULL, &action, NULL) ||
        state.current_preset != 141u) {
        return 1;
    }
    request.message_class = GP100_SYSEX_REQUEST_CLASS;
    request.payload = NULL;
    request.payload_length = 0u;
    if (gp100_sysex_system_build_state(
            &request, &state, response, sizeof(response), &response_length) !=
            GP100_SYSEX_SYSTEM_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.payload_length != 8u ||
        !gp100_sysex_decode_u16(response_view.payload, &first) ||
        !gp100_sysex_decode_u16(response_view.payload + 4u, &second) ||
        first != 1u || second != 42u) {
        return 2;
    }
    state.input_level_db = -7;
    state.usb_record_level_db = -8;
    state.usb_monitor_level_db = 3;
    state.usb_record_source_left = GP100_USB_RECORD_SOURCE_EFFECT;
    state.usb_record_source_right = GP100_USB_RECORD_SOURCE_DRY;
    const struct {
        uint32_t address;
        int16_t value;
    } scalar_reads[] = {
        {GP100_SYSEX_SYSTEM_INPUT_LEVEL, -7},
        {GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL, -8},
        {GP100_SYSEX_SYSTEM_USB_MONITOR_LEVEL, 3},
        {GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_LEFT, GP100_USB_RECORD_SOURCE_EFFECT},
        {GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT, GP100_USB_RECORD_SOURCE_DRY},
    };
    for (size_t index = 0u; index < sizeof(scalar_reads) / sizeof(scalar_reads[0]); ++index) {
        request.address = scalar_reads[index].address;
        if (gp100_sysex_system_build_state(
                &request, &state, response, sizeof(response), &response_length) !=
                GP100_SYSEX_SYSTEM_RESPONSE ||
            !gp100_sysex_parse(response, response_length, &response_view) ||
            response_view.address != request.address || response_view.payload_length != 4u ||
            !gp100_sysex_decode_u16(response_view.payload, &first) ||
            (int16_t)first != scalar_reads[index].value) {
            return 3;
        }
    }
    return 0;
}

static int test_factory_validation_boundaries(void)
{
    gp100_language_catalog_t languages;
    gp100_sysex_system_state_t state;
    gp100_sysex_system_action_t action;

    memset(&state, 0, sizeof(state));
    gp100_language_catalog_from_mask(0u, &languages);
    if (decode_scalar(GP100_SYSEX_SYSTEM_MASTER_VOLUME,
                      101, &action) != 0 ||
        gp100_sysex_system_apply(
            &state, &languages, &action, NULL)) {
        return 1;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL,
                      -21, &action) != 0 ||
        gp100_sysex_system_apply(
            &state, &languages, &action, NULL)) {
        return 2;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_INPUT_LEVEL,
                      -100, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.input_level_db != -100) {
        return 3;
    }
    if (decode_scalar(GP100_SYSEX_SYSTEM_FOOTSWITCH_MODE,
                      -1, &action) != 0 ||
        !gp100_sysex_system_apply(
            &state, &languages, &action, NULL) ||
        state.footswitch_mode != -1) {
        return 4;
    }
    return 0;
}

static int test_validation(void)
{
    uint8_t payload[8] = {0u};
    gp100_sysex_system_action_t action;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_SYSTEM_PRESET, payload, 7u
    };

    if (gp100_sysex_system_decode_update(&request, &action) !=
            GP100_SYSEX_SYSTEM_INVALID) {
        return 1;
    }
    request.address = GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT + 1u;
    request.payload_length = 4u;
    if (gp100_sysex_system_decode_update(&request, &action) !=
            GP100_SYSEX_SYSTEM_UNHANDLED) {
        return 2;
    }
    return 0;
}

static int test_factory_reset_action(void)
{
    uint8_t unexpected_payload = 0u;
    gp100_sysex_system_action_t action;
    gp100_sysex_system_state_t state;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_SYSTEM_FACTORY_RESET, NULL, 0u
    };

    memset(&state, 0, sizeof(state));
    if (gp100_sysex_system_decode_update(&request, &action) !=
            GP100_SYSEX_SYSTEM_UPDATE ||
        action.kind != GP100_SYSEX_SYSTEM_REQUEST_FACTORY_RESET ||
        gp100_sysex_system_apply(&state, NULL, &action, NULL)) {
        return 1;
    }
    request.payload = &unexpected_payload;
    request.payload_length = 1u;
    if (gp100_sysex_system_decode_update(&request, &action) !=
            GP100_SYSEX_SYSTEM_INVALID) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_language_catalog();
    if (result != 0) {
        fprintf(stderr, "language-catalog test failed: %d\n", result);
        return 10 + result;
    }
    result = test_scalar_updates();
    if (result != 0) {
        fprintf(stderr, "system scalar-update test failed: %d\n", result);
        return 20 + result;
    }
    result = test_preset_update_and_reads();
    if (result != 0) {
        fprintf(stderr, "system preset/read test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_validation_boundaries();
    if (result != 0) {
        fprintf(stderr, "system boundary test failed: %d\n", result);
        return 40 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "system validation test failed: %d\n", result);
        return 50 + result;
    }
    result = test_factory_reset_action();
    if (result != 0) {
        fprintf(stderr, "system factory-reset test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
