#include "patch_chain.h"

#include "audio/effects/effect_catalog.h"
#include "audio/effects/effect_processor.h"

#include <math.h>
#include <string.h>

enum {
    CHAIN_HEADER = 0x00e,
    CHAIN_VERSION = 1,
    CHAIN_COUNT = CHAIN_HEADER + 1,
    CHAIN_ORDER = CHAIN_HEADER + 2,
    BLOCK_START = 0x020,
    BLOCK_SIZE = 68,
    BLOCK_ALGORITHM = 4,
    BLOCK_PARAMETERS = 8,
    CHAIN_END = BLOCK_START + GP100_PATCH_MAX_BLOCKS * BLOCK_SIZE,
    /* Existing native presets leave this space zeroed. */
    EXP_START = CHAIN_END,
    EXP_BYTES = 12,
    EXP_TAG = 0xa1,
    EXP_BLOCK_ID = 1,
    EXP_FOOTSWITCH = 3,
    EXP_HEEL = 4,
    EXP_TOE = 8,
    COMMON_FIELDS_START = 0x272,
    CHAIN_MAGIC = 0x308,
    CPU_BUDGET = 72,
};

_Static_assert(CHAIN_ORDER + GP100_PATCH_MAX_BLOCKS <= BLOCK_START,
               "Block order must fit in the chain header");
_Static_assert(EXP_START + EXP_BYTES <= 0x260,
               "Chain and expression assignment must fit before common preset fields");
_Static_assert(sizeof(float) == 4u, "Patch parameters use IEEE binary32");

static uint32_t read_word(const uint8_t *source)
{
    return (uint32_t)source[0] | (uint32_t)source[1] << 8 |
           (uint32_t)source[2] << 16 | (uint32_t)source[3] << 24;
}

static void write_word(uint8_t *destination, uint32_t word)
{
    for (size_t i = 0; i < 4; ++i) {
        destination[i] = (uint8_t)(word >> (8u * i));
    }
}

static size_t block_offset(size_t id)
{
    return BLOCK_START + id * BLOCK_SIZE;
}

bool gp100_patch_is_native(const gp100_preset_record_t *preset)
{
    return preset != NULL && preset->bytes[CHAIN_MAGIC] == 'O' &&
           preset->bytes[CHAIN_MAGIC + 1] == 'G';
}

void gp100_patch_init(gp100_preset_record_t *preset)
{
    memset(&preset->bytes[CHAIN_HEADER], 0, COMMON_FIELDS_START - CHAIN_HEADER);
    preset->bytes[CHAIN_HEADER] = CHAIN_VERSION;
    preset->bytes[CHAIN_MAGIC] = 'O';
    preset->bytes[CHAIN_MAGIC + 1] = 'G';
    gp100_preset_update_crc(preset);
}

size_t gp100_patch_count(const gp100_preset_record_t *preset)
{
    return gp100_patch_is_native(preset) ? preset->bytes[CHAIN_COUNT] : 0u;
}

static bool expression_targets(const gp100_preset_record_t *preset, uint8_t id)
{
    return preset->bytes[EXP_START] == EXP_TAG &&
           preset->bytes[EXP_START + EXP_BLOCK_ID] == id;
}

static bool read_block(const gp100_preset_record_t *preset, uint8_t id,
                       gp100_patch_block_t *block)
{
    if (id >= GP100_PATCH_MAX_BLOCKS) {
        return false;
    }
    const uint8_t *record = &preset->bytes[block_offset(id)];
    if (record[0] == 0u || (record[1] & ~7u) != 0u) {
        return false;
    }
    const gp100_effect_family_t family = (gp100_effect_family_t)(record[0] - 1u);
    const int algorithm = gp100_effect_algorithm_index(
        family, read_word(record + BLOCK_ALGORITHM));
    *block = (gp100_patch_block_t){
        .id = id, .family = family,
        .algorithm = algorithm < 0 ? GP100_PATCH_UNKNOWN_ALGORITHM : (uint16_t)algorithm,
        .enabled = (record[1] & 1u) != 0u,
        .footswitches = record[1] >> 1
    };
    if (block->footswitches == 0u && expression_targets(preset, id) &&
        preset->bytes[EXP_START + EXP_FOOTSWITCH] == 1u) {
        block->footswitches = GP100_PATCH_FOOTSWITCH_EXP;
    }
    if (block->algorithm == GP100_PATCH_UNKNOWN_ALGORITHM) {
        return true; /* Leave model ID and parameter words opaque in the record. */
    }
    for (size_t i = 0; i < GP100_PARAMETERS_PER_FAMILY; ++i) {
        const uint32_t bits = read_word(record + BLOCK_PARAMETERS + 4u * i);
        memcpy(&block->parameters[i], &bits, sizeof(bits));
        if (!isfinite(block->parameters[i])) {
            return false;
        }
    }
    return true;
}

