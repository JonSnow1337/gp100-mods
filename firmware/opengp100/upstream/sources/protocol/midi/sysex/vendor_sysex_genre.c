#include "vendor_sysex_genre.h"

#include <string.h>

static const char factory_system_names[GP100_SYSTEM_GENRE_COUNT]
                                      [GP100_GENRE_NAME_SIZE] = {
    "Metal", "World", "Indie", "Country", "Rock", "Funk",
    "Pop", "Blues", "Jazz", "Bass", "Acoustic"
};

static void copy_name_update(uint8_t destination[GP100_GENRE_NAME_SIZE],
                             const uint8_t *source,
                             size_t length)
{
    size_t index;

    for (index = 0u; index < GP100_GENRE_NAME_SIZE; ++index) {
        uint8_t byte = index < length ? source[index] : 0u;
        destination[index] = byte;
        if (byte == 0u) {
            break;
        }
    }
}

static void copy_sanitized_name(uint8_t *destination,
                                const uint8_t *source,
                                size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        destination[index] = source[index] <= 0x7eu ? source[index] : 0u;
    }
}

void gp100_genre_catalog_reset(gp100_genre_catalog_t *catalog)
{
    if (catalog != NULL) {
        memset(catalog, 0, sizeof(*catalog));
    }
}

bool gp100_genre_catalog_make_factory(
    gp100_genre_catalog_t *catalog)
{
    size_t index;

    if (catalog == NULL) {
        return false;
    }
    memset(catalog, 0, sizeof(*catalog));
    for (index = 0u; index < GP100_SYSTEM_GENRE_COUNT; ++index) {
        memcpy(catalog->names[index], factory_system_names[index],
               GP100_GENRE_NAME_SIZE);
    }
    return true;
}

bool gp100_genre_name(
    const gp100_genre_catalog_t *catalog,
    uint16_t index,
    char name[GP100_GENRE_NAME_SIZE + 1u])
{
    size_t byte_index;

    if (catalog == NULL || name == NULL ||
        index >= GP100_PRESET_GENRE_COUNT) {
        return false;
    }
    for (byte_index = 0u;
         byte_index < GP100_GENRE_NAME_SIZE;
         ++byte_index) {
        uint8_t byte = catalog->names[index][byte_index];
        if (byte == 0u || byte == UINT8_MAX) {
            break;
        }
        name[byte_index] = (char)byte;
    }
    name[byte_index] = '\0';
    return true;
}

gp100_sysex_genre_result_t gp100_sysex_genre_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_genre_update_t *update)
{
    if (request == NULL || update == NULL) {
        return GP100_SYSEX_GENRE_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        (request->address != GP100_SYSEX_GENRE_NAME &&
         request->address != GP100_SYSEX_GENRE_NAME_CHANGED)) {
        return GP100_SYSEX_GENRE_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length < GP100_SYSEX_GENRE_INDEX_BYTES ||
        request->payload_length > GP100_SYSEX_GENRE_READ_BYTES ||
        !gp100_sysex_decode_u16(request->payload, &update->index) ||
        update->index < GP100_SYSTEM_GENRE_COUNT ||
        update->index >= GP100_PRESET_GENRE_COUNT) {
        return GP100_SYSEX_GENRE_INVALID;
    }
    memset(update->name, 0, sizeof(update->name));
    update->name_length = request->payload_length -
                          GP100_SYSEX_GENRE_INDEX_BYTES;
    if (update->name_length != 0u) {
        memcpy(update->name,
               request->payload + GP100_SYSEX_GENRE_INDEX_BYTES,
               update->name_length);
    }
    return GP100_SYSEX_GENRE_UPDATE;
}

bool gp100_sysex_genre_apply(
    gp100_genre_catalog_t *catalog,
    gp100_preset_record_t *current_preset,
    const gp100_sysex_genre_update_t *update)
{
    if (catalog == NULL || current_preset == NULL || update == NULL ||
        update->index < GP100_SYSTEM_GENRE_COUNT ||
        update->index >= GP100_PRESET_GENRE_COUNT ||
        update->name_length > GP100_GENRE_NAME_SIZE) {
        return false;
    }
    copy_name_update(catalog->names[update->index],
                     update->name, update->name_length);
    return gp100_preset_set_genre_index(
        current_preset, update->index);
}

gp100_sysex_genre_result_t gp100_sysex_genre_build_state(
    const gp100_sysex_view_t *request,
    const gp100_genre_catalog_t *catalog,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_GENRE_READ_BYTES];
    uint16_t index;

    if (request == NULL || catalog == NULL || response_length == NULL) {
        return GP100_SYSEX_GENRE_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        request->address != GP100_SYSEX_GENRE_NAME) {
        return GP100_SYSEX_GENRE_UNHANDLED;
    }
    if (request->payload == NULL ||
        request->payload_length != GP100_SYSEX_GENRE_INDEX_BYTES ||
        !gp100_sysex_decode_u16(request->payload, &index) ||
        index >= GP100_PRESET_GENRE_COUNT) {
        return GP100_SYSEX_GENRE_INVALID;
    }
    gp100_sysex_encode_u16(index, payload);
    copy_sanitized_name(payload + GP100_SYSEX_GENRE_INDEX_BYTES,
                        catalog->names[index], GP100_GENRE_NAME_SIZE);
    if (!gp100_sysex_build_response(
            request->address, payload, sizeof(payload),
            response, response_capacity, response_length)) {
        return GP100_SYSEX_GENRE_INVALID;
    }
    return GP100_SYSEX_GENRE_RESPONSE;
}

bool gp100_sysex_genre_build_notification(
    const gp100_genre_catalog_t *catalog,
    uint16_t index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    uint8_t payload[GP100_SYSEX_GENRE_NOTIFICATION_BYTES];

    if (catalog == NULL || index >= GP100_PRESET_GENRE_COUNT) {
        return false;
    }
    gp100_sysex_encode_u16(index, payload);
    copy_sanitized_name(payload + GP100_SYSEX_GENRE_INDEX_BYTES,
                        catalog->names[index],
                        GP100_SYSEX_GENRE_NOTIFICATION_NAME_BYTES);
    return gp100_sysex_build_response(
        GP100_SYSEX_GENRE_NAME_CHANGED,
        payload, sizeof(payload), response, response_capacity,
        response_length);
}
