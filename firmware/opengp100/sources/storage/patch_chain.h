#ifndef GP100_PATCH_CHAIN_H
#define GP100_PATCH_CHAIN_H

#include "preset_record.h"

#define GP100_PATCH_MAX_BLOCKS 8u
#define GP100_PATCH_UNKNOWN_ALGORITHM UINT16_MAX
#define GP100_PATCH_DSP_BYTES (3u * 1024u * 1024u)
#define GP100_PATCH_DSP_ALIGNMENT 8u
#define GP100_PATCH_FOOTSWITCH_EXP (1u << GP100_STOMP_FOOTSWITCH_COUNT)

typedef struct gp100_patch_block {
    uint8_t id;
    gp100_effect_family_t family;
    uint16_t algorithm;
    bool enabled;
    uint8_t footswitches;
    float parameters[GP100_PARAMETERS_PER_FAMILY];
} gp100_patch_block_t;

typedef struct gp100_patch_expression {
    uint8_t block_id;
    uint8_t parameter;
    float heel;
    float toe;
} gp100_patch_expression_t;

typedef enum gp100_patch_result {
    GP100_PATCH_OK,
    GP100_PATCH_FULL,
    GP100_PATCH_CPU_LIMIT,
    GP100_PATCH_MEMORY_LIMIT,
    GP100_PATCH_INVALID
} gp100_patch_result_t;

/* Name, tempo and levels use the common record fields. The chain has its own
 * magic, version, slot identities and parameter encoding. */
void gp100_patch_init(gp100_preset_record_t *preset);
bool gp100_patch_is_native(const gp100_preset_record_t *preset);
bool gp100_patch_validate(const gp100_preset_record_t *preset);
size_t gp100_patch_count(const gp100_preset_record_t *preset);
bool gp100_patch_block_at(const gp100_preset_record_t *preset, size_t position,
                          gp100_patch_block_t *block);
/* Read the optional assignment and, if requested, its pedal. */
bool gp100_patch_expression_get(const gp100_preset_record_t *preset,
                                 gp100_patch_expression_t *assignment,
                                 gp100_patch_block_t *block);
/* NULL clears the assignment and returns the expression pedal to volume mode. */
bool gp100_patch_expression_set(gp100_preset_record_t *preset,
                                const gp100_patch_expression_t *assignment);
/* Update the active mode and flip the linked pedal when its footswitch is EXP. */
bool gp100_patch_expression_set_mode(gp100_preset_record_t *preset,
                                      gp100_expression_pedal_target_t mode);
size_t gp100_patch_state_bytes(gp100_effect_family_t family);

gp100_patch_result_t gp100_patch_insert(gp100_preset_record_t *preset,
                                        size_t position,
                                        gp100_effect_family_t family,
                                        size_t algorithm);
gp100_patch_result_t gp100_patch_replace(gp100_preset_record_t *preset,
                                         size_t position,
                                         gp100_effect_family_t family,
                                         size_t algorithm);
bool gp100_patch_remove(gp100_preset_record_t *preset, size_t position);
bool gp100_patch_move(gp100_preset_record_t *preset, size_t from, size_t to);
bool gp100_patch_update(gp100_preset_record_t *preset,
                        const gp100_patch_block_t *block);
bool gp100_patch_toggle_footswitch(gp100_preset_record_t *preset,
                                   size_t footswitch);

#endif