bool gp100_patch_block_at(const gp100_preset_record_t *preset, size_t position,
                          gp100_patch_block_t *block)
{
    return block != NULL && position < gp100_patch_count(preset) &&
           position < GP100_PATCH_MAX_BLOCKS &&
           read_block(preset, preset->bytes[CHAIN_ORDER + position], block);
}

bool gp100_patch_expression_get(const gp100_preset_record_t *preset,
                                 gp100_patch_expression_t *assignment,
                                 gp100_patch_block_t *block)
{
    if (!gp100_patch_is_native(preset) || preset->bytes[EXP_START] != EXP_TAG) {
        return false;
    }
    const uint8_t *data = preset->bytes + EXP_START;
    gp100_patch_block_t target;
    gp100_patch_expression_t value = {.block_id = data[EXP_BLOCK_ID], .parameter = data[2]};
    if (!read_block(preset, value.block_id, &target)) {
        return false;
    }
    const size_t count = target.algorithm == GP100_PATCH_UNKNOWN_ALGORITHM
        ? GP100_PARAMETERS_PER_FAMILY : gp100_effect_parameter_count(target.family, target.algorithm);
    if (value.parameter >= count) {
        return false;
    }
    const uint32_t heel = read_word(data + EXP_HEEL);
    const uint32_t toe = read_word(data + EXP_TOE);
    memcpy(&value.heel, &heel, sizeof(heel));
    memcpy(&value.toe, &toe, sizeof(toe));
    if (!isfinite(value.heel) || !isfinite(value.toe)) {
        return false;
    }
    if (assignment != NULL) {
        *assignment = value;
    }
    if (block != NULL) {
        *block = target;
    }
    return true;
}

bool gp100_patch_expression_set(gp100_preset_record_t *preset,
                                const gp100_patch_expression_t *assignment)
{
    if (!gp100_patch_is_native(preset)) {
        return false;
    }
    uint8_t *data = preset->bytes + EXP_START;
    if (assignment == NULL) {
        memset(data, 0, EXP_BYTES);
        (void)gp100_preset_set_expression_pedal_target(preset, GP100_EXPRESSION_PEDAL_VOLUME);
    } else {
        gp100_patch_block_t block;
        gp100_parameter_rule_t rule;
        if (!read_block(preset, assignment->block_id, &block) ||
            !gp100_effect_parameter_rule(block.family, block.algorithm,
                 assignment->parameter, block.parameters, &rule) ||
            !(assignment->heel >= rule.minimum && assignment->heel <= rule.maximum) ||
            !(assignment->toe >= rule.minimum && assignment->toe <= rule.maximum)) {
            return false;
        }
        if (data[0] != EXP_TAG) {
            (void)gp100_preset_set_expression_pedal_target(preset, GP100_EXPRESSION_PEDAL_VOLUME);
        }
        uint32_t heel;
        uint32_t toe;
        memcpy(&heel, &assignment->heel, sizeof(heel));
        memcpy(&toe, &assignment->toe, sizeof(toe));
        /* This byte was reserved in existing presets. Keep the switch choice
         * when editing the same pedal, and drop it when moving the link. */
        data[EXP_FOOTSWITCH] = block.footswitches == GP100_PATCH_FOOTSWITCH_EXP;
        data[0] = EXP_TAG;
        data[EXP_BLOCK_ID] = assignment->block_id;
        data[2] = assignment->parameter;
        write_word(data + EXP_HEEL, heel);
        write_word(data + EXP_TOE, toe);
    }
    gp100_preset_update_crc(preset);
    return true;
}

bool gp100_patch_expression_set_mode(gp100_preset_record_t *preset,
                                      gp100_expression_pedal_target_t mode)
{
    if (!gp100_patch_is_native(preset)) {
        return gp100_preset_set_expression_pedal_target(preset, mode);
    }
    gp100_patch_block_t block;
    gp100_expression_pedal_target_t previous;
    const bool assigned = gp100_patch_expression_get(preset, NULL, &block);
    if (!assigned && mode == GP100_EXPRESSION_PEDAL_EFFECT) {
        mode = GP100_EXPRESSION_PEDAL_VOLUME;
    }
    if (!gp100_preset_expression_pedal_target(preset, &previous) ||
        !gp100_preset_set_expression_pedal_target(preset, mode)) {
        return false;
    }
    if (assigned && previous != mode && block.footswitches == GP100_PATCH_FOOTSWITCH_EXP) {
        preset->bytes[block_offset(block.id) + 1u] ^= 1u;
    }
    return true;
}

