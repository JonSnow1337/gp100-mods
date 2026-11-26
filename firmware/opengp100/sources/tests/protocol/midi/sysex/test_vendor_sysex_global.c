#include "protocol/midi/sysex/vendor_sysex_global.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int test_individual_update_and_read(void)
{
    uint8_t payload[8];
    uint8_t response[32];
    gp100_sysex_global_state_t state = {{0u}};
    gp100_sysex_global_update_t update;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_GLOBAL_PARAMETER_BASE + 4u,
        payload, sizeof(payload)
    };
    gp100_sysex_view_t response_view;
    uint32_t decoded;
    size_t response_length;
    bool changed = false;

    gp100_sysex_encode_u32(UINT32_C(0x12345678), payload);
    if (gp100_sysex_global_decode_update(&request, &update) !=
            GP100_SYSEX_GLOBAL_UPDATE ||
        update.parameter != GP100_GLOBAL_EQ_BAND_1_Q ||
        update.value_bits != UINT32_C(0x12345678) ||
        !gp100_sysex_global_apply_update(&state, &update, &changed) ||
        !changed || state.parameters[4] != UINT32_C(0x12345678)) {
        return 1;
    }
    changed = true;
    if (!gp100_sysex_global_apply_update(&state, &update, &changed) ||
        changed) {
        return 2;
    }

    request.message_class = GP100_SYSEX_REQUEST_CLASS;
    request.payload = NULL;
    request.payload_length = 0u;
    if (gp100_sysex_global_build_state(
            &request, &state, response, sizeof(response), &response_length) !=
            GP100_SYSEX_GLOBAL_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.address != request.address ||
        response_view.payload_length != 8u ||
        !gp100_sysex_decode_u32(response_view.payload, &decoded) ||
        decoded != UINT32_C(0x12345678)) {
        return 3;
    }
    return 0;
}

static int test_snapshot(void)
{
    uint8_t response[96];
    gp100_sysex_global_state_t state;
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_GLOBAL_SNAPSHOT, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    size_t index;
    size_t response_length;

    for (index = 0u;
         index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        state.parameters[index] = UINT32_C(0x01020300) + (uint32_t)index;
    }
    if (gp100_sysex_global_build_state(
            &request, &state, response, sizeof(response), &response_length) !=
            GP100_SYSEX_GLOBAL_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.payload_length !=
            GP100_SYSEX_GLOBAL_SNAPSHOT_BYTES) {
        return 1;
    }
    for (index = 0u;
         index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        const uint8_t *word = response_view.payload + index * 4u;
        uint32_t expected = state.parameters[index];
        if (word[0] != (uint8_t)expected ||
            word[1] != (uint8_t)(expected >> 8) ||
            word[2] != (uint8_t)(expected >> 16) ||
            word[3] != (uint8_t)(expected >> 24)) {
            return 2;
        }
    }
    return 0;
}

static int test_factory_float_change_semantics(void)
{
    gp100_sysex_global_state_t state = {{0u}};
    gp100_sysex_global_update_t update = {
        0u, UINT32_C(0x80000000)
    };
    bool changed = true;

    if (!gp100_sysex_global_apply_update(&state, &update, &changed) ||
        changed || state.parameters[0] != UINT32_C(0x80000000)) {
        return 1;
    }
    update.value_bits = UINT32_C(0x7fc00000);
    if (!gp100_sysex_global_apply_update(&state, &update, &changed) ||
        !changed ||
        !gp100_sysex_global_apply_update(&state, &update, &changed) ||
        !changed) {
        return 2;
    }
    return 0;
}

static int test_validation(void)
{
    uint8_t payload[8] = {0u};
    uint8_t response[16];
    gp100_sysex_global_state_t state = {{0u}};
    gp100_sysex_global_update_t update = {
        (gp100_global_eq_parameter_t)
            GP100_SYSEX_GLOBAL_PARAMETER_COUNT,
        0u
    };
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_GLOBAL_PARAMETER_BASE - 1u,
        payload, sizeof(payload)
    };
    size_t response_length;
    size_t index;

    if (gp100_sysex_global_parameter_index(
            request.address, &index) ||
        gp100_sysex_global_decode_update(&request, &update) !=
            GP100_SYSEX_GLOBAL_UNHANDLED ||
        gp100_sysex_global_apply_update(&state, &update, NULL)) {
        return 1;
    }
    request.address = GP100_SYSEX_GLOBAL_PARAMETER_BASE;
    request.payload_length = 7u;
    if (gp100_sysex_global_decode_update(&request, &update) !=
            GP100_SYSEX_GLOBAL_INVALID) {
        return 2;
    }
    request.message_class = GP100_SYSEX_REQUEST_CLASS;
    request.payload_length = 1u;
    if (gp100_sysex_global_build_state(
            &request, &state, response, sizeof(response), &response_length) !=
            GP100_SYSEX_GLOBAL_INVALID) {
        return 3;
    }
    return 0;
}

static int test_parameter_rules(void)
{
    gp100_global_eq_parameter_rule_t rule;

    if (!gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_ENABLE, &rule) ||
        rule.minimum != 0.0f || rule.maximum != 1.0f || rule.step != 1.0f ||
        !gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY, &rule) ||
        rule.minimum != 19.0f || rule.maximum != 20000.0f ||
        rule.step != 1.0f ||
        !gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY, &rule) ||
        rule.minimum != 20.0f || rule.maximum != 20001.0f ||
        rule.step != 1.0f) {
        return 1;
    }
    if (!gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_BAND_1_FREQUENCY, &rule) ||
        rule.minimum != 20.0f || rule.maximum != 20000.0f ||
        rule.step != 1.0f ||
        !gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_BAND_2_Q, &rule) ||
        rule.minimum != 0.1f || rule.maximum != 10.0f || rule.step != 0.1f ||
        !gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_BAND_4_GAIN, &rule) ||
        rule.minimum != -20.0f || rule.maximum != 20.0f ||
        rule.step != 1.0f) {
        return 2;
    }
    if (gp100_global_eq_parameter_rule(
            (gp100_global_eq_parameter_t)
                GP100_SYSEX_GLOBAL_PARAMETER_COUNT,
            &rule) ||
        gp100_global_eq_parameter_rule(
            GP100_GLOBAL_EQ_ENABLE, NULL) ||
        gp100_global_eq_parameter_value_valid(
            GP100_GLOBAL_EQ_ENABLE, NAN) ||
        gp100_global_eq_parameter_value_valid(
            GP100_GLOBAL_EQ_ENABLE, 0.5f) ||
        gp100_global_eq_parameter_value_valid(
            GP100_GLOBAL_EQ_BAND_1_Q, 10.1f) ||
        !gp100_global_eq_parameter_value_valid(
            GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY, 19.0f) ||
        !gp100_global_eq_parameter_value_valid(
            GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY, 20001.0f)) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_individual_update_and_read();
    if (result != 0) {
        fprintf(stderr, "individual global parameter test failed: %d\n",
                result);
        return 10 + result;
    }
    result = test_snapshot();
    if (result != 0) {
        fprintf(stderr, "global snapshot test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "global validation test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_float_change_semantics();
    if (result != 0) {
        fprintf(stderr, "global float-change test failed: %d\n", result);
        return 40 + result;
    }
    result = test_parameter_rules();
    if (result != 0) {
        fprintf(stderr, "global parameter-rule test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
