#include "preset_record.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define PRESET_NUMBER_OFFSET       0x000u
#define PRESET_NAME_OFFSET         0x002u
#define PRESET_CHAIN_OFFSET        0x00eu
#define PRESET_ALGORITHM_OFFSET    0x020u
#define PRESET_PARAMETER_OFFSET    0x044u
#define PRESET_FAMILY_STRIDE       0x03cu
#define PRESET_ENABLE_OFFSET       0x260u
#define PRESET_TEMPO_OFFSET        0x272u
#define PRESET_LEVEL_OFFSET        0x274u
#define PRESET_ASSIGNMENT_OFFSET   0x276u
#define PRESET_TARGET_FAMILY_OFFSET 0x286u
#define PRESET_TARGET_ALGORITHM_OFFSET 0x28cu
#define PRESET_TARGET_PARAMETER_OFFSET 0x298u
#define PRESET_EXPRESSION_TARGET_OFFSET 0x282u
#define PRESET_VOLUME_POSITION_OFFSET  0x284u
#define PRESET_LEGACY_CABINET_ID_OFFSET 0x2beu
#define PRESET_GENRE_OFFSET             0x2c0u
#define PRESET_SCHEMA_OFFSET       0x308u

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      (uint16_t)((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_float_le(uint8_t *bytes, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(bytes, bits);
}

static bool valid_family(gp100_effect_family_t family)
{
    return (unsigned)family < GP100_EFFECT_FAMILY_COUNT;
}

static void copy_text_field(const gp100_preset_record_t *record,
                            size_t offset,
                            size_t field_size,
                            char *text)
{
    size_t index;

    if (text == NULL) {
        return;
    }
    if (record == NULL) {
        text[0] = '\0';
        return;
    }
    for (index = 0u; index < field_size; ++index) {
        uint8_t byte = record->bytes[offset + index];
        if (byte == 0u || byte == UINT8_MAX) {
            break;
        }
        text[index] = (char)byte;
    }
    text[index] = '\0';
}

static bool update_text_field(gp100_preset_record_t *record,
                              size_t offset,
                              size_t field_size,
                              const uint8_t *text,
                              size_t length)
{
    size_t index;

    if (record == NULL || length > field_size ||
        (length != 0u && text == NULL)) {
        return false;
    }
    for (index = 0u; index < field_size; ++index) {
        uint8_t byte = index < length ? text[index] : 0u;
        record->bytes[offset + index] = byte;
        if (byte == 0u) {
            break;
        }
    }
    return true;
}

uint32_t gp100_crc32(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_MAX;
    size_t index;
    unsigned bit;

    if (bytes == NULL && size != 0u) {
        return 0u;
    }
    for (index = 0u; index < size; ++index) {
        crc ^= bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

bool gp100_preset_is_erased(const gp100_preset_record_t *record)
{
    size_t index;

    if (record == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(record->bytes); ++index) {
        if (record->bytes[index] != UINT8_MAX) {
            return false;
        }
    }
    return true;
}

gp100_preset_validation_t gp100_preset_validate(
    const gp100_preset_record_t *record)
{
    uint32_t calculated;

    if (record == NULL) {
        return GP100_PRESET_INVALID_ARGUMENT;
    }
    if (gp100_preset_is_erased(record)) {
        return GP100_PRESET_ERASED;
    }
    calculated = gp100_crc32(record->bytes,
                                  GP100_PRESET_CRC_OFFSET);
    return calculated == gp100_preset_stored_crc(record)
               ? GP100_PRESET_VALID
               : GP100_PRESET_BAD_CRC;
}

uint32_t gp100_preset_stored_crc(
    const gp100_preset_record_t *record)
{
    if (record == NULL) {
        return 0u;
    }
    return read_u32_le(&record->bytes[GP100_PRESET_CRC_OFFSET]);
}

void gp100_preset_update_crc(gp100_preset_record_t *record)
{
    uint32_t crc;

    if (record == NULL) {
        return;
    }
    crc = gp100_crc32(record->bytes, GP100_PRESET_CRC_OFFSET);
    write_u32_le(&record->bytes[GP100_PRESET_CRC_OFFSET], crc);
}

bool gp100_preset_flash_address(uint16_t logical_index,
                                     uint32_t *address)
{
    if (address == NULL || logical_index >= GP100_PRESETS_PER_BANK) {
        return false;
    }
    *address = GP100_USER_PRESET_BASE + (uint32_t)logical_index * GP100_PRESET_RECORD_SIZE;
    return true;
}

uint16_t gp100_preset_number(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_NUMBER_OFFSET]);
}

void gp100_preset_name(const gp100_preset_record_t *record,
                            char name[GP100_PRESET_NAME_SIZE + 1u])
{
    copy_text_field(record, PRESET_NAME_OFFSET,
                    GP100_PRESET_NAME_SIZE, name);
}

bool gp100_preset_set_name(gp100_preset_record_t *record,
                                const char *name)
{
    size_t length;

    if (record == NULL || name == NULL) {
        return false;
    }
    length = strlen(name);
    if (length >= GP100_PRESET_NAME_SIZE) {
        length = GP100_PRESET_NAME_SIZE - 1u;
    }
    memset(&record->bytes[PRESET_NAME_OFFSET], 0,
           GP100_PRESET_NAME_SIZE);
    memcpy(&record->bytes[PRESET_NAME_OFFSET], name, length);
    return true;
}

bool gp100_preset_effect_position(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t *position)
{
    uint16_t value;

    if (record == NULL || position == NULL || !valid_family(family)) {
        return false;
    }
    value = read_u16_le(&record->bytes[PRESET_CHAIN_OFFSET +
                                      (size_t)family * 2u]);
    if (value >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    *position = value;
    return true;
}

bool gp100_preset_effect_at_position(
    const gp100_preset_record_t *record,
    size_t position,
    gp100_effect_family_t *family)
{
    size_t family_index;

    if (record == NULL || family == NULL ||
        position >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        size_t stored_position;
        if (!gp100_preset_effect_position(
                record, (gp100_effect_family_t)family_index,
                &stored_position)) {
            return false;
        }
        if (stored_position == position) {
            *family = (gp100_effect_family_t)family_index;
            return true;
        }
    }
    return false;
}

bool gp100_preset_set_effect_positions(
    gp100_preset_record_t *record,
    const uint16_t positions[GP100_EFFECT_FAMILY_COUNT])
{
    uint16_t seen = 0u;
    size_t family_index;

    if (record == NULL || positions == NULL) {
        return false;
    }
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        uint16_t position = positions[family_index];
        uint16_t mask;
        if (position >= GP100_EFFECT_FAMILY_COUNT) {
            return false;
        }
        mask = (uint16_t)(UINT16_C(1) << position);
        if ((seen & mask) != 0u) {
            return false;
        }
        seen = (uint16_t)(seen | mask);
    }
    for (family_index = 0u;
         family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        write_u16_le(&record->bytes[PRESET_CHAIN_OFFSET +
                                    family_index * 2u],
                     positions[family_index]);
    }
    return true;
}

bool gp100_preset_algorithm_id(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    uint32_t *algorithm_id)
{
    size_t offset;

    if (record == NULL || algorithm_id == NULL || !valid_family(family)) {
        return false;
    }
    offset = PRESET_ALGORITHM_OFFSET + (size_t)family * 4u;
    *algorithm_id = read_u32_le(&record->bytes[offset]);
    return true;
}

bool gp100_preset_select_algorithm(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    float parameters[GP100_PARAMETERS_PER_FAMILY] = {0.0f};
    uint32_t algorithm_id;
    size_t parameter_count;
    size_t parameter_index;
    size_t family_offset;

    if (record == NULL || !valid_family(family) ||
        !gp100_effect_algorithm_id(
            family, algorithm_index, &algorithm_id)) {
        return false;
    }
    parameter_count = gp100_effect_parameter_count(
        family, algorithm_index);
    for (parameter_index = 0u; parameter_index < parameter_count;
         ++parameter_index) {
        gp100_parameter_rule_t rule;

        if (!gp100_effect_parameter_rule(
                family, algorithm_index, parameter_index,
                parameters, &rule)) {
            return false;
        }
        parameters[rule.storage_slot] = rule.default_value;
    }
    family_offset = (size_t)family * PRESET_FAMILY_STRIDE;
    write_u32_le(
        &record->bytes[PRESET_ALGORITHM_OFFSET + (size_t)family * 4u],
        algorithm_id);
    for (parameter_index = 0u;
         parameter_index < GP100_PARAMETERS_PER_FAMILY;
         ++parameter_index) {
        write_float_le(
            &record->bytes[PRESET_PARAMETER_OFFSET + family_offset +
                           parameter_index * sizeof(uint32_t)],
            parameters[parameter_index]);
    }
    return true;
}

bool gp100_preset_parameter(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t parameter_index,
    float *value)
{
    uint32_t bits;
    size_t offset;

    if (record == NULL || value == NULL || !valid_family(family) ||
        parameter_index >= GP100_PARAMETERS_PER_FAMILY) {
        return false;
    }
    offset = PRESET_PARAMETER_OFFSET +
             (size_t)family * PRESET_FAMILY_STRIDE +
             parameter_index * sizeof(uint32_t);
    bits = read_u32_le(&record->bytes[offset]);
    memcpy(value, &bits, sizeof(*value));
    return true;
}

bool gp100_preset_set_parameter(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t parameter_index,
    float value,
    uint16_t *changed_slots)
{
    float parameters[GP100_PARAMETERS_PER_FAMILY];
    uint32_t algorithm_id;
    uint16_t changes;
    int algorithm_index;
    size_t slot;
    size_t family_offset;

    if (record == NULL || !valid_family(family) ||
        !gp100_preset_algorithm_id(record, family, &algorithm_id)) {
        return false;
    }
    algorithm_index = gp100_effect_algorithm_index(
        family, algorithm_id);
    if (algorithm_index < 0) {
        return false;
    }
    family_offset = (size_t)family * PRESET_FAMILY_STRIDE;
    for (slot = 0u; slot < GP100_PARAMETERS_PER_FAMILY; ++slot) {
        uint32_t bits = read_u32_le(
            &record->bytes[PRESET_PARAMETER_OFFSET + family_offset +
                           slot * sizeof(uint32_t)]);
        memcpy(&parameters[slot], &bits, sizeof(parameters[slot]));
    }
    if (!gp100_effect_apply_parameter_change(
            family, (size_t)algorithm_index, parameter_index, value,
            parameters, &changes)) {
        return false;
    }
    for (slot = 0u; slot < GP100_PARAMETERS_PER_FAMILY; ++slot) {
        if ((changes & (uint16_t)(UINT16_C(1) << slot)) != 0u) {
            write_float_le(
                &record->bytes[PRESET_PARAMETER_OFFSET + family_offset +
                               slot * sizeof(uint32_t)],
                parameters[slot]);
        }
    }
    if (changed_slots != NULL) {
        *changed_slots = changes;
    }
    return true;
}

bool gp100_preset_effect_enabled(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    bool *enabled)
{
    uint16_t value;
    size_t offset;

    if (record == NULL || enabled == NULL || !valid_family(family)) {
        return false;
    }
    offset = PRESET_ENABLE_OFFSET + (size_t)family * 2u;
    value = read_u16_le(&record->bytes[offset]);
    if (value > 1u) {
        return false;
    }
    *enabled = value != 0u;
    return true;
}

bool gp100_preset_set_effect_enabled(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    bool enabled)
{
    size_t offset;

    if (record == NULL || !valid_family(family)) {
        return false;
    }
    offset = PRESET_ENABLE_OFFSET + (size_t)family * 2u;
    write_u16_le(&record->bytes[offset], enabled ? 1u : 0u);
    return true;
}

bool gp100_preset_control_target(
    const gp100_preset_record_t *record,
    size_t target_index,
    gp100_control_target_t *target)
{
    int algorithm_index;
    int parameter_index;
    uint16_t family_value;
    uint16_t storage_slot;

    if (record == NULL || target == NULL ||
        target_index >= GP100_CONTROL_TARGET_COUNT) {
        return false;
    }
    family_value = read_u16_le(
        &record->bytes[PRESET_TARGET_FAMILY_OFFSET + target_index * 2u]);
    if (family_value == UINT16_MAX) {
        memset(target, 0, sizeof(*target));
        target->assigned = false;
        return true;
    }
    if (family_value >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    target->assigned = true;
    target->family = (gp100_effect_family_t)family_value;
    target->algorithm_id = read_u32_le(
        &record->bytes[PRESET_TARGET_ALGORITHM_OFFSET + target_index * 4u]);
    storage_slot = read_u16_le(
        &record->bytes[PRESET_TARGET_PARAMETER_OFFSET + target_index * 2u]);
    algorithm_index = gp100_effect_algorithm_index(
        target->family, target->algorithm_id);
    if (algorithm_index < 0 || storage_slot > UINT8_MAX) {
        return false;
    }
    parameter_index = gp100_effect_parameter_index_for_storage_slot(
        target->family, (size_t)algorithm_index, (uint8_t)storage_slot);
    if (parameter_index < 0 || parameter_index > UINT16_MAX) {
        return false;
    }
    target->parameter_index = (uint16_t)parameter_index;
    return true;
}

bool gp100_preset_set_control_target(
    gp100_preset_record_t *record,
    size_t target_index,
    const gp100_control_target_t *target)
{
    int algorithm_index;
    uint8_t storage_slot;

    if (record == NULL || target == NULL ||
        target_index >= GP100_CONTROL_TARGET_COUNT) {
        return false;
    }
    if (!target->assigned) {
        write_u16_le(&record->bytes[PRESET_TARGET_FAMILY_OFFSET +
                                    target_index * 2u], UINT16_MAX);
        write_u32_le(&record->bytes[PRESET_TARGET_ALGORITHM_OFFSET +
                                    target_index * 4u], 0u);
        write_u16_le(&record->bytes[PRESET_TARGET_PARAMETER_OFFSET +
                                    target_index * 2u], 0u);
        return true;
    }
    if (!valid_family(target->family)) {
        return false;
    }
    algorithm_index = gp100_effect_algorithm_index(
        target->family, target->algorithm_id);
    if (algorithm_index < 0 ||
        !gp100_effect_parameter_storage_slot(
            target->family, (size_t)algorithm_index,
            target->parameter_index, &storage_slot)) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_TARGET_FAMILY_OFFSET +
                                target_index * 2u],
                 (uint16_t)target->family);
    write_u32_le(&record->bytes[PRESET_TARGET_ALGORITHM_OFFSET +
                                target_index * 4u],
                 target->algorithm_id);
    write_u16_le(&record->bytes[PRESET_TARGET_PARAMETER_OFFSET +
                                target_index * 2u],
                 storage_slot);
    return true;
}

bool gp100_preset_control_assignment(
    const gp100_preset_record_t *record,
    size_t footswitch_index,
    size_t assignment_index,
    uint16_t *family)
{
    uint16_t value;
    size_t index;

    if (record == NULL || family == NULL ||
        footswitch_index >= GP100_STOMP_FOOTSWITCH_COUNT ||
        assignment_index >= GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH) {
        return false;
    }
    index = footswitch_index * GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH +
            assignment_index;
    value = read_u16_le(&record->bytes[PRESET_ASSIGNMENT_OFFSET + index * 2u]);
    *family = value < GP100_EFFECT_FAMILY_COUNT ? value : UINT16_MAX;
    return true;
}

bool gp100_preset_set_control_assignment(
    gp100_preset_record_t *record,
    size_t footswitch_index,
    size_t assignment_index,
    uint16_t family)
{
    size_t index;

    if (record == NULL ||
        footswitch_index >= GP100_STOMP_FOOTSWITCH_COUNT ||
        assignment_index >= GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH) {
        return false;
    }
    index = footswitch_index * GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH +
            assignment_index;
    write_u16_le(&record->bytes[PRESET_ASSIGNMENT_OFFSET + index * 2u],
                 family < GP100_EFFECT_FAMILY_COUNT
                     ? family
                     : UINT16_MAX);
    return true;
}

uint16_t gp100_preset_tempo_bpm(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_TEMPO_OFFSET]);
}

