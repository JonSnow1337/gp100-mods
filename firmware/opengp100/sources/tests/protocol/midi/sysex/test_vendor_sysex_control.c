#include "protocol/midi/sysex/vendor_sysex_control.h"

#include "audio/effects/effect_catalog.h"

#include <stdio.h>
#include <string.h>

static int test_assignment(void)
{
    gp100_preset_record_t record = {{0}};
    uint8_t payload[4];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS, 0u, payload, sizeof(payload)
    };
    gp100_sysex_control_action_t action;
    uint16_t family;
    size_t footswitch;
    size_t slot;

    for (footswitch = 0u;
         footswitch < GP100_STOMP_FOOTSWITCH_COUNT;
         ++footswitch) {
        for (slot = 0u;
             slot < GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
             ++slot) {
            if (!gp100_preset_set_control_assignment(
                    &record, footswitch, slot, UINT16_MAX)) {
                return 1;
            }
        }
    }

    gp100_sysex_encode_u16(GP100_EFFECT_PRE_FX, payload);
    if (gp100_sysex_control_decode_update(&command, &action) !=
            GP100_SYSEX_CONTROL_ACTION ||
        !gp100_sysex_control_apply(&record, &action) ||
        !gp100_preset_control_assignment(
            &record, GP100_STOMP_FOOTSWITCH_FS1, 0u, &family) ||
        family != GP100_EFFECT_PRE_FX) {
        return 2;
    }

    command.address = 3u;
    gp100_sysex_encode_u16(GP100_EFFECT_DELAY, payload);
    if (gp100_sysex_control_decode_update(&command, &action) !=
            GP100_SYSEX_CONTROL_ACTION ||
        !gp100_sysex_control_apply(&record, &action) ||
        !gp100_preset_control_assignment(
            &record, GP100_STOMP_FOOTSWITCH_FS2, 0u, &family) ||
        family != GP100_EFFECT_DELAY) {
        return 3;
    }
    return 0;
}

static int test_target_round_trip(void)
{
    gp100_preset_record_t record = {{0}};
    gp100_control_target_t target = {
        true, GP100_EFFECT_CABINET, 0u, 0u
    };
    gp100_control_target_t decoded;
    gp100_sysex_view_t query = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_CONTROL_TARGET_BASE, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    uint8_t response[40];
    uint16_t family;
    uint16_t parameter;
    uint32_t algorithm;
    size_t response_length;

    if (!gp100_effect_algorithm_id(
            target.family, 0u, &target.algorithm_id) ||
        !gp100_preset_set_control_target(&record, 0u, &target) ||
        !gp100_preset_control_target(&record, 0u, &decoded) ||
        decoded.parameter_index != 0u ||
        gp100_sysex_control_build_state(
            &query, &record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_CONTROL_RESPONSE ||
        !gp100_sysex_parse(
            response, response_length, &response_view) ||
        response_view.payload_length != GP100_SYSEX_CONTROL_TARGET_BYTES ||
        !gp100_sysex_decode_u16(response_view.payload, &family) ||
        !gp100_sysex_decode_u32(
            response_view.payload + 4u, &algorithm) ||
        !gp100_sysex_decode_u16(
            response_view.payload + 12u, &parameter) ||
        family != GP100_EFFECT_CABINET ||
        algorithm != target.algorithm_id || parameter != 0u) {
        return 1;
    }
    return 0;
}

static int test_ranges_and_read_asymmetry(void)
{
    gp100_preset_record_t record = {{0}};
    uint8_t payload[8];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_CONTROL_TARGET_BASE + 7u,
        payload, sizeof(payload)
    };
    gp100_sysex_view_t query = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_CONTROL_TARGET_BASE + 7u,
        NULL, 0u
    };
    gp100_sysex_control_action_t action;
    uint8_t response[32];
    float minimum;
    float maximum;
    float value = 25.0f;
    uint32_t bits;
    size_t response_length;

    if (!gp100_preset_set_control_range(
            &record, GP100_CONTROL_RANGE_EXPRESSION_TARGET_3,
            0.0f, 100.0f)) {
        return 1;
    }
    memcpy(&bits, &value, sizeof(bits));
    gp100_sysex_encode_u32(bits, payload);
    if (gp100_sysex_control_decode_update(&command, &action) !=
            GP100_SYSEX_CONTROL_ACTION ||
        !gp100_sysex_control_apply(&record, &action) ||
        !gp100_preset_control_range(
            &record, GP100_CONTROL_RANGE_EXPRESSION_TARGET_3,
            &minimum, &maximum) ||
        minimum != value || maximum != 100.0f ||
        gp100_sysex_control_build_state(
            &query, &record, response, sizeof(response), &response_length) !=
            GP100_SYSEX_CONTROL_UNHANDLED) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_assignment();
    if (result != 0) {
        fprintf(stderr, "assignment test failed: %d\n", result);
        return 10 + result;
    }
    result = test_target_round_trip();
    if (result != 0) {
        fprintf(stderr, "target test failed: %d\n", result);
        return 20 + result;
    }
    result = test_ranges_and_read_asymmetry();
    if (result != 0) {
        fprintf(stderr, "range test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
