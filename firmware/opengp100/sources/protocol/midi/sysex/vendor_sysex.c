#include "vendor_sysex.h"

#include "vendor_sysex_cabinet.h"
#include "vendor_sysex_control.h"
#include "vendor_sysex_data.h"
#include "vendor_sysex_effect.h"
#include "vendor_sysex_genre.h"
#include "vendor_sysex_global.h"
#include "vendor_sysex_performance.h"
#include "vendor_sysex_preset.h"
#include "vendor_sysex_settings.h"
#include "vendor_sysex_system.h"
#include "vendor_sysex_transfer.h"
#include "vendor_sysex_user_amp.h"

#include <stddef.h>

static bool has_gp100_signature(
    const uint8_t *message,
    size_t message_length)
{
    return message_length >= GP100_SYSEX_MIN_MESSAGE_BYTES &&
           message[0] == 0xf0u &&
           message[1] == 0x21u &&
           message[2] == 0x25u &&
           message[4] == (uint8_t)'G' &&
           message[5] == (uint8_t)'P' &&
           message[6] == (uint8_t)'-' &&
           message[7] == (uint8_t)'d' &&
           message[message_length - 1u] == 0xf7u;
}

bool gp100_sysex_parse(
    const uint8_t *message,
    size_t message_length,
    gp100_sysex_view_t *view)
{
    if (message == NULL || view == NULL ||
        !has_gp100_signature(message, message_length)) {
        return false;
    }
    view->device_id = message[3];
    view->message_class = message[8];
    view->address = ((uint32_t)message[9] << 24) |
                    ((uint32_t)message[10] << 16) |
                    ((uint32_t)message[11] << 8) |
                    (uint32_t)message[12];
    view->payload = message + GP100_SYSEX_HEADER_BYTES;
    view->payload_length = message_length -
                           GP100_SYSEX_MIN_MESSAGE_BYTES;
    return true;
}

static bool is_control_address(uint32_t address)
{
    return address < GP100_SYSEX_CONTROL_ASSIGNMENT_COUNT ||
           address == GP100_SYSEX_CONTROL_PEDAL_TARGET ||
           address == GP100_SYSEX_CONTROL_VOLUME_POSITION ||
           (address >= GP100_SYSEX_CONTROL_TARGET_BASE &&
            address <= GP100_SYSEX_CONTROL_VOLUME_MAXIMUM);
}

static bool is_performance_address(uint32_t address)
{
    return address == GP100_SYSEX_LOOP_RECORD_VOLUME ||
           address == GP100_SYSEX_LOOP_PLAYBACK_VOLUME ||
           address == GP100_SYSEX_RHYTHM_GENRE ||
           address == GP100_SYSEX_RHYTHM_STYLE ||
           address == GP100_SYSEX_RHYTHM_TEMPO ||
           address == GP100_SYSEX_RHYTHM_VOLUME;
}

static bool is_system_address(uint32_t address)
{
    return (address >= GP100_SYSEX_SYSTEM_PRESET &&
            address <= GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT) ||
           address == GP100_SYSEX_SYSTEM_FACTORY_RESET;
}

static bool is_effect_address(uint32_t address)
{
    uint32_t delta;
    uint32_t family;
    uint32_t suffix;

    if (address < GP100_SYSEX_EFFECT_BASE) {
        return false;
    }
    delta = address - GP100_SYSEX_EFFECT_BASE;
    family = delta / GP100_SYSEX_EFFECT_FAMILY_STRIDE;
    suffix = delta % GP100_SYSEX_EFFECT_FAMILY_STRIDE;
    return family < GP100_EFFECT_FAMILY_COUNT &&
           suffix <= GP100_SYSEX_EFFECT_PARAMETER_SUFFIX;
}

static bool is_preset_address(uint32_t address)
{
    return address == GP100_SYSEX_PRESET_CHAIN ||
           address == GP100_SYSEX_PRESET_TEMPO ||
           address == GP100_SYSEX_PRESET_LEVEL ||
           address == GP100_SYSEX_PRESET_AUTHOR ||
           address == GP100_SYSEX_PRESET_GENRE ||
           address == GP100_SYSEX_PRESET_NOTES;
}

