#ifndef GP100_EFFECT_RUNTIME_H
#define GP100_EFFECT_RUNTIME_H

#include "effect_catalog.h"
#include "storage/settings_record.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_effect_apply_action {
    GP100_EFFECT_APPLY_NONE = 0u,
    GP100_EFFECT_APPLY_DISABLE = UINT32_C(1) << 0,
    GP100_EFFECT_APPLY_ENABLE = UINT32_C(1) << 1,
    GP100_EFFECT_APPLY_LOAD_ALGORITHM = UINT32_C(1) << 2,
    GP100_EFFECT_APPLY_COPY_PARAMETERS = UINT32_C(1) << 3,
    GP100_EFFECT_APPLY_REFRESH_ASSET = UINT32_C(1) << 4,
    GP100_EFFECT_APPLY_INITIALIZE = UINT32_C(1) << 5
} gp100_effect_apply_action_t;

typedef struct gp100_effect_slot_state {
    bool enabled;
    bool algorithm_loaded;
    uint16_t algorithm_index;
    gp100_effect_family_t family;
    uint8_t instance_id;
    float parameters[GP100_PARAMETERS_PER_FAMILY];
} gp100_effect_slot_state_t;

typedef struct gp100_effect_runtime {
    gp100_effect_slot_state_t slots[GP100_EFFECT_FAMILY_COUNT];
} gp100_effect_runtime_t;

typedef enum gp100_control_update_result {
    GP100_CONTROL_UPDATE_INVALID = -1,
    GP100_CONTROL_UPDATE_UNASSIGNED = 0,
    GP100_CONTROL_UPDATE_APPLIED = 1,
    GP100_CONTROL_UPDATE_INACTIVE = 2
} gp100_control_update_result_t;

void gp100_effect_runtime_reset(
    gp100_effect_runtime_t *runtime);

/*
 * Applies one normalized preset to semantic runtime state. actions receives
 * the board/DSP work required for each family. Disabled slots preserve their
 * previously loaded algorithm and parameter block, matching V2.1 recall.
 */
bool gp100_effect_runtime_apply_preset(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT]);

/*
 * Synchronize a live edit without replaying recall-only initialization for
 * unchanged slots. Each action is NONE unless that family actually changed.
 */
bool gp100_effect_runtime_sync_preset(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT]);

/* Changes the live bypass state of an already loaded effect slot. */
bool gp100_effect_runtime_set_enabled(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    bool enabled,
    uint32_t *action);

/*
 * Selects a live algorithm. A changed algorithm starts with its recovered
 * catalog defaults and reports the board/DSP work needed to publish it.
 */
bool gp100_effect_runtime_select_algorithm(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint32_t *action);

/* Updates the active algorithm only; inactive/mismatched slots are rejected. */
bool gp100_effect_runtime_set_parameter(
    gp100_effect_runtime_t *runtime,
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    uint16_t *changed_slots);

/*
 * Apply the configured PRE, MOD and DLY sync edits when entering Tap Tempo.
 * Publish changes to matching loaded algorithms, including bypassed ones.
 * During recall, pass NULL for runtime to edit enabled families in the incoming
 * preset before loading their DSP state.
 */
bool gp100_effect_runtime_apply_tap_sync(
    gp100_preset_record_t *preset,
    const gp100_settings_record_t *settings,
    gp100_effect_runtime_t *runtime,
    uint16_t selectors[GP100_EFFECT_FAMILY_COUNT]);

/* Replacing a PRE, MOD or DLY algorithm exits the factory Tap Tempo mode. */
bool gp100_effect_runtime_tap_sync_algorithm_changed(
    const gp100_preset_record_t *before,
    const gp100_preset_record_t *after);

gp100_control_update_result_t
gp100_effect_runtime_set_control_target(
    gp100_effect_runtime_t *runtime,
    const gp100_preset_record_t *preset,
    size_t target_index,
    float value,
    uint16_t *changed_slots);

#ifdef __cplusplus
}
#endif

#endif
