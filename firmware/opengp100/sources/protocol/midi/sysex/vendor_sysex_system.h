#ifndef GP100_VENDOR_SYSEX_SYSTEM_H
#define GP100_VENDOR_SYSEX_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_SYSTEM_PRESET UINT32_C(0x00020000)
#define GP100_SYSEX_SYSTEM_FOOTSWITCH_MODE UINT32_C(0x00020001)
#define GP100_SYSEX_SYSTEM_LANGUAGE UINT32_C(0x00020002)
#define GP100_SYSEX_SYSTEM_MASTER_VOLUME UINT32_C(0x00020003)
#define GP100_SYSEX_SYSTEM_INPUT_LEVEL UINT32_C(0x00020004)
#define GP100_SYSEX_SYSTEM_USB_RECORD_LEVEL UINT32_C(0x00020005)
#define GP100_SYSEX_SYSTEM_USB_MONITOR_LEVEL UINT32_C(0x00020006)
#define GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_LEFT UINT32_C(0x00020007)
#define GP100_SYSEX_SYSTEM_USB_RECORD_SOURCE_RIGHT UINT32_C(0x00020008)
#define GP100_SYSEX_SYSTEM_FACTORY_RESET UINT32_C(0x12002000)

#define GP100_SYSEX_SYSTEM_MAX_LANGUAGES 32u

typedef enum gp100_footswitch_mode {
    GP100_FOOTSWITCH_MODE_PATCH = 0,
    GP100_FOOTSWITCH_MODE_STOMP = 1
} gp100_footswitch_mode_t;

typedef enum gp100_usb_record_source {
    GP100_USB_RECORD_SOURCE_DRY = 0,
    GP100_USB_RECORD_SOURCE_EFFECT = 1
} gp100_usb_record_source_t;

typedef struct gp100_language_catalog {
    uint8_t identifiers[GP100_SYSEX_SYSTEM_MAX_LANGUAGES];
    size_t count;
} gp100_language_catalog_t;

typedef struct gp100_sysex_system_state {
    uint16_t current_preset;
    int16_t footswitch_mode;
    uint8_t language_identifier;
    int16_t master_volume;
    int16_t input_level_db;
    int16_t usb_record_level_db;
    int16_t usb_monitor_level_db;
    int16_t usb_record_source_left;
    int16_t usb_record_source_right;
} gp100_sysex_system_state_t;

typedef enum gp100_sysex_system_action_kind {
    GP100_SYSEX_SYSTEM_RECALL_PRESET = 0,
    GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE,
    GP100_SYSEX_SYSTEM_SET_LANGUAGE,
    GP100_SYSEX_SYSTEM_SET_MASTER_VOLUME,
    GP100_SYSEX_SYSTEM_SET_INPUT_LEVEL,
    GP100_SYSEX_SYSTEM_SET_USB_RECORD_LEVEL,
    GP100_SYSEX_SYSTEM_SET_USB_MONITOR_LEVEL,
    GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_LEFT,
    GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_RIGHT,
    GP100_SYSEX_SYSTEM_REQUEST_FACTORY_RESET
} gp100_sysex_system_action_kind_t;

typedef struct gp100_sysex_system_action {
    gp100_sysex_system_action_kind_t kind;
    int16_t value;
    uint16_t logical_preset;
} gp100_sysex_system_action_t;

typedef enum gp100_sysex_system_result {
    GP100_SYSEX_SYSTEM_INVALID = -1,
    GP100_SYSEX_SYSTEM_UNHANDLED = 0,
    GP100_SYSEX_SYSTEM_UPDATE = 1,
    GP100_SYSEX_SYSTEM_RESPONSE = 2
} gp100_sysex_system_result_t;

void gp100_language_catalog_from_mask(
    uint32_t availability_mask,
    gp100_language_catalog_t *catalog);

gp100_sysex_system_result_t gp100_sysex_system_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_system_action_t *action);

bool gp100_sysex_system_apply(
    gp100_sysex_system_state_t *state,
    const gp100_language_catalog_t *languages,
    const gp100_sysex_system_action_t *action,
    bool *changed);

gp100_sysex_system_result_t gp100_sysex_system_build_state(
    const gp100_sysex_view_t *request,
    const gp100_sysex_system_state_t *state,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
