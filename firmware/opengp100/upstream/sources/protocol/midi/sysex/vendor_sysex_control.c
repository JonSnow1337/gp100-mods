#include "vendor_sysex_control.h"
#include "storage/patch_chain.h"

#include <math.h>
#include <string.h>

static bool decode_address(uint32_t address,
                           gp100_sysex_control_kind_t *kind,
                           size_t *index)
{
    uint32_t delta;
    uint32_t suffix;

    if (address < GP100_SYSEX_CONTROL_ASSIGNMENT_COUNT) {
        *kind = GP100_SYSEX_CONTROL_ASSIGNMENT;
        *index = address;
        return true;
    }
    if (address == GP100_SYSEX_CONTROL_VOLUME_POSITION ||
        address == GP100_SYSEX_CONTROL_PEDAL_TARGET) {
        *kind = address == GP100_SYSEX_CONTROL_VOLUME_POSITION
            ? GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION
            : GP100_SYSEX_CONTROL_EXPRESSION_PEDAL_TARGET;
        *index = 0u;
        return true;
    }
    if (address == GP100_SYSEX_CONTROL_VOLUME_MINIMUM ||
        address == GP100_SYSEX_CONTROL_VOLUME_MAXIMUM) {
        *kind = address == GP100_SYSEX_CONTROL_VOLUME_MINIMUM
            ? GP100_SYSEX_CONTROL_RANGE_MINIMUM
            : GP100_SYSEX_CONTROL_RANGE_MAXIMUM;
        *index = GP100_CONTROL_TARGET_COUNT;
        return true;
    }
    if (address < GP100_SYSEX_CONTROL_TARGET_BASE) {
        return false;
    }
    delta = address - GP100_SYSEX_CONTROL_TARGET_BASE;
    *index = delta / GP100_SYSEX_CONTROL_TARGET_STRIDE;
    suffix = delta % GP100_SYSEX_CONTROL_TARGET_STRIDE;
    if (*index >= GP100_CONTROL_TARGET_COUNT) {
        return false;
    }
    if (suffix == 0u) {
        *kind = GP100_SYSEX_CONTROL_TARGET;
    } else if (suffix == 1u) {
        *kind = GP100_SYSEX_CONTROL_RANGE_MINIMUM;
    } else {
        *kind = GP100_SYSEX_CONTROL_RANGE_MAXIMUM;
    }
    return true;
}

static gp100_control_range_t range_for_index(size_t index)
{
    return index == GP100_CONTROL_TARGET_COUNT
        ? GP100_CONTROL_RANGE_VOLUME
        : (gp100_control_range_t)(GP100_CONTROL_RANGE_EXPRESSION_TARGET_1 + index);
}