static void clear_expression_for_block(gp100_preset_record_t *preset, uint8_t id)
{
    if (expression_targets(preset, id)) {
        (void)gp100_patch_expression_set(preset, NULL);
    }
}

static uint8_t block_flags(const gp100_patch_block_t *block)
{
    return (uint8_t)(((block->footswitches & (GP100_PATCH_FOOTSWITCH_EXP - 1u)) << 1) |
                     block->enabled);
}

static void write_block(gp100_preset_record_t *preset,
                         const gp100_patch_block_t *block, uint32_t algorithm)
{
    uint8_t *record = &preset->bytes[block_offset(block->id)];
    record[0] = (uint8_t)block->family + 1u;
    record[1] = block_flags(block);
    write_word(record + BLOCK_ALGORITHM, algorithm);
    for (size_t i = 0; i < GP100_PARAMETERS_PER_FAMILY; ++i) {
        uint32_t bits;
        memcpy(&bits, &block->parameters[i], sizeof(bits));
        write_word(record + BLOCK_PARAMETERS + 4u * i, bits);
    }
}

size_t gp100_patch_state_bytes(gp100_effect_family_t family)
{
    const size_t size = gp100_effect_processor_state_size(family);
    return (size + GP100_PATCH_DSP_ALIGNMENT - 1u) &
           ~(size_t)(GP100_PATCH_DSP_ALIGNMENT - 1u);
}

/* Charges use units of 10,000 cycles per 64-frame block on the 600 MHz
 * board. The per-model values are rounded up from single-block benchmarks
 * at default settings. The budget leaves room for transport and variation.
 * Cabinet uses its measured cost in a stacked chain. User amp profiles
 * use a fixed conservative charge. Envelope and Studio Comp use estimates. */
static unsigned cpu_cost(const gp100_patch_block_t *block)
{
    static const uint8_t pre_fx[] = {
        3, 4, 8, 2, 4, 4, 3, 4, 4, 3, 3, 2,
        9, 8, 4, 2, [GP100_PRE_FX_LIMITTER] = 5, [GP100_PRE_FX_OCTAVER2] = 4,
        [GP100_PRE_FX_BALLS] = 7, [GP100_PRE_FX_ENVELOPE] = 3,
        [GP100_PRE_FX_STUDIO_COMP] = 6,
    };
    static const uint8_t distortion[] = {
        24, 12, 8, 11, 14, 12, 14, 11, 14, 15, 18, 5,
        20, 9, 19, 9, 15, 8, 2, [GP100_DISTORTION_MUFF] = 21,
    };
    static const uint8_t amplifier[] = {
        12, 18, 12, 14, 16, 16, 5, 9, 13, 15, 11, 23,
        33, 16, 20, 15, 18, 17, 24, 18, 9, 16, 13, 19,
        16, 16, 14, 16, 13, 17, 16, 13, 22, 18, 19, 12,
        22, 19, 18, 10, 6, 4, 11, 3, 18, 10,
    };
    static const uint8_t noise_reduction[] = {
        2, 2,
    };
    static const uint8_t cabinet[] = {
        17,
    };
    static const uint8_t equalizer[] = {
        3, 3, 3,
    };
    static const uint8_t modulation[] = {
        6, 6, 5, 3, 3, 4, 4, 5, 4, 3, 5, 3,
    };
    static const uint8_t delay[] = {
        9, 2, 5, 3, 3, 4, 3, 4, 6, 3, 5, 2,
    };
    static const uint8_t reverb[] = {
        7, 6, 7, 6, 6, 6, 6, 6, 6, 9,
    };
    static const struct {
        const uint8_t *costs;
        size_t count;
    } profiles[] = {
        { pre_fx, sizeof(pre_fx) },
        { distortion, sizeof(distortion) },
        { amplifier, sizeof(amplifier) },
        { noise_reduction, sizeof(noise_reduction) },
        { cabinet, sizeof(cabinet) },
        { equalizer, sizeof(equalizer) },
        { modulation, sizeof(modulation) },
        { delay, sizeof(delay) },
        { reverb, sizeof(reverb) },
    };

    if (block->family == GP100_EFFECT_CABINET) {
        return cabinet[0];
    }
    if (block->family == GP100_EFFECT_AMPLIFIER &&
        block->algorithm >= sizeof(amplifier)) {
        return 35u;
    }
    return block->algorithm < profiles[block->family].count
        ? profiles[block->family].costs[block->algorithm] : CPU_BUDGET;
}

