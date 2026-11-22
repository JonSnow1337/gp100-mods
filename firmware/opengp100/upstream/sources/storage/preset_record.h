#ifndef GP100_PRESET_RECORD_H
#define GP100_PRESET_RECORD_H

#include "flash_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PRESET_RECORD_SIZE       0x30eu
#define GP100_PRESET_CRC_OFFSET        0x30au
#define GP100_PRESET_NAME_SIZE         12u
#define GP100_PRESET_AUTHOR_OFFSET     0x2c2u
#define GP100_PRESET_AUTHOR_SIZE       20u
#define GP100_PRESET_NOTES_OFFSET      0x2d6u
#define GP100_PRESET_NOTES_SIZE        50u
#define GP100_EFFECT_FAMILY_COUNT      9u
#define GP100_PARAMETERS_PER_FAMILY    15u
#define GP100_PRESETS_PER_BANK         99u
#define GP100_TOTAL_LOGICAL_PRESETS    198u
#define GP100_CONTROL_TARGET_COUNT      3u
#define GP100_CONTROL_RANGE_COUNT       4u
#define GP100_STOMP_FOOTSWITCH_COUNT 2u
#define GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH 3u
/* Compatibility aliases for the earlier provisional "bank" terminology. */
#define GP100_CONTROL_ASSIGNMENT_BANK_COUNT \
    GP100_STOMP_FOOTSWITCH_COUNT
#define GP100_CONTROL_ASSIGNMENTS_PER_BANK \
    GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH
#define GP100_SYSTEM_GENRE_COUNT        11u
#define GP100_USER_GENRE_COUNT          50u
#define GP100_PRESET_GENRE_COUNT        \
    (GP100_SYSTEM_GENRE_COUNT + GP100_USER_GENRE_COUNT)

#define GP100_USER_PRESET_BASE         UINT32_C(0x00778000)

typedef enum gp100_effect_family {
    GP100_EFFECT_PRE_FX = 0,
    GP100_EFFECT_DISTORTION = 1,
    GP100_EFFECT_AMPLIFIER = 2,
    GP100_EFFECT_NOISE_REDUCTION = 3,
    GP100_EFFECT_CABINET = 4,
    GP100_EFFECT_EQUALIZER = 5,
    GP100_EFFECT_MODULATION = 6,
    GP100_EFFECT_DELAY = 7,
    GP100_EFFECT_REVERB = 8
} gp100_effect_family_t;

typedef enum gp100_preset_validation {
    GP100_PRESET_INVALID_ARGUMENT = -1,
    GP100_PRESET_VALID = 0,
    GP100_PRESET_ERASED = 1,
    GP100_PRESET_BAD_CRC = 2
} gp100_preset_validation_t;

typedef enum gp100_control_range {
    GP100_CONTROL_RANGE_VOLUME = 0,
    GP100_CONTROL_RANGE_EXPRESSION_TARGET_1 = 1,
    GP100_CONTROL_RANGE_EXPRESSION_TARGET_2 = 2,
    GP100_CONTROL_RANGE_EXPRESSION_TARGET_3 = 3
} gp100_control_range_t;

typedef enum gp100_stomp_footswitch {
    GP100_STOMP_FOOTSWITCH_FS1 = 0,
    GP100_STOMP_FOOTSWITCH_FS2 = 1
} gp100_stomp_footswitch_t;

typedef enum gp100_volume_pedal_position {
    GP100_VOLUME_PEDAL_PRE = 0,
    GP100_VOLUME_PEDAL_POST = 1
} gp100_volume_pedal_position_t;

typedef enum gp100_expression_pedal_target {
    GP100_EXPRESSION_PEDAL_VOLUME = 0,
    GP100_EXPRESSION_PEDAL_EFFECT = 1
} gp100_expression_pedal_target_t;

typedef struct gp100_preset_record {
    uint8_t bytes[GP100_PRESET_RECORD_SIZE];
} gp100_preset_record_t;

typedef struct gp100_control_target {
    bool assigned;
    gp100_effect_family_t family;
    uint32_t algorithm_id;
    uint16_t parameter_index;
} gp100_control_target_t;

uint32_t gp100_crc32(const void *data, size_t size);

bool gp100_preset_is_erased(const gp100_preset_record_t *record);

gp100_preset_validation_t gp100_preset_validate(
    const gp100_preset_record_t *record);

uint32_t gp100_preset_stored_crc(
    const gp100_preset_record_t *record);

void gp100_preset_update_crc(gp100_preset_record_t *record);

bool gp100_preset_flash_address(uint16_t logical_index,
                                     uint32_t *address);

uint16_t gp100_preset_number(
    const gp100_preset_record_t *record);

