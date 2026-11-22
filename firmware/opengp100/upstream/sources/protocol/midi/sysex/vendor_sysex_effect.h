#ifndef GP100_VENDOR_SYSEX_EFFECT_H
#define GP100_VENDOR_SYSEX_EFFECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio/effects/effect_runtime.h"
#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_EFFECT_BASE UINT32_C(0x10010000)
#define GP100_SYSEX_EFFECT_FAMILY_STRIDE UINT32_C(0x00010000)
#define GP100_SYSEX_EFFECT_ENABLE_SUFFIX UINT32_C(0)
#define GP100_SYSEX_EFFECT_ALGORITHM_SUFFIX UINT32_C(1)
#define GP100_SYSEX_EFFECT_PARAMETER_SUFFIX UINT32_C(2)
#define GP100_SYSEX_EFFECT_PARAMETER_QUERY_BYTES 3u
#define GP100_SYSEX_EFFECT_PARAMETER_REPLY_BYTES 20u

typedef enum gp100_sysex_effect_kind {
    GP100_SYSEX_EFFECT_ENABLE = 0,
    GP100_SYSEX_EFFECT_ALGORITHM = 1,
    GP100_SYSEX_EFFECT_PARAMETER = 2
} gp100_sysex_effect_kind_t;

typedef struct gp100_sysex_effect_action {
    gp100_sysex_effect_kind_t kind;
    gp100_effect_family_t family;
    bool enabled;
    uint32_t algorithm_id;
    uint16_t algorithm_index;
    uint16_t parameter_index;
    float value;
} gp100_sysex_effect_action_t;

typedef enum gp100_sysex_effect_result {
    GP100_SYSEX_EFFECT_INVALID = -1,
    GP100_SYSEX_EFFECT_UNHANDLED = 0,
    GP100_SYSEX_EFFECT_ACTION = 1,
    GP100_SYSEX_EFFECT_RESPONSE = 2
} gp100_sysex_effect_result_t;

uint32_t gp100_sysex_effect_address(
    gp100_effect_family_t family,
    gp100_sysex_effect_kind_t kind);

gp100_sysex_effect_result_t gp100_sysex_effect_decode_command(
    const gp100_sysex_view_t *request,
    gp100_sysex_effect_action_t *action);

/* Applies one decoded action to the semantic effect runtime. */
bool gp100_sysex_effect_apply(
    gp100_effect_runtime_t *runtime,
    const gp100_sysex_effect_action_t *action,
    uint32_t *apply_action,
    uint16_t *changed_slots);

gp100_sysex_effect_result_t gp100_sysex_effect_build_state(
    const gp100_sysex_view_t *request,
    const gp100_effect_runtime_t *runtime,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