static gp100_patch_result_t validate_chain(const gp100_preset_record_t *preset)
{
    if (!gp100_patch_is_native(preset) ||
        preset->bytes[CHAIN_HEADER] != CHAIN_VERSION ||
        gp100_patch_count(preset) > GP100_PATCH_MAX_BLOCKS) {
        return GP100_PATCH_INVALID;
    }
    size_t memory = 0;
    unsigned cpu = 0;
    unsigned seen = 0;
    for (size_t i = 0; i < gp100_patch_count(preset); ++i) {
        gp100_patch_block_t block;
        if (!gp100_patch_block_at(preset, i, &block) ||
            (seen & (1u << block.id)) != 0u) {
            return GP100_PATCH_INVALID;
        }
        seen |= 1u << block.id;
        if (block.algorithm != GP100_PATCH_UNKNOWN_ALGORITHM) {
            memory += gp100_patch_state_bytes(block.family);
            cpu += cpu_cost(&block);
        }
    }
    for (size_t id = 0; id < GP100_PATCH_MAX_BLOCKS; ++id) {
        if ((preset->bytes[block_offset(id)] != 0u) !=
            ((seen & (1u << id)) != 0u)) {
            return GP100_PATCH_INVALID;
        }
    }
    if (memory > GP100_PATCH_DSP_BYTES) {
        return GP100_PATCH_MEMORY_LIMIT;
    }
    return cpu > CPU_BUDGET ? GP100_PATCH_CPU_LIMIT : GP100_PATCH_OK;
}

bool gp100_patch_validate(const gp100_preset_record_t *preset)
{
    return validate_chain(preset) == GP100_PATCH_OK &&
           gp100_preset_validate(preset) == GP100_PRESET_VALID;
}

static bool initialize_algorithm(gp100_patch_block_t *block,
                                 gp100_effect_family_t family, size_t algorithm,
                                 uint32_t *algorithm_id)
{
    gp100_preset_record_t defaults = {0};
    if (!gp100_effect_available(family, algorithm) ||
        !gp100_effect_algorithm_id(family, algorithm, algorithm_id) ||
        !gp100_preset_select_algorithm(&defaults, family, algorithm)) {
        return false;
    }
    block->family = family;
    block->algorithm = (uint16_t)algorithm;
    for (size_t i = 0; i < GP100_PARAMETERS_PER_FAMILY; ++i) {
        if (!gp100_preset_parameter(&defaults, family, i, &block->parameters[i])) {
            return false;
        }
    }
    return true;
}

static gp100_patch_result_t commit_candidate(gp100_preset_record_t *preset,
                                             gp100_preset_record_t *candidate)
{
    const gp100_patch_result_t result = validate_chain(candidate);
    if (result == GP100_PATCH_OK) {
        gp100_preset_update_crc(candidate);
        *preset = *candidate;
    }
    return result;
}

gp100_patch_result_t gp100_patch_insert(gp100_preset_record_t *preset,
                                        size_t position,
                                        gp100_effect_family_t family,
                                        size_t algorithm)
{
    if (!gp100_patch_is_native(preset) || position > gp100_patch_count(preset)) {
        return GP100_PATCH_INVALID;
    }
    const size_t count = gp100_patch_count(preset);
    if (count >= GP100_PATCH_MAX_BLOCKS) {
        return GP100_PATCH_FULL;
    }

    gp100_patch_block_t block = {.family = family, .enabled = true};
    uint32_t algorithm_id;
    if (!initialize_algorithm(&block, family, algorithm, &algorithm_id)) {
        return GP100_PATCH_INVALID;
    }
    while (block.id < GP100_PATCH_MAX_BLOCKS &&
           preset->bytes[block_offset(block.id)] != 0u) {
        ++block.id;
    }
    if (block.id == GP100_PATCH_MAX_BLOCKS) {
        return GP100_PATCH_INVALID;
    }
    gp100_preset_record_t candidate = *preset;
    uint8_t *order = &candidate.bytes[CHAIN_ORDER + position];
    memmove(order + 1, order, count - position);
    *order = block.id;
    ++candidate.bytes[CHAIN_COUNT];
    write_block(&candidate, &block, algorithm_id);
    return commit_candidate(preset, &candidate);
}

