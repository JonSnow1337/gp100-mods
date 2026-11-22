#ifndef GP100_EXPRESSION_RUNTIME_H
#define GP100_EXPRESSION_RUNTIME_H

#include "audio/effects/effect_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_expression_apply_result {
    uint16_t effect_changed_slots[GP100_EFFECT_FAMILY_COUNT];
    bool volume_changed;
    float volume_percent;
    gp100_volume_pedal_position_t volume_position;
} gp100_expression_apply_result_t;

/* Apply a calibrated position in the inclusive 0..full_scale domain. */
bool gp100_expression_runtime_apply(
    const gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t position,
    uint16_t full_scale,
    gp100_expression_apply_result_t *result);

/* Factory ADC path: reserve three counts above heel, then map toe to 255. */
bool gp100_expression_runtime_normalize_adc(
    uint16_t raw_position,
    uint16_t heel,
    uint16_t toe,
    uint16_t *position);

bool gp100_expression_runtime_apply_adc(
    const gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t raw_position,
    uint16_t heel,
    uint16_t toe,
    gp100_expression_apply_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