static bool is_data_address(uint32_t address)
{
    return (address >= GP100_SYSEX_DATA_SESSION_BEGIN &&
            address <= GP100_SYSEX_DATA_SESSION_END) ||
           address == GP100_SYSEX_DATA_READ_SELECTED_PRESET ||
           address == GP100_SYSEX_DATA_SELECTED_PRESET_READY ||
           address == GP100_SYSEX_DATA_READ_CURRENT_PRESET ||
           address == GP100_SYSEX_DATA_CURRENT_PRESET_READY ||
           address == GP100_SYSEX_DATA_RUNTIME_STATUS;
}

static bool is_transfer_address(uint32_t address)
{
    return (address >= GP100_SYSEX_SELECTED_START &&
            address <= GP100_SYSEX_SELECTED_FINAL) ||
           (address >= GP100_SYSEX_CURRENT_START &&
            address <= GP100_SYSEX_CURRENT_FINAL);
}

gp100_sysex_domain_t gp100_sysex_domain_for_address(
    uint32_t address)
{
    size_t parameter_index;

    if (is_control_address(address)) {
        return GP100_SYSEX_DOMAIN_CONTROL;
    }
    if (is_performance_address(address)) {
        return GP100_SYSEX_DOMAIN_PERFORMANCE;
    }
    if (is_system_address(address)) {
        return GP100_SYSEX_DOMAIN_SYSTEM;
    }
    if (address == GP100_SYSEX_GLOBAL_SNAPSHOT ||
        gp100_sysex_global_parameter_index(
            address, &parameter_index)) {
        return GP100_SYSEX_DOMAIN_GLOBAL;
    }
    if (is_effect_address(address)) {
        return GP100_SYSEX_DOMAIN_EFFECT;
    }
    if (is_preset_address(address)) {
        return GP100_SYSEX_DOMAIN_PRESET;
    }
    if (address == GP100_SYSEX_GENRE_NAME ||
        address == GP100_SYSEX_GENRE_NAME_CHANGED) {
        return GP100_SYSEX_DOMAIN_GENRE;
    }
    if (is_data_address(address)) {
        return GP100_SYSEX_DOMAIN_DATA;
    }
    if (address >= GP100_SYSEX_CABINET_METADATA_UPDATE &&
        address <= GP100_SYSEX_CABINET_METADATA_READ) {
        return GP100_SYSEX_DOMAIN_CABINET;
    }
    if (address >= GP100_SYSEX_USER_AMP_DESCRIPTOR_READ &&
        address <= GP100_SYSEX_USER_AMP_NAME_UPDATE) {
        return GP100_SYSEX_DOMAIN_USER_AMP;
    }
    if (address == GP100_SYSEX_SETTINGS_SNAPSHOT) {
        return GP100_SYSEX_DOMAIN_SETTINGS;
    }
    if (is_transfer_address(address)) {
        return GP100_SYSEX_DOMAIN_TRANSFER;
    }
    return GP100_SYSEX_DOMAIN_UNKNOWN;
}

const char *gp100_sysex_domain_name(
    gp100_sysex_domain_t domain)
{
    static const char *const names[] = {
        "Unknown", "Control", "Performance", "System", "Global",
        "Effect", "Preset", "Genre", "Data", "Cabinet", "User AMP",
        "Settings", "Transfer"
    };

    if ((unsigned)domain >= sizeof(names) / sizeof(names[0])) {
        return "Unknown";
    }
    return names[(size_t)domain];
}

bool gp100_sysex_route(
    const uint8_t *message,
    size_t message_length,
    gp100_sysex_view_t *view,
    gp100_sysex_domain_t *domain)
{
    if (domain != NULL) {
        *domain = GP100_SYSEX_DOMAIN_UNKNOWN;
    }
    if (view == NULL || domain == NULL ||
        !gp100_sysex_parse(message, message_length, view)) {
        return false;
    }
    *domain = gp100_sysex_domain_for_address(view->address);
    return true;
}

