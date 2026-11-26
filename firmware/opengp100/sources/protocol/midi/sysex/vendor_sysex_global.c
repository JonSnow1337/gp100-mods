#include "vendor_sysex_global.h"

#include <math.h>
#include <string.h>

static void encode_u32_little_endian(uint32_t value, uint8_t output[4])
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

bool gp100_sysex_global_parameter_index(
    uint32_t address,
    size_t *parameter_index)
{
    uint32_t index;

    if (parameter_index == NULL ||
        address < GP100_SYSEX_GLOBAL_PARAMETER_BASE) {
        return false;
    }
    index = address - GP100_SYSEX_GLOBAL_PARAMETER_BASE;
    if (index >= GP100_SYSEX_GLOBAL_PARAMETER_COUNT) {
        return false;
    }
    *parameter_index = (size_t)index;
    return true;
}

bool gp100_global_eq_parameter_rule(
    gp100_global_eq_parameter_t parameter,
    gp100_global_eq_parameter_rule_t *rule)
{
    gp100_global_eq_parameter_rule_t next;
    unsigned band_parameter;

    if (rule == NULL ||
        (unsigned)parameter >= GP100_SYSEX_GLOBAL_PARAMETER_COUNT) {
        return false;
    }
    if (parameter == GP100_GLOBAL_EQ_ENABLE) {
        next.minimum = 0.0f;
        next.maximum = 1.0f;
        next.step = 1.0f;
    } else if (parameter == GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY) {
        next.minimum = 19.0f; /* OFF sentinel */
        next.maximum = 20000.0f;
        next.step = 1.0f;
    } else if (parameter == GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY) {
        next.minimum = 20.0f;
        next.maximum = 20001.0f; /* OFF sentinel */
        next.step = 1.0f;
    } else {
        band_parameter = ((unsigned)parameter -
                          GP100_GLOBAL_EQ_BAND_1_FREQUENCY) % 3u;
        if (band_parameter == 0u) {
            next.minimum = 20.0f;
            next.maximum = 20000.0f;
            next.step = 1.0f;
        } else if (band_parameter == 1u) {
            next.minimum = 0.1f;
            next.maximum = 10.0f;
            next.step = 0.1f;
        } else {
            next.minimum = -20.0f;
            next.maximum = 20.0f;
            next.step = 1.0f;
        }
    }
    *rule = next;
    return true;
}

bool gp100_global_eq_parameter_value_valid(
    gp100_global_eq_parameter_t parameter,
    float value)
{
    gp100_global_eq_parameter_rule_t rule;

    if (!gp100_global_eq_parameter_rule(parameter, &rule) ||
        !isfinite(value) || value < rule.minimum || value > rule.maximum) {
        return false;
    }
    /* The factory stores this switch as a float but accepts only its two
       discrete front-panel values. Other controls are range-checked because
       SysEx may carry values between the UI's adjustment increments. */
    return parameter != GP100_GLOBAL_EQ_ENABLE ||
           value == 0.0f || value == 1.0f;
}

gp100_sysex_global_result_t gp100_sysex_global_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_global_update_t *update)
{
    size_t parameter_index;

    if (request == NULL || update == NULL) {
        return GP100_SYSEX_GLOBAL_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !gp100_sysex_global_parameter_index(
            request->address, &parameter_index)) {
        return GP100_SYSEX_GLOBAL_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 8u ||
        !gp100_sysex_decode_u32(
            request->payload, &update->value_bits)) {
        return GP100_SYSEX_GLOBAL_INVALID;
    }
    update->parameter = (gp100_global_eq_parameter_t)parameter_index;
    return GP100_SYSEX_GLOBAL_UPDATE;
}

bool gp100_sysex_global_apply_update(
    gp100_sysex_global_state_t *state,
    const gp100_sysex_global_update_t *update,
    bool *changed)
{
    uint32_t previous;
    float previous_value;
    float next_value;

    if (state == NULL || update == NULL ||
        (unsigned)update->parameter >=
            GP100_SYSEX_GLOBAL_PARAMETER_COUNT) {
        return false;
    }
    previous = state->parameters[(size_t)update->parameter];
    state->parameters[(size_t)update->parameter] = update->value_bits;
    if (changed != NULL) {
        memcpy(&previous_value, &previous, sizeof(previous_value));
        memcpy(&next_value, &update->value_bits, sizeof(next_value));
        *changed = previous_value != next_value;
    }
    return true;
}

gp100_sysex_global_result_t gp100_sysex_global_build_state(
    const gp100_sysex_view_t *request,
    const gp100_sysex_global_state_t *state,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_GLOBAL_SNAPSHOT_BYTES];
    size_t parameter_index;
    size_t payload_length;

    if (request == NULL || state == NULL || response_length == NULL) {
        return GP100_SYSEX_GLOBAL_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS) {
        return GP100_SYSEX_GLOBAL_UNHANDLED;
    }
    if (request->address == GP100_SYSEX_GLOBAL_SNAPSHOT) {
        for (parameter_index = 0u;
             parameter_index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
             ++parameter_index) {
            encode_u32_little_endian(
                state->parameters[parameter_index],
                payload + parameter_index * sizeof(uint32_t));
        }
        payload_length = sizeof(payload);
    } else if (gp100_sysex_global_parameter_index(
                   request->address, &parameter_index)) {
        gp100_sysex_encode_u32(
            state->parameters[parameter_index], payload);
        payload_length = 8u;
    } else {
        return GP100_SYSEX_GLOBAL_UNHANDLED;
    }
    if (request->payload_length != 0u ||
        !gp100_sysex_build_response(
            request->address, payload, payload_length,
            response, response_capacity, response_length)) {
        return GP100_SYSEX_GLOBAL_INVALID;
    }
    return GP100_SYSEX_GLOBAL_RESPONSE;
}
