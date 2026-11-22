#include "vendor_sysex_system.h"

#include "storage/preset_record.h"

static bool scalar_action_for_address(
    uint32_t address,
    gp100_sysex_system_action_kind_t *kind)
{
    if (kind == NULL) {
        return false;
    }
    switch (address) {
    case GP100_SYSEX_SYSTEM_FOOTSWITCH_MODE:
        *kind = GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE;
        return true;
    case GP100_SYSEX_SYSTEM_LANGUAGE:
        *kind = GP100_SYSEX_SYSTEM_SET_LANGUAGE;
        return true;
    case GP100_SYSEX_SYSTEM_MASTER_VOLUME:
        *kind = GP100_SYSEX_SYSTEM_SET_MASTER_VOLUME;
        return true;
    case GP100_SYSEX_SYSTEM_INPUT_LEVEL:
        *kind = GP100_SYSEX_SYSTEM_SET_INPUT_LEVEL;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL:
        *kind = GP100_SYSEX_SYSTEM_SET_USB_RECORD_LEVEL;
        return true;
    case GP100_SYSEX_SYSTEM_USB_MONITOR_LEVEL:
        *kind = GP100_SYSEX_SYSTEM_SET_USB_MONITOR_LEVEL;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_LEFT:
        *kind = GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_LEFT;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT:
        *kind = GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_RIGHT;
        return true;
    default:
        return false;
    }
}

static bool readable_scalar(
    uint32_t address,
    const gp100_sysex_system_state_t *state,
    int16_t *value)
{
    if (state == NULL || value == NULL) {
        return false;
    }
    switch (address) {
    case GP100_SYSEX_SYSTEM_FOOTSWITCH_MODE:
        *value = state->footswitch_mode;
        return true;
    case GP100_SYSEX_SYSTEM_LANGUAGE:
        *value = (int16_t)state->language_identifier;
        return true;
    case GP100_SYSEX_SYSTEM_MASTER_VOLUME:
        *value = state->master_volume;
        return true;
    case GP100_SYSEX_SYSTEM_INPUT_LEVEL:
        *value = state->input_level_db;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL:
        *value = state->usb_record_level_db;
        return true;
    case GP100_SYSEX_SYSTEM_USB_MONITOR_LEVEL:
        *value = state->usb_monitor_level_db;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_LEFT:
        *value = state->usb_record_source_left;
        return true;
    case GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT:
        *value = state->usb_record_source_right;
        return true;
    default:
        return false;
    }
}

void gp100_language_catalog_from_mask(
    uint32_t availability_mask,
    gp100_language_catalog_t *catalog)
{
    uint32_t low_ten_bits;
    uint32_t remaining;
    uint8_t identifier;

    if (catalog == NULL) {
        return;
    }
    catalog->count = 0u;
    low_ten_bits = availability_mask & UINT32_C(0x3ff);
    if (availability_mask == 0u ||
        availability_mask == UINT32_MAX ||
        low_ten_bits == UINT32_C(0x2ff) ||
        low_ten_bits == UINT32_C(0x37f) ||
        low_ten_bits == UINT32_C(0x3ff)) {
        for (identifier = 0u; identifier < 9u; ++identifier) {
            catalog->identifiers[catalog->count++] = identifier;
        }
        return;
    }

    remaining = availability_mask;
    identifier = 0u;
    while (remaining != 0u &&
           catalog->count < GP100_SYSEX_SYSTEM_MAX_LANGUAGES) {
        if ((remaining & 1u) != 0u) {
            catalog->identifiers[catalog->count++] = identifier;
        }
        remaining >>= 1;
        ++identifier;
    }
}

gp100_sysex_system_result_t gp100_sysex_system_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_system_action_t *action)
{
    gp100_sysex_system_action_kind_t kind;
    uint16_t first;
    uint16_t second;
    uint32_t logical_preset;

    if (request == NULL || action == NULL) {
        return GP100_SYSEX_SYSTEM_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS) {
        return GP100_SYSEX_SYSTEM_UNHANDLED;
    }
    action->value = 0;
    action->logical_preset = 0u;
    if (request->address == GP100_SYSEX_SYSTEM_FACTORY_RESET) {
        if (request->payload_length != 0u) {
            return GP100_SYSEX_SYSTEM_INVALID;
        }
        action->kind = GP100_SYSEX_SYSTEM_REQUEST_FACTORY_RESET;
        return GP100_SYSEX_SYSTEM_UPDATE;
    }
    if (request->address == GP100_SYSEX_SYSTEM_PRESET) {
        if (request->payload == NULL || request->payload_length != 8u ||
            !gp100_sysex_decode_u16(request->payload, &first) ||
            !gp100_sysex_decode_u16(request->payload + 4u, &second)) {
            return GP100_SYSEX_SYSTEM_INVALID;
        }
        logical_preset = (uint32_t)first *
            GP100_PRESETS_PER_BANK + second;
        if (logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
            return GP100_SYSEX_SYSTEM_INVALID;
        }
        action->kind = GP100_SYSEX_SYSTEM_RECALL_PRESET;
        action->logical_preset = (uint16_t)logical_preset;
        return GP100_SYSEX_SYSTEM_UPDATE;
    }
    if (!scalar_action_for_address(request->address, &kind)) {
        return GP100_SYSEX_SYSTEM_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 4u ||
        !gp100_sysex_decode_u16(request->payload, &first)) {
        return GP100_SYSEX_SYSTEM_INVALID;
    }
    action->kind = kind;
    action->value = (int16_t)first;
    return GP100_SYSEX_SYSTEM_UPDATE;
}

