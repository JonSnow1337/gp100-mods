#include "vendor_sysex_preset.h"

#include <string.h>

static bool address_kind(uint32_t address,
                         gp100_sysex_preset_kind_t *kind)
{
    switch (address) {
    case GP100_SYSEX_PRESET_CHAIN:
        *kind = GP100_SYSEX_PRESET_KIND_CHAIN;
        return true;
    case GP100_SYSEX_PRESET_TEMPO:
        *kind = GP100_SYSEX_PRESET_KIND_TEMPO;
        return true;
    case GP100_SYSEX_PRESET_LEVEL:
        *kind = GP100_SYSEX_PRESET_KIND_LEVEL;
        return true;
    case GP100_SYSEX_PRESET_AUTHOR:
        *kind = GP100_SYSEX_PRESET_KIND_AUTHOR;
        return true;
    case GP100_SYSEX_PRESET_GENRE:
        *kind = GP100_SYSEX_PRESET_KIND_GENRE;
        return true;
    case GP100_SYSEX_PRESET_NOTES:
        *kind = GP100_SYSEX_PRESET_KIND_NOTES;
        return true;
    default:
        return false;
    }
}

static bool decode_chain(const uint8_t *payload,
                         uint16_t positions[GP100_EFFECT_FAMILY_COUNT])
{
    uint16_t seen = 0u;
    size_t family_index;

    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        uint16_t position;
        uint16_t mask;
        if (!gp100_sysex_decode_u16(
                payload + family_index * 4u, &position) ||
            position >= GP100_EFFECT_FAMILY_COUNT) {
            return false;
        }
        mask = (uint16_t)(UINT16_C(1) << position);
        if ((seen & mask) != 0u) {
            return false;
        }
        seen = (uint16_t)(seen | mask);
        positions[family_index] = position;
    }
    return true;
}

gp100_sysex_preset_result_t gp100_sysex_preset_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_preset_action_t *action)
{
    size_t maximum_text_length;

    if (request == NULL || action == NULL) {
        return GP100_SYSEX_PRESET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !address_kind(request->address, &action->kind)) {
        return GP100_SYSEX_PRESET_UNHANDLED;
    }
    if (request->payload_length != 0u && request->payload == NULL) {
        return GP100_SYSEX_PRESET_INVALID;
    }
    memset(&action->data, 0, sizeof(action->data));
    if (action->kind == GP100_SYSEX_PRESET_KIND_CHAIN) {
        if (request->payload_length != GP100_SYSEX_PRESET_CHAIN_BYTES ||
            !decode_chain(request->payload, action->data.positions)) {
            return GP100_SYSEX_PRESET_INVALID;
        }
        return GP100_SYSEX_PRESET_ACTION;
    }
    if (action->kind == GP100_SYSEX_PRESET_KIND_TEMPO ||
        action->kind == GP100_SYSEX_PRESET_KIND_LEVEL ||
        action->kind == GP100_SYSEX_PRESET_KIND_GENRE) {
        if (request->payload_length != 4u ||
            !gp100_sysex_decode_u16(
                request->payload, &action->data.value) ||
            (action->kind == GP100_SYSEX_PRESET_KIND_TEMPO &&
             (action->data.value < 40u || action->data.value > 250u)) ||
            (action->kind == GP100_SYSEX_PRESET_KIND_LEVEL &&
             action->data.value > 100u) ||
            (action->kind == GP100_SYSEX_PRESET_KIND_GENRE &&
             action->data.value > GP100_PRESET_GENRE_COUNT)) {
            return GP100_SYSEX_PRESET_INVALID;
        }
        return GP100_SYSEX_PRESET_ACTION;
    }
    maximum_text_length =
        action->kind == GP100_SYSEX_PRESET_KIND_AUTHOR
            ? GP100_PRESET_AUTHOR_SIZE
            : GP100_PRESET_NOTES_SIZE;
    if (request->payload_length > maximum_text_length) {
        return GP100_SYSEX_PRESET_INVALID;
    }
    if (request->payload_length != 0u) {
        memcpy(action->data.text.bytes, request->payload,
               request->payload_length);
    }
    action->data.text.length = request->payload_length;
    return GP100_SYSEX_PRESET_ACTION;
}

