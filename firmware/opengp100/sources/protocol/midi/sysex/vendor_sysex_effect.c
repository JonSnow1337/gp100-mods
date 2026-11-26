#include "vendor_sysex_effect.h"

#include <math.h>
#include <string.h>

static bool decode_address(
    uint32_t address,
    gp100_effect_family_t *family,
    gp100_sysex_effect_kind_t *kind)
{
    uint32_t delta;
    uint32_t family_index;
    uint32_t suffix;

    if (address < GP100_SYSEX_EFFECT_BASE) {
        return false;
    }
    delta = address - GP100_SYSEX_EFFECT_BASE;
    family_index = delta / GP100_SYSEX_EFFECT_FAMILY_STRIDE;
    suffix = delta % GP100_SYSEX_EFFECT_FAMILY_STRIDE;
    if (family_index >= GP100_EFFECT_FAMILY_COUNT ||
        suffix > GP100_SYSEX_EFFECT_PARAMETER_SUFFIX) {
        return false;
    }
    *family = (gp100_effect_family_t)family_index;
    *kind = (gp100_sysex_effect_kind_t)suffix;
    return true;
}

uint32_t gp100_sysex_effect_address(
    gp100_effect_family_t family,
    gp100_sysex_effect_kind_t kind)
{
    if ((size_t)family >= GP100_EFFECT_FAMILY_COUNT ||
        (unsigned)kind > GP100_SYSEX_EFFECT_PARAMETER) {
        return 0u;
    }
    return GP100_SYSEX_EFFECT_BASE +
           (uint32_t)family * GP100_SYSEX_EFFECT_FAMILY_STRIDE +
           (uint32_t)kind;
}

gp100_sysex_effect_result_t gp100_sysex_effect_decode_command(
    const gp100_sysex_view_t *request,
    gp100_sysex_effect_action_t *action)
{
    uint32_t bits;
    int algorithm_index;

    if (request == NULL || action == NULL) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !decode_address(request->address, &action->family, &action->kind)) {
        return GP100_SYSEX_EFFECT_UNHANDLED;
    }
    memset(&action->enabled, 0,
           sizeof(*action) - offsetof(gp100_sysex_effect_action_t,
                                      enabled));
    if (request->payload == NULL) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    if (action->kind == GP100_SYSEX_EFFECT_ENABLE) {
        uint16_t enabled;
        if (request->payload_length != 4u ||
            !gp100_sysex_decode_u16(request->payload, &enabled) ||
            enabled > 1u) {
            return GP100_SYSEX_EFFECT_INVALID;
        }
        action->enabled = enabled != 0u;
        return GP100_SYSEX_EFFECT_ACTION;
    }
    if (request->payload_length < 8u ||
        !gp100_sysex_decode_u32(
            request->payload, &action->algorithm_id)) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    algorithm_index = gp100_effect_algorithm_index(
        action->family, action->algorithm_id);
    if (algorithm_index < 0) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    action->algorithm_index = (uint16_t)algorithm_index;
    if (action->kind == GP100_SYSEX_EFFECT_ALGORITHM) {
        return request->payload_length == 8u
                   ? GP100_SYSEX_EFFECT_ACTION
                   : GP100_SYSEX_EFFECT_INVALID;
    }
    if (request->payload_length != 20u ||
        !gp100_sysex_decode_u16(
            request->payload + 8u, &action->parameter_index) ||
        !gp100_sysex_decode_u32(request->payload + 12u, &bits) ||
        action->parameter_index >= gp100_effect_parameter_count(
            action->family, (size_t)algorithm_index)) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    memcpy(&action->value, &bits, sizeof(action->value));
    if (!isfinite(action->value)) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    return GP100_SYSEX_EFFECT_ACTION;
}