bool gp100_preset_set_tempo_bpm(
    gp100_preset_record_t *record,
    uint16_t tempo_bpm)
{
    if (record == NULL || tempo_bpm < 40u || tempo_bpm > 250u) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_TEMPO_OFFSET], tempo_bpm);
    return true;
}

uint16_t gp100_preset_level(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_LEVEL_OFFSET]);
}

bool gp100_preset_set_level(
    gp100_preset_record_t *record,
    uint16_t level)
{
    if (record == NULL || level > 100u) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_LEVEL_OFFSET], level);
    return true;
}

bool gp100_preset_control_range(
    const gp100_preset_record_t *record,
    gp100_control_range_t range,
    float *minimum,
    float *maximum)
{
    static const size_t minimum_offsets[GP100_CONTROL_RANGE_COUNT] = {
        0x29eu, 0x2a6u, 0x2aau, 0x2aeu
    };
    static const size_t maximum_offsets[GP100_CONTROL_RANGE_COUNT] = {
        0x2a2u, 0x2b2u, 0x2b6u, 0x2bau
    };
    uint32_t bits;

    if (record == NULL || minimum == NULL || maximum == NULL ||
        (unsigned)range >= GP100_CONTROL_RANGE_COUNT) {
        return false;
    }
    bits = read_u32_le(&record->bytes[minimum_offsets[(unsigned)range]]);
    memcpy(minimum, &bits, sizeof(*minimum));
    bits = read_u32_le(&record->bytes[maximum_offsets[(unsigned)range]]);
    memcpy(maximum, &bits, sizeof(*maximum));
    return true;
}

