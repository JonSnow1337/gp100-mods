#include "effect_chain.h"

#include "storage/patch_chain.h"

#include <string.h>

static size_t chain_count(const gp100_effect_chain_t *chain)
{
    return chain->native ? chain->count : GP100_EFFECT_FAMILY_COUNT;
}

static size_t slot_at(const gp100_effect_chain_t *chain, size_t position)
{
    return chain->native ? chain->slot_at_position[position]
                         : (size_t)chain->family_at_position[position];
}

static bool validate_chain(const gp100_effect_chain_t *chain)
{
    unsigned seen = 0u;

    if (chain == NULL || (chain->native && chain->count > GP100_PATCH_MAX_BLOCKS)) {
        return false;
    }
    const size_t slot_count = chain->native
        ? GP100_PATCH_MAX_BLOCKS : GP100_EFFECT_FAMILY_COUNT;
    for (size_t position = 0u; position < chain_count(chain); ++position) {
        const size_t slot = slot_at(chain, position);
        if ((unsigned)chain->family_at_position[position] >= GP100_EFFECT_FAMILY_COUNT ||
            slot >= slot_count || (seen & (1u << slot)) != 0u) {
            return false;
        }
        seen |= 1u << slot;
    }
    return true;
}

bool gp100_effect_chain_from_preset(
    gp100_effect_chain_t *chain,
    const gp100_preset_record_t *preset)
{
    gp100_effect_chain_t next = {0};
    bool seen[GP100_EFFECT_FAMILY_COUNT] = {false};
    size_t family_index;

    if (chain == NULL || preset == NULL) {
        return false;
    }
    if (gp100_patch_is_native(preset)) {
        if (gp100_patch_count(preset) > GP100_PATCH_MAX_BLOCKS) {
            return false;
        }
        next.native = true;
        next.count = (uint8_t)gp100_patch_count(preset);
        memset(next.position_by_family, UINT8_MAX, sizeof(next.position_by_family));
        for (size_t position = 0; position < next.count; ++position) {
            gp100_patch_block_t block;
            if (!gp100_patch_block_at(preset, position, &block)) {
                return false;
            }
            const gp100_effect_family_t family = (unsigned)block.family < GP100_EFFECT_FAMILY_COUNT
                ? block.family : GP100_EFFECT_PRE_FX;
            if (block.algorithm != GP100_PATCH_UNKNOWN_ALGORITHM &&
                next.position_by_family[family] == UINT8_MAX) {
                next.position_by_family[family] = (uint8_t)position;
            }
            next.family_at_position[position] = family;
            next.slot_at_position[position] = block.id;
        }
        *chain = next;
        return true;
    }
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        size_t position;

        if (!gp100_preset_effect_position(preset, family, &position) ||
            seen[position]) {
            return false;
        }
        seen[position] = true;
        next.position_by_family[family_index] = (uint8_t)position;
        next.family_at_position[position] = family;
    }
    *chain = next;
    return true;
}

bool gp100_effect_chain_family_at(
    const gp100_effect_chain_t *chain,
    size_t position,
    gp100_effect_family_t *family)
{
    if (chain == NULL || family == NULL ||
        position >= chain_count(chain)) {
        return false;
    }
    *family = chain->family_at_position[position];
    return (size_t)*family < GP100_EFFECT_FAMILY_COUNT;
}

bool gp100_effect_chain_position_of(
    const gp100_effect_chain_t *chain,
    gp100_effect_family_t family,
    size_t *position)
{
    if (chain == NULL || position == NULL ||
        (size_t)family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    *position = chain->position_by_family[(size_t)family];
    return *position < chain_count(chain);
}

bool gp100_effect_chain_process_block(
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    uint16_t routed_family_mask,
    float *interleaved_stereo,
    size_t frame_count,
    const gp100_effect_chain_ops_t *ops)
{
    size_t position;

    if (!validate_chain(chain) || runtime == NULL || ops == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        (routed_family_mask & ~GP100_EFFECT_CHAIN_ALL_FAMILIES) != 0u) {
        return false;
    }
    for (position = 0u;
         position < chain_count(chain);
         ++position) {
        gp100_effect_family_t family =
            chain->family_at_position[position];
        uint16_t family_bit;

        if (ops->process_effect != NULL) {
            family_bit = (uint16_t)(UINT16_C(1) << (size_t)family);
            if ((routed_family_mask & family_bit) != 0u) {
                ops->process_effect(ops->context, family,
                                    &runtime->slots[slot_at(chain, position)],
                                    interleaved_stereo, frame_count);
            }
        }
    }
    if (ops->post_process != NULL) {
        ops->post_process(ops->context, interleaved_stereo, frame_count);
    }
    return true;
}

bool gp100_effect_chain_process_routed_block(
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    gp100_cabinet_bypass_t cabinet_bypass,
    float *interleaved_stereo,
    size_t frame_count,
    float *channel_scratch,
    size_t scratch_count,
    const gp100_effect_chain_ops_t *ops)
{
    uint32_t bypass = (uint32_t)cabinet_bypass;
    size_t position;

    if (!validate_chain(chain) || runtime == NULL || ops == NULL ||
        bypass > (uint32_t)GP100_CABINET_BYPASS_BOTH ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        (ops->process_effect != NULL &&
         (bypass == GP100_CABINET_BYPASS_LEFT ||
          bypass == GP100_CABINET_BYPASS_RIGHT) &&
         (channel_scratch == NULL || scratch_count < frame_count))) {
        return false;
    }

    if (ops->process_effect != NULL) {
        for (position = 0u;
             position < chain_count(chain);
             ++position) {
            gp100_effect_family_t family =
                chain->family_at_position[position];
            if (family != GP100_EFFECT_CABINET || bypass == 0u) {
                ops->process_effect(ops->context, family,
                                    &runtime->slots[slot_at(chain, position)],
                                    interleaved_stereo, frame_count);
            }
        }

        if (bypass == GP100_CABINET_BYPASS_LEFT ||
            bypass == GP100_CABINET_BYPASS_RIGHT) {
            size_t channel =
                bypass == GP100_CABINET_BYPASS_LEFT ? 0u : 1u;
            size_t frame;
            for (frame = 0u; frame < frame_count; ++frame) {
                channel_scratch[frame] =
                    interleaved_stereo[frame * 2u + channel];
            }
            for (position = 0u; position < chain_count(chain); ++position) {
                if (chain->family_at_position[position] == GP100_EFFECT_CABINET) {
                    ops->process_effect(
                        ops->context, GP100_EFFECT_CABINET,
                        &runtime->slots[slot_at(chain, position)],
                        interleaved_stereo, frame_count);
                }
            }
            for (frame = 0u; frame < frame_count; ++frame) {
                interleaved_stereo[frame * 2u + channel] =
                    channel_scratch[frame];
            }
        }
    }
    if (ops->post_process != NULL) {
        ops->post_process(ops->context, interleaved_stereo, frame_count);
    }
    return true;
}