gp100_sysex_control_result_t gp100_sysex_control_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_control_action_t *action)
{
    uint32_t bits;

    if (request == NULL || action == NULL) {
        return GP100_SYSEX_CONTROL_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !decode_address(request->address, &action->kind, &action->index)) {
        return GP100_SYSEX_CONTROL_UNHANDLED;
    }
    if (request->payload == NULL) {
        return GP100_SYSEX_CONTROL_INVALID;
    }
    if (action->kind == GP100_SYSEX_CONTROL_ASSIGNMENT) {
        if (request->payload_length != 4u ||
            !gp100_sysex_decode_u16(
                request->payload, &action->data.assignment_family)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
    } else if (action->kind == GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION ||
               action->kind == GP100_SYSEX_CONTROL_EXPRESSION_PEDAL_TARGET) {
        uint16_t selection;

        if (request->payload_length != 4u ||
            !gp100_sysex_decode_u16(request->payload, &selection) ||
            selection > 1u) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        if (action->kind == GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION) {
            action->data.volume_position = selection == 1u
                ? GP100_VOLUME_PEDAL_PRE : GP100_VOLUME_PEDAL_POST;
        } else {
            action->data.expression_target = (gp100_expression_pedal_target_t)selection;
        }
    } else if (action->kind == GP100_SYSEX_CONTROL_TARGET) {
        uint16_t family;
        if (request->payload_length != GP100_SYSEX_CONTROL_TARGET_BYTES ||
            !gp100_sysex_decode_u16(request->payload, &family) ||
            !gp100_sysex_decode_u32(
                request->payload + 4u, &action->data.target.algorithm_id) ||
            !gp100_sysex_decode_u16(
                request->payload + 12u,
                &action->data.target.parameter_index)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        if (family == UINT16_MAX) {
            memset(&action->data.target, 0, sizeof(action->data.target));
            action->data.target.assigned = false;
        } else if (family < GP100_EFFECT_FAMILY_COUNT) {
            action->data.target.assigned = true;
            action->data.target.family = (gp100_effect_family_t)family;
        } else {
            return GP100_SYSEX_CONTROL_INVALID;
        }
    } else {
        if (request->payload_length != 8u ||
            !gp100_sysex_decode_u32(request->payload, &bits)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        memcpy(&action->data.range_value, &bits,
               sizeof(action->data.range_value));
        if (!isfinite(action->data.range_value) ||
            action->data.range_value < 0.0f ||
            action->data.range_value > 100.0f) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
    }
    return GP100_SYSEX_CONTROL_ACTION;
}

bool gp100_sysex_control_apply(
    gp100_preset_record_t *record,
    const gp100_sysex_control_action_t *action)
{
    float minimum;
    float maximum;
    gp100_control_range_t range;

    if (record == NULL || action == NULL) {
        return false;
    }
    if (action->kind == GP100_SYSEX_CONTROL_ASSIGNMENT) {
        return gp100_preset_set_control_assignment(
            record,
            action->index / GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
            action->index % GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
            action->data.assignment_family);
    }
    if (action->kind == GP100_SYSEX_CONTROL_TARGET) {
        return gp100_preset_set_control_target(
            record, action->index, &action->data.target);
    }
    if (action->kind == GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION) {
        return gp100_preset_set_volume_pedal_position(
            record, action->data.volume_position);
    }
    if (action->kind == GP100_SYSEX_CONTROL_EXPRESSION_PEDAL_TARGET) {
        return gp100_patch_expression_set_mode(
            record, action->data.expression_target);
    }
    if (action->index > GP100_CONTROL_TARGET_COUNT) {
        return false;
    }
    range = range_for_index(action->index);
    if (!gp100_preset_control_range(
            record, range, &minimum, &maximum)) {
        return false;
    }
    if (action->kind == GP100_SYSEX_CONTROL_RANGE_MINIMUM) {
        minimum = action->data.range_value;
    } else if (action->kind == GP100_SYSEX_CONTROL_RANGE_MAXIMUM) {
        maximum = action->data.range_value;
    } else {
        return false;
    }
    return gp100_preset_set_control_range(
        record, range, minimum, maximum);
}

gp100_sysex_control_result_t gp100_sysex_control_build_state(
    const gp100_sysex_view_t *request,
    const gp100_preset_record_t *record,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    gp100_sysex_control_kind_t kind;
    size_t index;
    uint8_t payload[GP100_SYSEX_CONTROL_TARGET_BYTES];
    size_t payload_length;

    if (request == NULL || record == NULL || response_length == NULL) {
        return GP100_SYSEX_CONTROL_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        !decode_address(request->address, &kind, &index)) {
        return GP100_SYSEX_CONTROL_UNHANDLED;
    }
    if (request->payload_length != 0u) {
        return GP100_SYSEX_CONTROL_INVALID;
    }
    if (kind == GP100_SYSEX_CONTROL_ASSIGNMENT) {
        uint16_t family;
        if (!gp100_preset_control_assignment(
                record,
                index / GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                index % GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH,
                &family)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        gp100_sysex_encode_u16(family, payload);
        payload_length = 4u;
    } else if (kind == GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION) {
        gp100_volume_pedal_position_t position;

        if (!gp100_preset_volume_pedal_position(record, &position)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        gp100_sysex_encode_u16(
            position == GP100_VOLUME_PEDAL_PRE ? 1u : 0u, payload);
        payload_length = 4u;
    } else if (kind == GP100_SYSEX_CONTROL_EXPRESSION_PEDAL_TARGET) {
        gp100_expression_pedal_target_t target;

        if (!gp100_preset_expression_pedal_target(record, &target)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        gp100_sysex_encode_u16((uint16_t)target, payload);
        payload_length = 4u;
    } else if (kind == GP100_SYSEX_CONTROL_TARGET) {
        gp100_control_target_t target;
        if (!gp100_preset_control_target(record, index, &target)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        gp100_sysex_encode_u16(
            target.assigned ? (uint16_t)target.family : UINT16_MAX,
            payload);
        gp100_sysex_encode_u32(
            target.assigned ? target.algorithm_id : 0u,
            payload + 4u);
        gp100_sysex_encode_u16(
            target.assigned ? target.parameter_index : 0u,
            payload + 12u);
        payload_length = sizeof(payload);
    } else {
        gp100_control_range_t range;
        float minimum;
        float maximum;
        float value;
        uint32_t bits;

        /* The shipped read dispatcher omits target 3's 0x1007/0x1008 pair. */
        if (index == 2u) {
            return GP100_SYSEX_CONTROL_UNHANDLED;
        }
        range = range_for_index(index);
        if (!gp100_preset_control_range(
                record, range, &minimum, &maximum)) {
            return GP100_SYSEX_CONTROL_INVALID;
        }
        value = kind == GP100_SYSEX_CONTROL_RANGE_MINIMUM
                    ? minimum
                    : maximum;
        memcpy(&bits, &value, sizeof(bits));
        gp100_sysex_encode_u32(bits, payload);
        payload_length = 8u;
    }
    if (!gp100_sysex_build_response(
            request->address, payload, payload_length,
            response, response_capacity, response_length)) {
        return GP100_SYSEX_CONTROL_INVALID;
    }
    return GP100_SYSEX_CONTROL_RESPONSE;
}