bool gp100_preset_set_control_range(
    gp100_preset_record_t *record,
    gp100_control_range_t range,
    float minimum,
    float maximum)
{
    static const size_t minimum_offsets[GP100_CONTROL_RANGE_COUNT] = {
        0x29eu, 0x2a6u, 0x2aau, 0x2aeu
    };
    static const size_t maximum_offsets[GP100_CONTROL_RANGE_COUNT] = {
        0x2a2u, 0x2b2u, 0x2b6u, 0x2bau
    };

    if (record == NULL ||
        (unsigned)range >= GP100_CONTROL_RANGE_COUNT ||
        !isfinite(minimum) || !isfinite(maximum) ||
        minimum < 0.0f || minimum > 100.0f ||
        maximum < 0.0f || maximum > 100.0f) {
        return false;
    }
    write_float_le(&record->bytes[minimum_offsets[(unsigned)range]], minimum);
    write_float_le(&record->bytes[maximum_offsets[(unsigned)range]], maximum);
    return true;
}

bool gp100_preset_expression_pedal_target(
    const gp100_preset_record_t *record,
    gp100_expression_pedal_target_t *target)
{
    uint16_t value;

    if (record == NULL || target == NULL) {
        return false;
    }
    value = read_u16_le(&record->bytes[PRESET_EXPRESSION_TARGET_OFFSET]);
    if (value > GP100_EXPRESSION_PEDAL_EFFECT) {
        return false;
    }
    *target = (gp100_expression_pedal_target_t)value;
    return true;
}