gp100_patch_result_t gp100_patch_replace(gp100_preset_record_t *preset,
                                         size_t position,
                                         gp100_effect_family_t family,
                                         size_t algorithm)
{
    gp100_patch_block_t block;
    uint32_t algorithm_id;
    if (!gp100_patch_block_at(preset, position, &block) ||
        !initialize_algorithm(&block, family, algorithm, &algorithm_id)) {
        return GP100_PATCH_INVALID;
    }
    gp100_preset_record_t candidate = *preset;
    clear_expression_for_block(&candidate, block.id);
    write_block(&candidate, &block, algorithm_id);
    return commit_candidate(preset, &candidate);
}

bool gp100_patch_remove(gp100_preset_record_t *preset, size_t position)
{
    gp100_patch_block_t block;
    if (gp100_patch_count(preset) > GP100_PATCH_MAX_BLOCKS ||
        !gp100_patch_block_at(preset, position, &block)) {
        return false;
    }
    uint8_t *order = &preset->bytes[CHAIN_ORDER + position];
    memmove(order, order + 1, gp100_patch_count(preset) - position - 1u);
    --preset->bytes[CHAIN_COUNT];
    preset->bytes[CHAIN_ORDER + gp100_patch_count(preset)] = 0u;
    clear_expression_for_block(preset, block.id);
    memset(&preset->bytes[block_offset(block.id)], 0, BLOCK_SIZE);
    gp100_preset_update_crc(preset);
    return true;
}

bool gp100_patch_move(gp100_preset_record_t *preset, size_t from, size_t to)
{
    const size_t count = gp100_patch_count(preset);
    if (count > GP100_PATCH_MAX_BLOCKS || from >= count || to >= count) {
        return false;
    }
    uint8_t *order = &preset->bytes[CHAIN_ORDER];
    const uint8_t id = order[from];
    if (from < to) {
        memmove(order + from, order + from + 1u, to - from);
    } else if (from > to) {
        memmove(order + to + 1u, order + to, from - to);
    }
    order[to] = id;
    gp100_preset_update_crc(preset);
    return true;
}

bool gp100_patch_update(gp100_preset_record_t *preset,
                        const gp100_patch_block_t *block)
{
    if (!gp100_patch_is_native(preset) || block == NULL ||
        block->id >= GP100_PATCH_MAX_BLOCKS || block->footswitches > GP100_PATCH_FOOTSWITCH_EXP) {
        return false;
    }
    const bool linked = expression_targets(preset, block->id);
    if (block->footswitches == GP100_PATCH_FOOTSWITCH_EXP && !linked) {
        return false;
    }
    uint8_t *record = &preset->bytes[block_offset(block->id)];
    if (record[0] == 0u || record[0] != (uint8_t)block->family + 1u) {
        return false;
    }
    const uint32_t saved_algorithm = read_word(record + BLOCK_ALGORITHM);
    if (block->algorithm == GP100_PATCH_UNKNOWN_ALGORITHM) {
        if (gp100_effect_algorithm_index(block->family, saved_algorithm) >= 0) {
            return false;
        }
        record[1] = block_flags(block);
    } else {
        uint32_t algorithm;
        if (!gp100_effect_algorithm_id(block->family, block->algorithm, &algorithm) ||
            saved_algorithm != algorithm) {
            return false;
        }
        for (size_t i = 0; i < GP100_PARAMETERS_PER_FAMILY; ++i) {
            if (!isfinite(block->parameters[i])) {
                return false;
            }
        }
        write_block(preset, block, algorithm);
    }
    if (linked) {
        preset->bytes[EXP_START + EXP_FOOTSWITCH] = block->footswitches == GP100_PATCH_FOOTSWITCH_EXP;
    }
    gp100_preset_update_crc(preset);
    return true;
}

bool gp100_patch_toggle_footswitch(gp100_preset_record_t *preset, size_t footswitch)
{
    bool changed = false;
    if (!gp100_patch_is_native(preset) || footswitch >= 2u) {
        return false;
    }
    for (size_t i = 0; i < gp100_patch_count(preset); ++i) {
        gp100_patch_block_t block;
        if (gp100_patch_block_at(preset, i, &block) &&
            (block.footswitches & (1u << footswitch)) != 0u) {
            preset->bytes[block_offset(block.id) + 1u] ^= 1u;
            changed = true;
        }
    }
    if (changed) {
        gp100_preset_update_crc(preset);
    }
    return changed;
}
