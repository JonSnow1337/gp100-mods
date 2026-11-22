#ifndef GP100_PRESET_NORMALIZE_H
#define GP100_PRESET_NORMALIZE_H

#include "preset_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PRESET_SCHEMA_CURRENT 0x12u

typedef struct gp100_parameter_rule {
    uint8_t storage_slot;
    float minimum;
    float maximum;
    float default_value;
} gp100_parameter_rule_t;

typedef struct gp100_effect_catalog {
    void *context;
    bool (*algorithm_is_valid)(void *context,
                               gp100_effect_family_t family,
                               uint32_t algorithm_id);
    uint32_t (*default_algorithm)(void *context,
                                  gp100_effect_family_t family);
    size_t (*parameter_count)(void *context,
                              gp100_effect_family_t family,
                              uint32_t algorithm_id);
    bool (*parameter_rule)(void *context,
                           gp100_effect_family_t family,
                           uint32_t algorithm_id,
                           size_t parameter_index,
                           const float stored_parameters[
                               GP100_PARAMETERS_PER_FAMILY],
                           gp100_parameter_rule_t *rule);
} gp100_effect_catalog_t;

typedef enum gp100_preset_repair {
    GP100_PRESET_REPAIRED_NONE = 0u,
    GP100_PRESET_REPAIRED_NAME = UINT32_C(1) << 0,
    GP100_PRESET_REPAIRED_CHAIN = UINT32_C(1) << 1,
    GP100_PRESET_REPAIRED_ALGORITHM = UINT32_C(1) << 2,
    GP100_PRESET_REPAIRED_PARAMETER = UINT32_C(1) << 3,
    GP100_PRESET_REPAIRED_ENABLE = UINT32_C(1) << 4,
    GP100_PRESET_REPAIRED_TEMPO = UINT32_C(1) << 5,
    GP100_PRESET_REPAIRED_LEVEL = UINT32_C(1) << 6,
    GP100_PRESET_REPAIRED_ASSIGNMENT = UINT32_C(1) << 7,
    GP100_PRESET_REPAIRED_BOOLEAN = UINT32_C(1) << 8,
    GP100_PRESET_REPAIRED_SELECTOR = UINT32_C(1) << 9,
    GP100_PRESET_REPAIRED_SELECTOR_CACHE = UINT32_C(1) << 10,
    GP100_PRESET_REPAIRED_PARAMETER_SELECTOR = UINT32_C(1) << 11,
    GP100_PRESET_REPAIRED_CONTROL_RANGE = UINT32_C(1) << 12,
    GP100_PRESET_REPAIRED_GENRE = UINT32_C(1) << 13,
    GP100_PRESET_REPAIRED_SCHEMA = UINT32_C(1) << 14,
    GP100_PRESET_REPAIRED_LEGACY_CABINET = UINT32_C(1) << 15
} gp100_preset_repair_t;

#define GP100_PRESET_REPAIRED_AUX_VALUE \
    GP100_PRESET_REPAIRED_GENRE

/*
 * Repairs a loaded record in memory. Like the factory recall path, this does
 * not update the stored CRC; call gp100_preset_update_crc() before saving.
 * Algorithm-specific work is skipped when catalog or its relevant callback is
 * NULL.
 */
uint32_t gp100_preset_normalize(
    gp100_preset_record_t *record,
    const gp100_effect_catalog_t *catalog);

#ifdef __cplusplus
}
#endif

#endif