bool gp100_sysex_effect_apply(
    gp100_effect_runtime_t *runtime,
    const gp100_sysex_effect_action_t *action,
    uint32_t *apply_action,
    uint16_t *changed_slots)
{
    uint16_t next_changed_slots = 0u;

    if (runtime == NULL || action == NULL || apply_action == NULL ||
        (size_t)action->family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    if (action->kind == GP100_SYSEX_EFFECT_ENABLE) {
        if (!gp100_effect_runtime_set_enabled(
                runtime, action->family, action->enabled, apply_action)) {
            return false;
        }
    } else if (action->kind == GP100_SYSEX_EFFECT_ALGORITHM) {
        if (!gp100_effect_runtime_select_algorithm(
                runtime, action->family, action->algorithm_index,
                apply_action)) {
            return false;
        }
    } else if (action->kind == GP100_SYSEX_EFFECT_PARAMETER) {
        if (!gp100_effect_runtime_set_parameter(
                runtime, action->family, action->algorithm_index,
                action->parameter_index, action->value,
                &next_changed_slots)) {
            return false;
        }
        *apply_action = GP100_EFFECT_APPLY_COPY_PARAMETERS;
    } else {
        return false;
    }
    if (changed_slots != NULL) {
        *changed_slots = next_changed_slots;
    }
    return true;
}

gp100_sysex_effect_result_t gp100_sysex_effect_build_state(
    const gp100_sysex_view_t *request,
    const gp100_effect_runtime_t *runtime,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    const gp100_effect_slot_state_t *slot;
    gp100_effect_family_t family;
    gp100_sysex_effect_kind_t kind;
    uint8_t payload[GP100_SYSEX_EFFECT_PARAMETER_REPLY_BYTES];
    uint32_t algorithm_id;
    size_t payload_length;

    if (request == NULL || runtime == NULL || response_length == NULL) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        !decode_address(request->address, &family, &kind)) {
        return GP100_SYSEX_EFFECT_UNHANDLED;
    }
    slot = &runtime->slots[(size_t)family];
    if (kind == GP100_SYSEX_EFFECT_ENABLE) {
        if (request->payload_length != 0u) {
            return GP100_SYSEX_EFFECT_INVALID;
        }
        gp100_sysex_encode_u16(slot->enabled ? 1u : 0u, payload);
        payload_length = 4u;
    } else if (kind == GP100_SYSEX_EFFECT_ALGORITHM) {
        if (request->payload_length != 0u || !slot->algorithm_loaded ||
            !gp100_effect_algorithm_id(
                family, slot->algorithm_index, &algorithm_id)) {
            return GP100_SYSEX_EFFECT_INVALID;
        }
        gp100_sysex_encode_u32(algorithm_id, payload);
        payload_length = 8u;
    } else if (kind == GP100_SYSEX_EFFECT_PARAMETER) {
        uint32_t compact_algorithm_id;
        uint32_t request_bytes_before_end;
        int algorithm_index;

        if (request->payload == NULL ||
            request->payload_length !=
                GP100_SYSEX_EFFECT_PARAMETER_QUERY_BYTES ||
            request->payload[0] > 0x7fu || request->payload[1] > 0x7fu ||
            request->payload[2] > 0x7fu) {
            return GP100_SYSEX_EFFECT_INVALID;
        }
        compact_algorithm_id =
            ((uint32_t)request->payload[0] << 7) | request->payload[1];
        algorithm_index = gp100_effect_algorithm_index(
            family, compact_algorithm_id);
        if (algorithm_index < 0) {
            return GP100_SYSEX_EFFECT_INVALID;
        }

        /*
         * This is the shipped handler's reply, not a semantic parameter read.
         * It returns the resolved algorithm index, echoes one query byte, and
         * leaks the parser's byte count in the final word. No live value is
         * loaded by factory 0x1de8a..0x1df5e.
         */
        gp100_sysex_encode_u32((uint32_t)algorithm_index, payload);
        gp100_sysex_encode_u16(request->payload[2], payload + 8u);
        request_bytes_before_end =
            GP100_SYSEX_HEADER_BYTES + (uint32_t)request->payload_length;
        gp100_sysex_encode_u32(
            request_bytes_before_end, payload + 12u);
        payload_length = sizeof(payload);
    } else {
        return GP100_SYSEX_EFFECT_UNHANDLED;
    }
    if (!gp100_sysex_build_response(
            request->address, payload, payload_length,
            response, response_capacity, response_length)) {
        return GP100_SYSEX_EFFECT_INVALID;
    }
    return GP100_SYSEX_EFFECT_RESPONSE;
}
