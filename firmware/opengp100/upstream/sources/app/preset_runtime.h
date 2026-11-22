#ifndef GP100_PRESET_RUNTIME_H
#define GP100_PRESET_RUNTIME_H

#include "audio/engine/audio_output.h"
#include "audio/effects/effect_chain.h"
#include "storage/preset_normalize.h"
#include "ui/ui_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_preset_recall_result {
    uint32_t repairs;
    uint32_t effect_actions[GP100_EFFECT_FAMILY_COUNT];
    uint16_t tap_tempo_sync_slots[GP100_EFFECT_FAMILY_COUNT];
    uint32_t ui_updates;
} gp100_preset_recall_result_t;

typedef bool (*gp100_preset_loader_t)(
    void *context,
    uint16_t logical_preset,
    gp100_preset_record_t *preset);

/*
 * Publish one caller-loaded preset to all portable live consumers. The
 * operation is transactional: normalization or any downstream validation
 * failure leaves every supplied object unchanged. The published live
 * record has a CRC over its normalized fields. When Tap Tempo is active,
 * the incoming patch uses the configured sync defaults before the mode closes.
 */
bool gp100_preset_runtime_recall(
    uint16_t logical_preset,
    const gp100_effect_catalog_t *catalog,
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    gp100_ui_runtime_t *ui,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_preset_recall_result_t *result);

/* Load into a temporary record and publish only after recall succeeds. */
bool gp100_preset_runtime_load_and_recall(
    uint16_t logical_preset,
    gp100_preset_loader_t loader,
    void *loader_context,
    const gp100_effect_catalog_t *catalog,
    const gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    gp100_ui_runtime_t *ui,
    gp100_effect_chain_t *chain,
    gp100_effect_runtime_t *effects,
    gp100_audio_output_level_t *output,
    gp100_preset_recall_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
