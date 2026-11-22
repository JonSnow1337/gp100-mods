#ifndef GP100_EFFECT_CHAIN_H
#define GP100_EFFECT_CHAIN_H

#include "effect_runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EFFECT_CHAIN_ALL_FAMILIES UINT16_C(0x01ff)

typedef enum gp100_cabinet_bypass {
    GP100_CABINET_BYPASS_NONE = 0u,
    GP100_CABINET_BYPASS_LEFT = UINT32_C(1) << 0,
    GP100_CABINET_BYPASS_RIGHT = UINT32_C(1) << 1,
    GP100_CABINET_BYPASS_BOTH =
        GP100_CABINET_BYPASS_LEFT |
        GP100_CABINET_BYPASS_RIGHT
} gp100_cabinet_bypass_t;

typedef struct gp100_effect_chain {
    gp100_effect_family_t family_at_position[
        GP100_EFFECT_FAMILY_COUNT];
    uint8_t position_by_family[GP100_EFFECT_FAMILY_COUNT];
    uint8_t slot_at_position[GP100_EFFECT_FAMILY_COUNT];
    uint8_t count;
    bool native;
} gp100_effect_chain_t;

typedef void (*gp100_effect_process_fn)(
    void *context,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count);

typedef void (*gp100_audio_post_process_fn)(
    void *context,
    float *interleaved_stereo,
    size_t frame_count);

typedef struct gp100_effect_chain_ops {
    void *context;
    gp100_effect_process_fn process_effect;
    gp100_audio_post_process_fn post_process;
} gp100_effect_chain_ops_t;

bool gp100_effect_chain_from_preset(
    gp100_effect_chain_t *chain,
    const gp100_preset_record_t *preset);

bool gp100_effect_chain_family_at(
    const gp100_effect_chain_t *chain,
    size_t position,
    gp100_effect_family_t *family);

bool gp100_effect_chain_position_of(
    const gp100_effect_chain_t *chain,
    gp100_effect_family_t family,
    size_t *position);

/*
 * Dispatches each routed family in preset order, then runs post_process once.
 * Effect callbacks receive disabled slots too and may implement a cheap bypass,
 * matching the factory's family-callback boundary.
 */
bool gp100_effect_chain_process_block(
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    uint16_t routed_family_mask,
    float *interleaved_stereo,
    size_t frame_count,
    const gp100_effect_chain_ops_t *ops);

/*
 * Models the factory's per-output cabinet switches. With one channel bypassed,
 * cabinet processing moves after the rest of the chain and that channel is
 * restored from mono scratch. scratch_count is measured in float samples.
 */
bool gp100_effect_chain_process_routed_block(
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    gp100_cabinet_bypass_t cabinet_bypass,
    float *interleaved_stereo,
    size_t frame_count,
    float *channel_scratch,
    size_t scratch_count,
    const gp100_effect_chain_ops_t *ops);

#ifdef __cplusplus
}
#endif

#endif