bool gp100_preset_set_expression_pedal_target(
    gp100_preset_record_t *record,
    gp100_expression_pedal_target_t target)
{
    if (record == NULL ||
        (unsigned)target > GP100_EXPRESSION_PEDAL_EFFECT) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_EXPRESSION_TARGET_OFFSET],
                 (uint16_t)target);
    return true;
}

bool gp100_preset_volume_pedal_position(
    const gp100_preset_record_t *record,
    gp100_volume_pedal_position_t *position)
{
    uint16_t value;

    if (record == NULL || position == NULL) {
        return false;
    }
    value = read_u16_le(&record->bytes[PRESET_VOLUME_POSITION_OFFSET]);
    if (value > 1u) {
        return false;
    }
    *position = value == 1u ? GP100_VOLUME_PEDAL_PRE
                            : GP100_VOLUME_PEDAL_POST;
    return true;
}

bool gp100_preset_set_volume_pedal_position(
    gp100_preset_record_t *record,
    gp100_volume_pedal_position_t position)
{
    if (record == NULL ||
        (unsigned)position > GP100_VOLUME_PEDAL_POST) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_VOLUME_POSITION_OFFSET],
                 position == GP100_VOLUME_PEDAL_PRE ? 1u : 0u);
    return true;
}

uint16_t gp100_preset_legacy_cabinet_id(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_LEGACY_CABINET_ID_OFFSET]);
}