static bool select_language(
    const gp100_language_catalog_t *languages,
    int16_t requested,
    uint8_t *selected)
{
    size_t index;

    if (languages == NULL || selected == NULL || languages->count == 0u ||
        languages->count > GP100_SYSEX_SYSTEM_MAX_LANGUAGES ||
        requested >= (int16_t)languages->count) {
        return false;
    }
    index = 0u;
    if (requested >= 0) {
        size_t candidate;
        for (candidate = 0u; candidate < languages->count; ++candidate) {
            if (languages->identifiers[candidate] == (uint8_t)requested) {
                index = candidate;
                break;
            }
        }
    }
    *selected = languages->identifiers[index];
    return true;
}

bool gp100_sysex_system_apply(
    gp100_sysex_system_state_t *state,
    const gp100_language_catalog_t *languages,
    const gp100_sysex_system_action_t *action,
    bool *changed)
{
    bool value_changed;
    uint8_t language;

    if (state == NULL || action == NULL) {
        return false;
    }
    switch (action->kind) {
    case GP100_SYSEX_SYSTEM_RECALL_PRESET:
        if (action->logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
            return false;
        }
        value_changed = state->current_preset != action->logical_preset;
        state->current_preset = action->logical_preset;
        break;
    case GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE:
        if (action->value > 1) {
            return false;
        }
        value_changed = state->footswitch_mode != action->value;
        state->footswitch_mode = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_LANGUAGE:
        if (!select_language(languages, action->value, &language)) {
            return false;
        }
        value_changed = state->language_identifier != language;
        state->language_identifier = language;
        break;
    case GP100_SYSEX_SYSTEM_SET_MASTER_VOLUME:
        if (action->value > 100) {
            return false;
        }
        value_changed = state->master_volume != action->value;
        state->master_volume = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_INPUT_LEVEL:
        if (action->value > 20) {
            return false;
        }
        value_changed = state->input_level_db != action->value;
        state->input_level_db = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_LEVEL:
        if (action->value < -20 || action->value > 20) {
            return false;
        }
        value_changed = state->usb_record_level_db != action->value;
        state->usb_record_level_db = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_USB_MONITOR_LEVEL:
        value_changed = state->usb_monitor_level_db != action->value;
        state->usb_monitor_level_db = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_LEFT:
        if (action->value > 1) {
            return false;
        }
        value_changed = state->usb_record_source_left != action->value;
        state->usb_record_source_left = action->value;
        break;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_RIGHT:
        if (action->value > 1) {
            return false;
        }
        value_changed = state->usb_record_source_right != action->value;
        state->usb_record_source_right = action->value;
        break;
    case GP100_SYSEX_SYSTEM_REQUEST_FACTORY_RESET:
        /* Destructive storage erasure and reboot require board policy. */
        return false;
    default:
        return false;
    }
    if (changed != NULL) {
        *changed = value_changed;
    }
    return true;
}

gp100_sysex_system_result_t gp100_sysex_system_build_state(
    const gp100_sysex_view_t *request,
    const gp100_sysex_system_state_t *state,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[8];
    size_t payload_length;
    int16_t scalar;

    if (request == NULL || state == NULL || response_length == NULL) {
        return GP100_SYSEX_SYSTEM_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS) {
        return GP100_SYSEX_SYSTEM_UNHANDLED;
    }
    if (request->payload_length != 0u) {
        return GP100_SYSEX_SYSTEM_INVALID;
    }
    if (request->address == GP100_SYSEX_SYSTEM_PRESET) {
        if (state->current_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
            return GP100_SYSEX_SYSTEM_INVALID;
        }
        gp100_sysex_encode_u16(
            (uint16_t)(state->current_preset /
                       GP100_PRESETS_PER_BANK),
            payload);
        gp100_sysex_encode_u16(
            (uint16_t)(state->current_preset %
                       GP100_PRESETS_PER_BANK),
            payload + 4u);
        payload_length = 8u;
    } else if (readable_scalar(request->address, state, &scalar)) {
        gp100_sysex_encode_u16((uint16_t)scalar, payload);
        payload_length = 4u;
    } else {
        return GP100_SYSEX_SYSTEM_UNHANDLED;
    }
    if (!gp100_sysex_build_response(
            request->address, payload, payload_length,
            response, response_capacity, response_length)) {
        return GP100_SYSEX_SYSTEM_INVALID;
    }
    return GP100_SYSEX_SYSTEM_RESPONSE;
}
