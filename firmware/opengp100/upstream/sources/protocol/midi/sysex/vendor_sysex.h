#ifndef GP100_VENDOR_SYSEX_H
#define GP100_VENDOR_SYSEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_REQUEST_CLASS 0x11u
#define GP100_SYSEX_RESPONSE_CLASS 0x12u
#define GP100_SYSEX_FACTORY_DEVICE_ID 0x7fu
#define GP100_SYSEX_HEADER_BYTES 13u
#define GP100_SYSEX_MIN_MESSAGE_BYTES 14u
#define GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES 0x11cu

typedef struct gp100_sysex_view {
    uint8_t device_id;
    uint8_t message_class;
    uint32_t address;
    const uint8_t *payload;
    size_t payload_length;
} gp100_sysex_view_t;

/* Address ownership after the common GP-d envelope has been parsed. */
typedef enum gp100_sysex_domain {
    GP100_SYSEX_DOMAIN_UNKNOWN = 0,
    GP100_SYSEX_DOMAIN_CONTROL,
    GP100_SYSEX_DOMAIN_PERFORMANCE,
    GP100_SYSEX_DOMAIN_SYSTEM,
    GP100_SYSEX_DOMAIN_GLOBAL,
    GP100_SYSEX_DOMAIN_EFFECT,
    GP100_SYSEX_DOMAIN_PRESET,
    GP100_SYSEX_DOMAIN_GENRE,
    GP100_SYSEX_DOMAIN_DATA,
    GP100_SYSEX_DOMAIN_CABINET,
    GP100_SYSEX_DOMAIN_USER_AMP,
    GP100_SYSEX_DOMAIN_SETTINGS,
    GP100_SYSEX_DOMAIN_TRANSFER
} gp100_sysex_domain_t;

bool gp100_sysex_parse(
    const uint8_t *message,
    size_t message_length,
    gp100_sysex_view_t *view);

/* Classify every statically observed top-level reader/writer address. */
gp100_sysex_domain_t gp100_sysex_domain_for_address(
    uint32_t address);

const char *gp100_sysex_domain_name(
    gp100_sysex_domain_t domain);

/* Parse and classify a complete message in one step. Unknown is still valid. */
bool gp100_sysex_route(
    const uint8_t *message,
    size_t message_length,
    gp100_sysex_view_t *view,
    gp100_sysex_domain_t *domain);

bool gp100_sysex_build_response(
    uint32_t address,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *message,
    size_t message_capacity,
    size_t *message_length);

size_t gp100_sysex_nibble_encoded_size(size_t raw_length);

bool gp100_sysex_nibble_encode(
    const uint8_t *raw,
    size_t raw_length,
    uint8_t *encoded,
    size_t encoded_capacity,
    size_t *encoded_length);

bool gp100_sysex_nibble_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    uint8_t *raw,
    size_t raw_capacity,
    size_t *raw_length);

void gp100_sysex_encode_u16(uint16_t value, uint8_t encoded[4]);
void gp100_sysex_encode_u32(uint32_t value, uint8_t encoded[8]);
bool gp100_sysex_decode_u16(
    const uint8_t encoded[4],
    uint16_t *value);
bool gp100_sysex_decode_u32(
    const uint8_t encoded[8],
    uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif
