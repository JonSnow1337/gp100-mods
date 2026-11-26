#ifndef GP100_VENDOR_SYSEX_PRESET_H
#define GP100_VENDOR_SYSEX_PRESET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/preset_record.h"
#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_PRESET_CHAIN  UINT32_C(0x11000001)
#define GP100_SYSEX_PRESET_TEMPO  UINT32_C(0x11000002)
#define GP100_SYSEX_PRESET_LEVEL  UINT32_C(0x11000003)
#define GP100_SYSEX_PRESET_AUTHOR UINT32_C(0x11000004)
#define GP100_SYSEX_PRESET_GENRE  UINT32_C(0x11000005)
#define GP100_SYSEX_PRESET_NOTES  UINT32_C(0x11000007)

#define GP100_SYSEX_PRESET_CHAIN_BYTES \
    (GP100_EFFECT_FAMILY_COUNT * 4u)
#define GP100_SYSEX_PRESET_LOCATION_BYTES 2u
#define GP100_SYSEX_PRESET_AUTHOR_REPLY_TEXT_BYTES \
    (GP100_PRESET_AUTHOR_SIZE - GP100_SYSEX_PRESET_LOCATION_BYTES)
#define GP100_SYSEX_PRESET_NOTES_REPLY_TEXT_BYTES \
    (GP100_PRESET_NOTES_SIZE - GP100_SYSEX_PRESET_LOCATION_BYTES)

typedef enum gp100_sysex_preset_kind {
    GP100_SYSEX_PRESET_KIND_CHAIN = 1,
    GP100_SYSEX_PRESET_KIND_TEMPO,
    GP100_SYSEX_PRESET_KIND_LEVEL,
    GP100_SYSEX_PRESET_KIND_AUTHOR,
    GP100_SYSEX_PRESET_KIND_GENRE,
    GP100_SYSEX_PRESET_KIND_NOTES
} gp100_sysex_preset_kind_t;

typedef enum gp100_sysex_preset_result {
    GP100_SYSEX_PRESET_INVALID = -1,
    GP100_SYSEX_PRESET_UNHANDLED = 0,
    GP100_SYSEX_PRESET_ACTION = 1,
    GP100_SYSEX_PRESET_RESPONSE = 2
} gp100_sysex_preset_result_t;

typedef struct gp100_sysex_preset_text {
    uint8_t bytes[GP100_PRESET_NOTES_SIZE];
    size_t length;
} gp100_sysex_preset_text_t;

typedef struct gp100_sysex_preset_action {
    gp100_sysex_preset_kind_t kind;
    union {
        uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
        uint16_t value;
        gp100_sysex_preset_text_t text;
    } data;
} gp100_sysex_preset_action_t;

gp100_sysex_preset_result_t gp100_sysex_preset_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_preset_action_t *action);

bool gp100_sysex_preset_apply(
    gp100_preset_record_t *record,
    const gp100_sysex_preset_action_t *action);

gp100_sysex_preset_result_t gp100_sysex_preset_build_state(
    const gp100_sysex_view_t *request,
    const gp100_preset_record_t *record,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