bool gp100_sysex_build_response(
    uint32_t address,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *message,
    size_t message_capacity,
    size_t *message_length)
{
    size_t total_length;
    size_t offset;

    if (payload_length >
        SIZE_MAX - GP100_SYSEX_MIN_MESSAGE_BYTES) {
        if (message_length != NULL) {
            *message_length = 0u;
        }
        return false;
    }
    total_length = payload_length + GP100_SYSEX_MIN_MESSAGE_BYTES;
    if (message_length != NULL) {
        *message_length = total_length;
    }
    if (message == NULL || message_length == NULL ||
        (payload_length != 0u && payload == NULL) ||
        total_length > message_capacity ||
        total_length > GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES) {
        return false;
    }

    message[0] = 0xf0u;
    message[1] = 0x21u;
    message[2] = 0x25u;
    message[3] = GP100_SYSEX_FACTORY_DEVICE_ID;
    message[4] = (uint8_t)'G';
    message[5] = (uint8_t)'P';
    message[6] = (uint8_t)'-';
    message[7] = (uint8_t)'d';
    message[8] = GP100_SYSEX_RESPONSE_CLASS;
    message[9] = (uint8_t)(address >> 24);
    message[10] = (uint8_t)(address >> 16);
    message[11] = (uint8_t)(address >> 8);
    message[12] = (uint8_t)address;
    for (offset = 0u; offset < payload_length; ++offset) {
        message[GP100_SYSEX_HEADER_BYTES + offset] = payload[offset];
    }
    message[total_length - 1u] = 0xf7u;
    return true;
}

size_t gp100_sysex_nibble_encoded_size(size_t raw_length)
{
    if (raw_length > SIZE_MAX / 2u) {
        return 0u;
    }
    return raw_length * 2u;
}

bool gp100_sysex_nibble_encode(
    const uint8_t *raw,
    size_t raw_length,
    uint8_t *encoded,
    size_t encoded_capacity,
    size_t *encoded_length)
{
    size_t required = gp100_sysex_nibble_encoded_size(raw_length);
    size_t offset;

    if (encoded_length != NULL) {
        *encoded_length = required;
    }
    if ((raw_length != 0u && raw == NULL) || encoded == NULL ||
        encoded_length == NULL || (raw_length != 0u && required == 0u) ||
        encoded_capacity < required) {
        return false;
    }
    for (offset = 0u; offset < raw_length; ++offset) {
        encoded[offset * 2u] = raw[offset] >> 4;
        encoded[offset * 2u + 1u] = raw[offset] & 0x0fu;
    }
    return true;
}

bool gp100_sysex_nibble_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length)
{
    size_t required = encoded_length / 2u;
    size_t offset;

    if (raw_length != NULL) {
        *raw_length = required;
    }
    if (encoded == NULL || raw == NULL || raw_length == NULL ||
        (encoded_length & 1u) != 0u || raw_capacity < required) {
        return false;
    }
    for (offset = 0u; offset < required; ++offset) {
        uint8_t high = encoded[offset * 2u];
        uint8_t low = encoded[offset * 2u + 1u];

        if (high > 0x0fu || low > 0x0fu) {
            return false;
        }
        raw[offset] = (uint8_t)((high << 4) | low);
    }
    return true;
}

void gp100_sysex_encode_u16(uint16_t value, uint8_t encoded[4])
{
    uint8_t raw[2];
    size_t encoded_length;

    if (encoded == NULL) {
        return;
    }
    raw[0] = (uint8_t)value;
    raw[1] = (uint8_t)(value >> 8);
    (void)gp100_sysex_nibble_encode(
        raw, sizeof(raw), encoded, 4u, &encoded_length);
}

void gp100_sysex_encode_u32(uint32_t value, uint8_t encoded[8])
{
    uint8_t raw[4];
    size_t encoded_length;

    if (encoded == NULL) {
        return;
    }
    raw[0] = (uint8_t)value;
    raw[1] = (uint8_t)(value >> 8);
    raw[2] = (uint8_t)(value >> 16);
    raw[3] = (uint8_t)(value >> 24);
    (void)gp100_sysex_nibble_encode(
        raw, sizeof(raw), encoded, 8u, &encoded_length);
}

bool gp100_sysex_decode_u16(
    const uint8_t encoded[4],
    uint16_t *value)
{
    uint8_t raw[2];
    size_t raw_length;

    if (value == NULL ||
        !gp100_sysex_nibble_decode(
            encoded, 4u, raw, sizeof(raw), &raw_length)) {
        return false;
    }
    *value = (uint16_t)((uint16_t)raw[0] |
                        (uint16_t)((uint16_t)raw[1] << 8));
    return true;
}

bool gp100_sysex_decode_u32(
    const uint8_t encoded[8],
    uint32_t *value)
{
    uint8_t raw[4];
    size_t raw_length;

    if (value == NULL ||
        !gp100_sysex_nibble_decode(
            encoded, 8u, raw, sizeof(raw), &raw_length)) {
        return false;
    }
    *value = (uint32_t)raw[0] |
             ((uint32_t)raw[1] << 8) |
             ((uint32_t)raw[2] << 16) |
             ((uint32_t)raw[3] << 24);
    return true;
}