uint16_t gp100_preset_genre_index(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_GENRE_OFFSET]);
}

bool gp100_preset_set_genre_index(
    gp100_preset_record_t *record,
    uint16_t genre_index)
{
    /* The shipped setter accepts the one-past-count value 61. */
    if (record == NULL || genre_index > GP100_PRESET_GENRE_COUNT) {
        return false;
    }
    write_u16_le(&record->bytes[PRESET_GENRE_OFFSET], genre_index);
    return true;
}

void gp100_preset_author(
    const gp100_preset_record_t *record,
    char author[GP100_PRESET_AUTHOR_SIZE + 1u])
{
    copy_text_field(record, GP100_PRESET_AUTHOR_OFFSET,
                    GP100_PRESET_AUTHOR_SIZE, author);
}

bool gp100_preset_update_author(
    gp100_preset_record_t *record,
    const uint8_t *text,
    size_t length)
{
    return update_text_field(record, GP100_PRESET_AUTHOR_OFFSET,
                             GP100_PRESET_AUTHOR_SIZE,
                             text, length);
}

void gp100_preset_notes(
    const gp100_preset_record_t *record,
    char notes[GP100_PRESET_NOTES_SIZE + 1u])
{
    copy_text_field(record, GP100_PRESET_NOTES_OFFSET,
                    GP100_PRESET_NOTES_SIZE, notes);
}

bool gp100_preset_update_notes(
    gp100_preset_record_t *record,
    const uint8_t *text,
    size_t length)
{
    return update_text_field(record, GP100_PRESET_NOTES_OFFSET,
                             GP100_PRESET_NOTES_SIZE,
                             text, length);
}

uint16_t gp100_preset_schema_version(
    const gp100_preset_record_t *record)
{
    return record == NULL ? 0u :
           read_u16_le(&record->bytes[PRESET_SCHEMA_OFFSET]);
}
