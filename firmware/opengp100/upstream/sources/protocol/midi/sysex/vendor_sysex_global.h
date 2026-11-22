#ifndef GP100_VENDOR_SYSEX_GLOBAL_H
#define GP100_VENDOR_SYSEX_GLOBAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_GLOBAL_SNAPSHOT UINT32_C(0x00020010)
#define GP100_SYSEX_GLOBAL_PARAMETER_BASE UINT32_C(0x00020011)
#define GP100_SYSEX_GLOBAL_PARAMETER_COUNT 15u
#define GP100_SYSEX_GLOBAL_SNAPSHOT_BYTES \
    (GP100_SYSEX_GLOBAL_PARAMETER_COUNT * sizeof(uint32_t))

typedef enum gp100_global_eq_parameter {
    GP100_GLOBAL_EQ_ENABLE = 0,
    GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY = 1,
    GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY = 2,
    GP100_GLOBAL_EQ_BAND_1_FREQUENCY = 3,
    GP100_GLOBAL_EQ_BAND_1_Q = 4,
    GP100_GLOBAL_EQ_BAND_1_GAIN = 5,
    GP100_GLOBAL_EQ_BAND_2_FREQUENCY = 6,
    GP100_GLOBAL_EQ_BAND_2_Q = 7,
    GP100_GLOBAL_EQ_BAND_2_GAIN = 8,
    GP100_GLOBAL_EQ_BAND_3_FREQUENCY = 9,
    GP100_GLOBAL_EQ_BAND_3_Q = 10,
    GP100_GLOBAL_EQ_BAND_3_GAIN = 11,
    GP100_GLOBAL_EQ_BAND_4_FREQUENCY = 12,
    GP100_GLOBAL_EQ_BAND_4_Q = 13,
    GP100_GLOBAL_EQ_BAND_4_GAIN = 14
} gp100_global_eq_parameter_t;

typedef struct gp100_sysex_global_state {
    uint32_t parameters[GP100_SYSEX_GLOBAL_PARAMETER_COUNT];
} gp100_sysex_global_state_t;

typedef struct gp100_sysex_global_update {
    gp100_global_eq_parameter_t parameter;
    uint32_t value_bits;
} gp100_sysex_global_update_t;

typedef struct gp100_global_eq_parameter_rule {
    float minimum;
    float maximum;
    float step;
} gp100_global_eq_parameter_rule_t;

typedef enum gp100_sysex_global_result {
    GP100_SYSEX_GLOBAL_INVALID = -1,
    GP100_SYSEX_GLOBAL_UNHANDLED = 0,
    GP100_SYSEX_GLOBAL_UPDATE = 1,
    GP100_SYSEX_GLOBAL_RESPONSE = 2
} gp100_sysex_global_result_t;

bool gp100_sysex_global_parameter_index(
    uint32_t address,
    size_t *parameter_index);

/* Recovered front-panel range and increment for one semantic EQ control. */
bool gp100_global_eq_parameter_rule(
    gp100_global_eq_parameter_t parameter,
    gp100_global_eq_parameter_rule_t *rule);

bool gp100_global_eq_parameter_value_valid(
    gp100_global_eq_parameter_t parameter,
    float value);

gp100_sysex_global_result_t gp100_sysex_global_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_global_update_t *update);

bool gp100_sysex_global_apply_update(
    gp100_sysex_global_state_t *state,
    const gp100_sysex_global_update_t *update,
    bool *changed);

gp100_sysex_global_result_t gp100_sysex_global_build_state(
    const gp100_sysex_view_t *request,
    const gp100_sysex_global_state_t *state,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
