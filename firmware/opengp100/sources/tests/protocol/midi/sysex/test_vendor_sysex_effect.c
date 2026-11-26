#include "protocol/midi/sysex/vendor_sysex_effect.h"

#include <stdio.h>
#include <string.h>

static int test_command_decode(void)
{
    uint8_t payload[20];
    gp100_sysex_view_t request = {
        0x7fu, GP100_SYSEX_RESPONSE_CLASS, 0u, payload, 4u
    };
    gp100_sysex_effect_action_t action;
    uint32_t algorithm_id;
    uint32_t value_bits;
    float value = 0.5f;

    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_ENABLE);
    gp100_sysex_encode_u16(1u, payload);
    if (gp100_sysex_effect_decode_command(&request, &action) !=
            GP100_SYSEX_EFFECT_ACTION ||
        action.kind != GP100_SYSEX_EFFECT_ENABLE ||
        action.family != GP100_EFFECT_DELAY || !action.enabled) {
        return 1;
    }

    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 0u, &algorithm_id)) {
        return 2;
    }
    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_ALGORITHM);
    request.payload_length = 8u;
    gp100_sysex_encode_u32(algorithm_id, payload);
    if (gp100_sysex_effect_decode_command(&request, &action) !=
            GP100_SYSEX_EFFECT_ACTION ||
        action.algorithm_id != algorithm_id || action.algorithm_index != 0u) {
        return 3;
    }

    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_PARAMETER);
    request.payload_length = sizeof(payload);
    gp100_sysex_encode_u32(algorithm_id, payload);
    gp100_sysex_encode_u16(0u, payload + 8u);
    memcpy(&value_bits, &value, sizeof(value_bits));
    gp100_sysex_encode_u32(value_bits, payload + 12u);
    if (gp100_sysex_effect_decode_command(&request, &action) !=
            GP100_SYSEX_EFFECT_ACTION ||
        action.kind != GP100_SYSEX_EFFECT_PARAMETER ||
        action.parameter_index != 0u || action.value != value) {
        return 4;
    }
    return 0;
}

static int test_state_response(void)
{
    gp100_effect_runtime_t runtime;
    gp100_sysex_view_t request = {
        1u, GP100_SYSEX_REQUEST_CLASS, 0u, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    uint8_t response[32];
    uint32_t algorithm_id;
    uint32_t decoded_id;
    uint16_t enabled;
    size_t response_length;

    gp100_effect_runtime_reset(&runtime);
    runtime.slots[GP100_EFFECT_MODULATION].enabled = true;
    runtime.slots[GP100_EFFECT_MODULATION].algorithm_loaded = true;
    runtime.slots[GP100_EFFECT_MODULATION].algorithm_index = 1u;

    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_MODULATION, GP100_SYSEX_EFFECT_ENABLE);
    if (gp100_sysex_effect_build_state(
            &request, &runtime, response, sizeof(response), &response_length) !=
            GP100_SYSEX_EFFECT_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        !gp100_sysex_decode_u16(response_view.payload, &enabled) ||
        enabled != 1u) {
        return 1;
    }

    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_MODULATION, GP100_SYSEX_EFFECT_ALGORITHM);
    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_MODULATION, 1u, &algorithm_id) ||
        gp100_sysex_effect_build_state(
            &request, &runtime, response, sizeof(response), &response_length) !=
            GP100_SYSEX_EFFECT_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        !gp100_sysex_decode_u32(response_view.payload, &decoded_id) ||
        decoded_id != algorithm_id) {
        return 2;
    }
    return 0;
}

static int test_factory_parameter_query_reply(void)
{
    gp100_effect_runtime_t runtime;
    uint8_t query[GP100_SYSEX_EFFECT_PARAMETER_QUERY_BYTES] = {
        0u, 0x1au, 2u
    };
    gp100_sysex_view_t request = {
        1u, GP100_SYSEX_REQUEST_CLASS, 0u,
        query, sizeof(query)
    };
    gp100_sysex_view_t response_view;
    uint8_t response[48];
    uint32_t algorithm_index;
    uint32_t parser_count;
    uint16_t query_byte;
    size_t response_length;

    gp100_effect_runtime_reset(&runtime);
    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_PRE_FX, GP100_SYSEX_EFFECT_PARAMETER);
    if (gp100_sysex_effect_build_state(
            &request, &runtime, response, sizeof(response), &response_length) !=
            GP100_SYSEX_EFFECT_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.payload_length !=
            GP100_SYSEX_EFFECT_PARAMETER_REPLY_BYTES ||
        !gp100_sysex_decode_u32(
            response_view.payload, &algorithm_index) ||
        !gp100_sysex_decode_u16(
            response_view.payload + 8u, &query_byte) ||
        !gp100_sysex_decode_u32(
            response_view.payload + 12u, &parser_count) ||
        algorithm_index != 2u || query_byte != 2u ||
        parser_count != GP100_SYSEX_HEADER_BYTES + sizeof(query)) {
        return 1;
    }

    query[0] = 0x7fu;
    query[1] = 0x7fu;
    if (gp100_sysex_effect_build_state(
            &request, &runtime, response, sizeof(response), &response_length) !=
            GP100_SYSEX_EFFECT_INVALID) {
        return 2;
    }
    return 0;
}