bool gp100_sysex_preset_apply(
    gp100_preset_record_t *record,
    const gp100_sysex_preset_action_t *action)
{
    if (record == NULL || action == NULL) {
        return false;
    }
    switch (action->kind) {
    case GP100_SYSEX_PRESET_KIND_CHAIN:
        return gp100_preset_set_effect_positions(
            record, action->data.positions);
    case GP100_SYSEX_PRESET_KIND_TEMPO:
        return gp100_preset_set_tempo_bpm(
            record, action->data.value);
    case GP100_SYSEX_PRESET_KIND_LEVEL:
        return gp100_preset_set_level(record, action->data.value);
    case GP100_SYSEX_PRESET_KIND_AUTHOR:
        return gp100_preset_update_author(
            record, action->data.text.bytes, action->data.text.length);
    case GP100_SYSEX_PRESET_KIND_GENRE:
        return gp100_preset_set_genre_index(
            record, action->data.value);
    case GP100_SYSEX_PRESET_KIND_NOTES:
        return gp100_preset_update_notes(
            record, action->data.text.bytes, action->data.text.length);
    default:
        return false;
    }
}

static void copy_sanitized_text(uint8_t *destination,
                                const uint8_t *source,
                                size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        uint8_t byte = source[index];
        destination[index] = byte <= 0x7eu ? byte : 0u;
    }
}

gp100_sysex_preset_result_t gp100_sysex_preset_build_state(
    const gp100_sysex_view_t *request,
    const gp100_preset_record_t *record,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    gp100_sysex_preset_kind_t kind;
    uint8_t payload[GP100_PRESET_NOTES_SIZE] = {0};
    size_t payload_length;
    size_t family_index;

    if (request == NULL || record == NULL || response_length == NULL) {
        return GP100_SYSEX_PRESET_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        !address_kind(request->address, &kind)) {
        return GP100_SYSEX_PRESET_UNHANDLED;
    }
    if (kind == GP100_SYSEX_PRESET_KIND_CHAIN) {
        if (request->payload_length != 0u) {
            return GP100_SYSEX_PRESET_INVALID;
        }
        for (family_index = 0u;
             family_index < GP100_EFFECT_FAMILY_COUNT;
             ++family_index) {
            size_t position;
            if (!gp100_preset_effect_position(
                    record, (gp100_effect_family_t)family_index,
                    &position)) {
                return GP100_SYSEX_PRESET_INVALID;
            }
            gp100_sysex_encode_u16(
                (uint16_t)position, payload + family_index * 4u);
        }
        payload_length = GP100_SYSEX_PRESET_CHAIN_BYTES;
    } else if (kind == GP100_SYSEX_PRESET_KIND_TEMPO ||
               kind == GP100_SYSEX_PRESET_KIND_LEVEL ||
               kind == GP100_SYSEX_PRESET_KIND_GENRE) {
        uint16_t value;
        if (request->payload_length != 0u) {
            return GP100_SYSEX_PRESET_INVALID;
        }
        if (kind == GP100_SYSEX_PRESET_KIND_TEMPO) {
            value = gp100_preset_tempo_bpm(record);
        } else if (kind == GP100_SYSEX_PRESET_KIND_LEVEL) {
            value = gp100_preset_level(record);
        } else {
            value = gp100_preset_genre_index(record);
        }
        gp100_sysex_encode_u16(value, payload);
        payload_length = 4u;
    } else {
        size_t text_offset;
        if (request->payload == NULL ||
            request->payload_length !=
                GP100_SYSEX_PRESET_LOCATION_BYTES) {
            return GP100_SYSEX_PRESET_INVALID;
        }
        memcpy(payload, request->payload,
               GP100_SYSEX_PRESET_LOCATION_BYTES);
        if (kind == GP100_SYSEX_PRESET_KIND_AUTHOR) {
            text_offset = GP100_PRESET_AUTHOR_OFFSET;
            payload_length = GP100_PRESET_AUTHOR_SIZE;
        } else {
            text_offset = GP100_PRESET_NOTES_OFFSET;
            payload_length = GP100_PRESET_NOTES_SIZE;
        }
        copy_sanitized_text(
            payload + GP100_SYSEX_PRESET_LOCATION_BYTES,
            record->bytes + text_offset,
            payload_length - GP100_SYSEX_PRESET_LOCATION_BYTES);
    }
    if (!gp100_sysex_build_response(
            request->address, payload, payload_length,
            response, response_capacity, response_length)) {
        return GP100_SYSEX_PRESET_INVALID;
    }
    return GP100_SYSEX_PRESET_RESPONSE;
}
