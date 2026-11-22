#ifndef GP100_VENDOR_SYSEX_CONTROL_H
#define GP100_VENDOR_SYSEX_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/preset_record.h"
#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_CONTROL_ASSIGNMENT_BASE UINT32_C(0x00000000)
#define GP100_SYSEX_CONTROL_ASSIGNMENT_COUNT 6u
#define GP100_SYSEX_CONTROL_TARGET_BASE UINT32_C(0x00001000)
#define GP100_SYSEX_CONTROL_TARGET_STRIDE UINT32_C(3)
#define GP100_SYSEX_CONTROL_TARGET_BYTES 16u
#define GP100_SYSEX_CONTROL_VOLUME_MINIMUM UINT32_C(0x00001009)
#define GP100_SYSEX_CONTROL_VOLUME_MAXIMUM UINT32_C(0x0000100a)
#define GP100_SYSEX_CONTROL_PEDAL_TARGET UINT32_C(0x0000100d)
#define GP100_SYSEX_CONTROL_VOLUME_POSITION UINT32_C(0x0000100e)

typedef enum gp100_sysex_control_kind {
    GP100_SYSEX_CONTROL_ASSIGNMENT = 1,
    GP100_SYSEX_CONTROL_TARGET,
    GP100_SYSEX_CONTROL_RANGE_MINIMUM,
    GP100_SYSEX_CONTROL_RANGE_MAXIMUM,
    GP100_SYSEX_CONTROL_VOLUME_PEDAL_POSITION,
    GP100_SYSEX_CONTROL_EXPRESSION_PEDAL_TARGET
} gp100_sysex_control_kind_t;

typedef enum gp100_sysex_control_result {
    GP100_SYSEX_CONTROL_INVALID = -1,
    GP100_SYSEX_CONTROL_UNHANDLED = 0,
    GP100_SYSEX_CONTROL_ACTION = 1,
    GP100_SYSEX_CONTROL_RESPONSE = 2
} gp100_sysex_control_result_t;

typedef struct gp100_sysex_control_action {
    gp100_sysex_control_kind_t kind;
    /* Range actions use GP100_CONTROL_TARGET_COUNT for pedal volume. */
    size_t index;
    union {
        uint16_t assignment_family;
        gp100_control_target_t target;
        gp100_volume_pedal_position_t volume_position;
        gp100_expression_pedal_target_t expression_target;
        float range_value;
    } data;
} gp100_sysex_control_action_t;

gp100_sysex_control_result_t gp100_sysex_control_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_control_action_t *action);

bool gp100_sysex_control_apply(
    gp100_preset_record_t *record,
    const gp100_sysex_control_action_t *action);

gp100_sysex_control_result_t gp100_sysex_control_build_state(
    const gp100_sysex_view_t *request,
    const gp100_preset_record_t *record,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