void gp100_preset_name(const gp100_preset_record_t *record,
                            char name[GP100_PRESET_NAME_SIZE + 1u]);

/* Save UI names use at most eleven visible bytes plus a terminator. */
bool gp100_preset_set_name(gp100_preset_record_t *record,
                                const char *name);

/* The record stores a processing position for each family, not vice versa. */
bool gp100_preset_effect_position(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t *position);

bool gp100_preset_effect_at_position(
    const gp100_preset_record_t *record,
    size_t position,
    gp100_effect_family_t *family);

bool gp100_preset_set_effect_positions(
    gp100_preset_record_t *record,
    const uint16_t positions[GP100_EFFECT_FAMILY_COUNT]);

bool gp100_preset_algorithm_id(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    uint32_t *algorithm_id);

/* Select an algorithm and install its recovered default parameter vector. */
bool gp100_preset_select_algorithm(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t algorithm_index);

bool gp100_preset_parameter(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t parameter_index,
    float *value);

/* Apply one logical parameter value, including tempo-sync dependent resets. */
bool gp100_preset_set_parameter(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    size_t parameter_index,
    float value,
    uint16_t *changed_slots);

bool gp100_preset_effect_enabled(
    const gp100_preset_record_t *record,
    gp100_effect_family_t family,
    bool *enabled);

bool gp100_preset_set_effect_enabled(
    gp100_preset_record_t *record,
    gp100_effect_family_t family,
    bool enabled);

bool gp100_preset_control_target(
    const gp100_preset_record_t *record,
    size_t target_index,
    gp100_control_target_t *target);

bool gp100_preset_set_control_target(
    gp100_preset_record_t *record,
    size_t target_index,
    const gp100_control_target_t *target);

bool gp100_preset_control_assignment(
    const gp100_preset_record_t *record,
    size_t footswitch_index,
    size_t assignment_index,
    uint16_t *family);

bool gp100_preset_set_control_assignment(
    gp100_preset_record_t *record,
    size_t footswitch_index,
    size_t assignment_index,
    uint16_t family);

uint16_t gp100_preset_tempo_bpm(
    const gp100_preset_record_t *record);

bool gp100_preset_set_tempo_bpm(
    gp100_preset_record_t *record,
    uint16_t tempo_bpm);

uint16_t gp100_preset_level(
    const gp100_preset_record_t *record);

bool gp100_preset_set_level(
    gp100_preset_record_t *record,
    uint16_t level);

bool gp100_preset_control_range(
    const gp100_preset_record_t *record,
    gp100_control_range_t range,
    float *minimum,
    float *maximum);

bool gp100_preset_set_control_range(
    gp100_preset_record_t *record,
    gp100_control_range_t range,
    float minimum,
    float maximum);

/* Toe-switch and MIDI CC 13 toggle this saved VOL/EXP target at 0x282. */
bool gp100_preset_expression_pedal_target(
    const gp100_preset_record_t *record,
    gp100_expression_pedal_target_t *target);

bool gp100_preset_set_expression_pedal_target(
    gp100_preset_record_t *record,
    gp100_expression_pedal_target_t target);

/* The saved halfword is 1 for PRE and 0 for POST. */
bool gp100_preset_volume_pedal_position(
    const gp100_preset_record_t *record,
    gp100_volume_pedal_position_t *position);

bool gp100_preset_set_volume_pedal_position(
    gp100_preset_record_t *record,
    gp100_volume_pedal_position_t position);

uint16_t gp100_preset_legacy_cabinet_id(
    const gp100_preset_record_t *record);

uint16_t gp100_preset_genre_index(
    const gp100_preset_record_t *record);

bool gp100_preset_set_genre_index(
    gp100_preset_record_t *record,
    uint16_t genre_index);

void gp100_preset_author(
    const gp100_preset_record_t *record,
    char author[GP100_PRESET_AUTHOR_SIZE + 1u]);

bool gp100_preset_update_author(
    gp100_preset_record_t *record,
    const uint8_t *text,
    size_t length);

void gp100_preset_notes(
    const gp100_preset_record_t *record,
    char notes[GP100_PRESET_NOTES_SIZE + 1u]);

bool gp100_preset_update_notes(
    gp100_preset_record_t *record,
    const uint8_t *text,
    size_t length);

#define gp100_preset_category_index \
    gp100_preset_genre_index
#define gp100_preset_auxiliary_cabinet_id \
    gp100_preset_legacy_cabinet_id
#define gp100_preset_auxiliary_cabinet_value \
    gp100_preset_genre_index

uint16_t gp100_preset_schema_version(
    const gp100_preset_record_t *record);

#ifdef __cplusplus
}
#endif

#endif
