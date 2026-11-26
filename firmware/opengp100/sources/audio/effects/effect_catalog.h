#ifndef GP100_EFFECT_CATALOG_H
#define GP100_EFFECT_CATALOG_H

#include "storage/preset_normalize.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_TOTAL_EFFECT_ALGORITHMS 191u
#define GP100_TOTAL_EFFECT_PARAMETERS 662u
#define GP100_PRE_FX_LIMITTER 16u
#define GP100_PRE_FX_OCTAVER2 17u
#define GP100_PRE_FX_BALLS 18u
#define GP100_PRE_FX_ENVELOPE 19u
#define GP100_PRE_FX_STUDIO_COMP 20u
#define GP100_DISTORTION_MUFF 19u

const char *gp100_effect_family_name(
    gp100_effect_family_t family);

size_t gp100_effect_algorithm_count(
    gp100_effect_family_t family);

bool gp100_effect_algorithm_id(
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint32_t *algorithm_id);

const char *gp100_effect_algorithm_name(
    gp100_effect_family_t family,
    size_t algorithm_index);

bool gp100_effect_algorithm_is_user_slot(
    gp100_effect_family_t family,
    size_t algorithm_index);

/* Profile membership is independent of the persistent algorithm catalog. */
bool gp100_effect_available(gp100_effect_family_t family, size_t algorithm_index);

int gp100_effect_algorithm_index(
    gp100_effect_family_t family,
    uint32_t algorithm_id);

uint32_t gp100_effect_default_algorithm_id(
    gp100_effect_family_t family);

size_t gp100_effect_parameter_count(
    gp100_effect_family_t family,
    size_t algorithm_index);

size_t gp100_effect_parameter_count_for_id(
    gp100_effect_family_t family,
    uint32_t algorithm_id);

/* Internal table offset for a valid catalog index, including reserved holes. */
size_t gp100_effect_parameter_offset(gp100_effect_family_t family, size_t algorithm_index);

/* Returns the factory display label, an empty string for unnamed user slots,
 * or NULL when the family/algorithm/parameter indices are invalid. */
const char *gp100_effect_parameter_name(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index);

bool gp100_effect_parameter_storage_slot(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    uint8_t *storage_slot);

/* Inverse mapping used by control/assignment paths; -1 means no such slot. */
int gp100_effect_parameter_index_for_storage_slot(
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint8_t storage_slot);

/*
 * Returns the factory minimum, maximum, default and physical storage slot.
 * stored_parameters may be NULL when no preset state is available. Tempo-sync
 * selectors switch 26 rate/time parameters to a 0..10 beat-division scale.
 */
bool gp100_effect_parameter_rule(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    gp100_parameter_rule_t *rule);

/*
 * Validates and stores one logical parameter, then applies V2.1's dependent
 * tempo-sync resets. changed_slots is a bit mask over the fifteen physical
 * storage slots and may be NULL.
 */
bool gp100_effect_apply_parameter_change(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    uint16_t *changed_slots);

/* Physical parameter slots set to 1.0 when Tap Tempo enables sync. */
uint16_t gp100_effect_tempo_sync_selector_slots(
    gp100_effect_family_t family,
    size_t algorithm_index);

/* Beat lengths for integer delay selectors and exact floating-point LFO rates. */
float gp100_effect_tempo_beats(size_t selector);
float gp100_effect_tempo_beats_for_rate(float rate);

/* Convert a truncated delay Time control to its bounded 44.1 kHz span. */
float gp100_effect_delay_samples(uint32_t time,
                                     bool sync,
                                     uint16_t tempo_period_ms);

/*
 * Formats a stored parameter value as shown by V2.1. Returns the full string
 * length (excluding NUL), or -1 for invalid indices/arguments. Output is
 * always NUL-terminated when output_size is nonzero.
 */
int gp100_effect_format_parameter_value(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    char *output,
    size_t output_size);

/* Complete adapter for the preset normalizer. */
gp100_effect_catalog_t gp100_effect_catalog_core(void);

#ifdef __cplusplus
}
#endif

#endif