static int test_action_apply(void)
{
    uint8_t payload[20];
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS, 0u, payload, 8u
    };
    gp100_sysex_effect_action_t decoded;
    gp100_effect_runtime_t runtime;
    gp100_parameter_rule_t rule;
    uint32_t algorithm_id;
    uint32_t value_bits;
    uint32_t apply_action = 0u;
    uint16_t changed_slots = 0u;

    gp100_effect_runtime_reset(&runtime);
    if (!gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 0u, &algorithm_id)) {
        return 1;
    }
    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_ALGORITHM);
    gp100_sysex_encode_u32(algorithm_id, payload);
    if (gp100_sysex_effect_decode_command(&request, &decoded) !=
            GP100_SYSEX_EFFECT_ACTION ||
        !gp100_sysex_effect_apply(
            &runtime, &decoded, &apply_action, &changed_slots) ||
        !runtime.slots[GP100_EFFECT_DELAY].algorithm_loaded ||
        (apply_action & GP100_EFFECT_APPLY_LOAD_ALGORITHM) == 0u ||
        changed_slots != 0u) {
        return 2;
    }

    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_ENABLE);
    request.payload_length = 4u;
    gp100_sysex_encode_u16(1u, payload);
    if (gp100_sysex_effect_decode_command(&request, &decoded) !=
            GP100_SYSEX_EFFECT_ACTION ||
        !gp100_sysex_effect_apply(
            &runtime, &decoded, &apply_action, &changed_slots) ||
        apply_action != GP100_EFFECT_APPLY_ENABLE ||
        !runtime.slots[GP100_EFFECT_DELAY].enabled) {
        return 3;
    }

    if (!gp100_effect_parameter_rule(
            GP100_EFFECT_DELAY, 0u, 0u,
            runtime.slots[GP100_EFFECT_DELAY].parameters, &rule)) {
        return 4;
    }
    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_PARAMETER);
    request.payload_length = sizeof(payload);
    gp100_sysex_encode_u32(algorithm_id, payload);
    gp100_sysex_encode_u16(0u, payload + 8u);
    memcpy(&value_bits, &rule.maximum, sizeof(value_bits));
    gp100_sysex_encode_u32(value_bits, payload + 12u);
    if (gp100_sysex_effect_decode_command(&request, &decoded) !=
            GP100_SYSEX_EFFECT_ACTION ||
        !gp100_sysex_effect_apply(
            &runtime, &decoded, &apply_action, &changed_slots) ||
        apply_action != GP100_EFFECT_APPLY_COPY_PARAMETERS ||
        (changed_slots & (UINT16_C(1) << rule.storage_slot)) == 0u ||
        runtime.slots[GP100_EFFECT_DELAY]
                .parameters[rule.storage_slot] != rule.maximum) {
        return 5;
    }
    return 0;
}

static int test_validation(void)
{
    uint8_t payload[8];
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_EFFECT_BASE, payload, sizeof(payload)
    };
    gp100_sysex_effect_action_t action;

    gp100_sysex_encode_u32(UINT32_C(0xffffffff), payload);
    request.address = gp100_sysex_effect_address(
        GP100_EFFECT_PRE_FX, GP100_SYSEX_EFFECT_ALGORITHM);
    if (gp100_sysex_effect_decode_command(&request, &action) !=
            GP100_SYSEX_EFFECT_INVALID ||
        gp100_sysex_effect_address(
            (gp100_effect_family_t)9,
            GP100_SYSEX_EFFECT_ENABLE) != 0u) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_command_decode();
    if (result != 0) {
        fprintf(stderr, "command decode test failed: %d\n", result);
        return 10 + result;
    }
    result = test_state_response();
    if (result != 0) {
        fprintf(stderr, "state response test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_parameter_query_reply();
    if (result != 0) {
        fprintf(stderr, "factory parameter-query test failed: %d\n", result);
        return 40 + result;
    }
    result = test_action_apply();
    if (result != 0) {
        fprintf(stderr, "action apply test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
