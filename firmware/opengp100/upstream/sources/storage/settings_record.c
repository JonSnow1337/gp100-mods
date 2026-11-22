#include "settings_record.h"

#include "preset_record.h"
#include "rhythm/rhythm_engine.h"

#include <string.h>

#define GLOBAL_EQ_OFFSET 0x44u
#define SETTINGS_RESERVED_FLAG_16_OFFSET 0x16u
#define SETTINGS_RESERVED_FLAG_18_OFFSET 0x18u

static const uint8_t rhythm_genre_style_counts[] = {
    11u, 33u, 4u, 4u, 7u, 23u, 8u, 10u
};

static const uint32_t factory_global_eq_defaults[
    GP100_SYSEX_GLOBAL_PARAMETER_COUNT] = {
    UINT32_C(0x00000000), UINT32_C(0x41980000),
    UINT32_C(0x469c4200), UINT32_C(0x42c80000),
    UINT32_C(0x3f35c28f), UINT32_C(0x00000000),
    UINT32_C(0x43fa0000), UINT32_C(0x3f35c28f),
    UINT32_C(0x00000000), UINT32_C(0x447a0000),
    UINT32_C(0x3f35c28f), UINT32_C(0x00000000),
    UINT32_C(0x459c4000), UINT32_C(0x3f35c28f),
    UINT32_C(0x00000000)
};

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
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

static bool field_is_known(gp100_settings_field_t field)
{
    switch (field) {
    case GP100_SETTINGS_CURRENT_PRESET:
    case GP100_SETTINGS_FOOTSWITCH_MODE:
    case GP100_SETTINGS_DRUM_GENRE:
    case GP100_SETTINGS_DRUM_STYLE:
    case GP100_SETTINGS_DRUM_STYLE_IN_GENRE:
    case GP100_SETTINGS_DRUM_TEMPO:
    case GP100_SETTINGS_DRUM_VOLUME:
    case GP100_SETTINGS_TUNER_REFERENCE_HZ:
    case GP100_SETTINGS_TUNER_MODE:
    case GP100_SETTINGS_LANGUAGE_INDEX:
    case GP100_SETTINGS_LOOPER_POSITION:
    case GP100_SETTINGS_LOOP_RECORD_VOLUME:
    case GP100_SETTINGS_LOOP_PLAYBACK_VOLUME:
    case GP100_SETTINGS_MASTER_VOLUME:
    case GP100_SETTINGS_INPUT_LEVEL_DB:
    case GP100_SETTINGS_USB_MONITOR_LEVEL_DB:
    case GP100_SETTINGS_USB_RECORD_SOURCE_LEFT:
    case GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT:
    case GP100_SETTINGS_FIRMWARE_VERSION:
    case GP100_SETTINGS_USB_RECORD_LEVEL_DB:
    case GP100_SETTINGS_DATA_REVISION:
    case GP100_SETTINGS_TAP_SYNC_PRE:
    case GP100_SETTINGS_TAP_SYNC_WAH:
    case GP100_SETTINGS_TAP_SYNC_DISTORTION:
    case GP100_SETTINGS_TAP_SYNC_AMPLIFIER:
    case GP100_SETTINGS_TAP_SYNC_NOISE_GATE:
    case GP100_SETTINGS_TAP_SYNC_CABINET:
    case GP100_SETTINGS_TAP_SYNC_MODULATION:
    case GP100_SETTINGS_TAP_SYNC_DELAY:
    case GP100_SETTINGS_TAP_SYNC_REVERB:
    case GP100_SETTINGS_NO_CAB_LEFT:
    case GP100_SETTINGS_NO_CAB_RIGHT:
        return true;
    default:
        return false;
    }
}

uint32_t gp100_settings_stored_crc(
    const gp100_settings_record_t *record)
{
    if (record == NULL) {
        return 0u;
    }
    return read_u32_le(record->bytes + GP100_SETTINGS_CRC_OFFSET);
}

bool gp100_settings_validate(
    const gp100_settings_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    return gp100_settings_stored_crc(record) ==
           gp100_crc32(record->bytes, GP100_SETTINGS_PAYLOAD_SIZE);
}

void gp100_settings_update_crc(gp100_settings_record_t *record)
{
    uint32_t crc;

    if (record == NULL) {
        return;
    }
    crc = gp100_crc32(record->bytes, GP100_SETTINGS_PAYLOAD_SIZE);
    write_u32_le(record->bytes + GP100_SETTINGS_CRC_OFFSET, crc);
}

bool gp100_settings_get_i16(
    const gp100_settings_record_t *record,
    gp100_settings_field_t field,
    int16_t *value)
{
    if (record == NULL || value == NULL || !field_is_known(field)) {
        return false;
    }
    *value = (int16_t)read_u16_le(record->bytes + (size_t)field);
    return true;
}

bool gp100_settings_set_i16(
    gp100_settings_record_t *record,
    gp100_settings_field_t field,
    int16_t value)
{
    if (record == NULL || !field_is_known(field)) {
        return false;
    }
    write_u16_le(record->bytes + (size_t)field, (uint16_t)value);
    return true;
}

bool gp100_settings_copy_drums(gp100_settings_record_t *target,
                               const gp100_settings_record_t *source)
{
    const size_t offset = GP100_SETTINGS_DRUM_GENRE;
    const size_t size = GP100_SETTINGS_DRUM_VOLUME - offset + sizeof(int16_t);

    if (memcmp(target->bytes + offset, source->bytes + offset, size) == 0) {
        return false;
    }
    memcpy(target->bytes + offset, source->bytes + offset, size);
    return true;
}

bool gp100_settings_get_global_eq(
    const gp100_settings_record_t *record,
    gp100_sysex_global_state_t *global_eq)
{
    size_t index;

    if (record == NULL || global_eq == NULL) {
        return false;
    }
    for (index = 0u; index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        global_eq->parameters[index] =
            read_u32_le(record->bytes + GLOBAL_EQ_OFFSET + index * 4u);
    }
    return true;
}

bool gp100_settings_set_global_eq(
    gp100_settings_record_t *record,
    const gp100_sysex_global_state_t *global_eq)
{
    size_t index;

    if (record == NULL || global_eq == NULL) {
        return false;
    }
    for (index = 0u; index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        write_u32_le(record->bytes + GLOBAL_EQ_OFFSET + index * 4u,
                     global_eq->parameters[index]);
    }
    return true;
}

bool gp100_settings_set_factory_global_eq(
    gp100_settings_record_t *record)
{
    gp100_sysex_global_state_t global_eq;

    memcpy(global_eq.parameters, factory_global_eq_defaults,
           sizeof(factory_global_eq_defaults));
    return gp100_settings_set_global_eq(record, &global_eq);
}

bool gp100_settings_stamp_v210_snapshot(
    gp100_settings_record_t *record)
{
    return gp100_settings_set_i16(
               record, GP100_SETTINGS_FIRMWARE_VERSION,
               (int16_t)GP100_SETTINGS_V210_VERSION_WORD) &&
           gp100_settings_set_i16(
               record, GP100_SETTINGS_DATA_REVISION,
               (int16_t)GP100_SETTINGS_V210_DATA_REVISION);
}

static bool replace_i16(uint8_t *bytes, size_t offset, int16_t replacement)
{
    int16_t value = (int16_t)read_u16_le(bytes + offset);

    if (value == replacement) {
        return false;
    }
    write_u16_le(bytes + offset, (uint16_t)replacement);
    return true;
}

static bool repair_i16_above(uint8_t *bytes, size_t offset,
                             int16_t maximum, int16_t replacement)
{
    int16_t value = (int16_t)read_u16_le(bytes + offset);

    return value > maximum && replace_i16(bytes, offset, replacement);
}

static bool repair_u16_boolean(uint8_t *bytes, size_t offset)
{
    uint16_t value = read_u16_le(bytes + offset);

    return value >= 2u && replace_i16(bytes, offset, 0);
}

static bool repair_i16_range(uint8_t *bytes, size_t offset,
                             uint16_t minimum, uint16_t maximum,
                             int16_t replacement)
{
    uint16_t value = read_u16_le(bytes + offset);

    return (value < minimum || value > maximum) &&
           replace_i16(bytes, offset, replacement);
}

static bool repair_global_eq(gp100_settings_record_t *record)
{
    gp100_sysex_global_state_t equalizer;
    size_t parameter;
    bool changed = false;

    (void)gp100_settings_get_global_eq(record, &equalizer);
    for (parameter = 0u;
         parameter < GP100_SYSEX_GLOBAL_PARAMETER_COUNT; ++parameter) {
        gp100_global_eq_parameter_rule_t rule;
        uint32_t bits = equalizer.parameters[parameter];
        float value;
        bool invalid;

        memcpy(&value, &bits, sizeof(value));
        (void)gp100_global_eq_parameter_rule(
            (gp100_global_eq_parameter_t)parameter, &rule);
        if (parameter == GP100_GLOBAL_EQ_ENABLE) {
            /* This signed bit comparison is intentional: it matches the
             * factory's first comparison before its floating lower bound. */
            invalid = (int32_t)bits > (int32_t)UINT32_C(0x3f800000) ||
                      value < 0.0f;
        } else {
            invalid = value < rule.minimum || value > rule.maximum;
        }
        if (invalid && bits != factory_global_eq_defaults[parameter]) {
            equalizer.parameters[parameter] =
                factory_global_eq_defaults[parameter];
            changed = true;
        }
    }
    if (changed) {
        (void)gp100_settings_set_global_eq(record, &equalizer);
    }
    return changed;
}

uint32_t gp100_settings_normalize(
    gp100_settings_record_t *record,
    size_t language_count)
{
    uint32_t repairs = GP100_SETTINGS_REPAIR_NONE;
    int16_t genre;
    int16_t relative_style;
    size_t offset;
    bool changed;

    if (record == NULL || language_count == 0u ||
        language_count > (size_t)INT16_MAX) {
        return GP100_SETTINGS_REPAIR_NONE;
    }
    if (repair_i16_above(record->bytes, GP100_SETTINGS_CURRENT_PRESET,
                         197, 0)) {
        repairs |= GP100_SETTINGS_REPAIR_CURRENT_PRESET;
    }
    if (repair_i16_above(record->bytes, GP100_SETTINGS_FOOTSWITCH_MODE,
                         1, 0)) {
        repairs |= GP100_SETTINGS_REPAIR_FOOTSWITCH;
    }
    changed = repair_i16_above(
        record->bytes, GP100_SETTINGS_DRUM_GENRE, 7, 0);
    if (repair_i16_range(record->bytes, GP100_SETTINGS_DRUM_STYLE,
                         0u, GP100_RHYTHM_FACTORY_PATTERN_COUNT - 1u, 0)) {
        (void)replace_i16(record->bytes, GP100_SETTINGS_DRUM_GENRE, 0);
        (void)replace_i16(record->bytes, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0);
        changed = true;
    }
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_RHYTHM;
    }
    genre = (int16_t)read_u16_le(
        record->bytes + GP100_SETTINGS_DRUM_GENRE);
    relative_style = (int16_t)read_u16_le(
        record->bytes + GP100_SETTINGS_DRUM_STYLE_IN_GENRE);
    if (genre >= 0 &&
        (size_t)genre < sizeof(rhythm_genre_style_counts) /
                            sizeof(rhythm_genre_style_counts[0]) &&
        relative_style >= rhythm_genre_style_counts[(size_t)genre] &&
        replace_i16(record->bytes,
                    GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0)) {
        repairs |= GP100_SETTINGS_REPAIR_RHYTHM;
    }
    changed = repair_i16_range(
        record->bytes, GP100_SETTINGS_DRUM_TEMPO, 40u, 250u, 120);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_DRUM_VOLUME, 99, 99);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_RHYTHM;
    }
    changed = repair_i16_range(
        record->bytes, GP100_SETTINGS_TUNER_REFERENCE_HZ,
        432u, 447u, 440);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_TUNER_MODE, 2, 0);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_TUNER;
    }
    if (repair_i16_above(record->bytes, GP100_SETTINGS_LANGUAGE_INDEX,
                         (int16_t)(language_count - 1u), 0)) {
        repairs |= GP100_SETTINGS_REPAIR_LANGUAGE;
    }
    changed = repair_i16_above(
        record->bytes, GP100_SETTINGS_LOOPER_POSITION, 1, 0);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_LOOP_RECORD_VOLUME, 99, 99);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 99, 99);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_LOOPER;
    }
    changed = repair_i16_above(
        record->bytes, SETTINGS_RESERVED_FLAG_16_OFFSET, 1, 0);
    changed |= repair_i16_above(
        record->bytes, SETTINGS_RESERVED_FLAG_18_OFFSET, 1, 0);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_RESERVED_FLAGS;
    }
    changed = repair_i16_above(
        record->bytes, GP100_SETTINGS_MASTER_VOLUME, 99, 99);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_INPUT_LEVEL_DB, 20, 0);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_LEVELS;
    }
    changed = repair_i16_above(
        record->bytes, GP100_SETTINGS_USB_MONITOR_LEVEL_DB, 20, 0);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_USB_RECORD_LEVEL_DB, 20, 20);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 1, 1);
    changed |= repair_i16_above(
        record->bytes, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 1, 1);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_USB;
    }
    for (offset = GP100_SETTINGS_TAP_SYNC_PRE;
         offset <= GP100_SETTINGS_TAP_SYNC_REVERB; offset += 2u) {
        if (repair_u16_boolean(record->bytes, offset)) {
            repairs |= GP100_SETTINGS_REPAIR_TAP_SYNC;
        }
    }
    changed = repair_u16_boolean(
        record->bytes, GP100_SETTINGS_NO_CAB_LEFT);
    changed |= repair_u16_boolean(
        record->bytes, GP100_SETTINGS_NO_CAB_RIGHT);
    if (changed) {
        repairs |= GP100_SETTINGS_REPAIR_OUTPUT;
    }
    if (replace_i16(record->bytes, GP100_SETTINGS_DATA_REVISION,
                    (int16_t)GP100_SETTINGS_V210_DATA_REVISION)) {
        repairs |= GP100_SETTINGS_REPAIR_REVISION;
    }
    if (repair_global_eq(record)) {
        repairs |= GP100_SETTINGS_REPAIR_GLOBAL_EQ;
    }
    if (repairs != GP100_SETTINGS_REPAIR_NONE) {
        gp100_settings_update_crc(record);
    }
    return repairs;
}

bool gp100_settings_slot_address(size_t slot, uint32_t *address)
{
    if (slot >= GP100_SETTINGS_SLOT_COUNT || address == NULL) {
        return false;
    }
    *address = GP100_SETTINGS_FLASH_BASE +
               (uint32_t)(slot * GP100_SETTINGS_RECORD_SIZE);
    return true;
}

bool gp100_settings_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_settings_record_t *record,
    size_t *record_slot,
    size_t *next_slot)
{
    size_t slot;

    if (flash_region == NULL || record == NULL ||
        flash_region_size < GP100_SETTINGS_FLASH_BYTES) {
        return false;
    }
    for (slot = GP100_SETTINGS_SLOT_COUNT; slot != 0u; --slot) {
        gp100_settings_record_t candidate;
        size_t candidate_slot = slot - 1u;
        memcpy(candidate.bytes,
               flash_region + candidate_slot * GP100_SETTINGS_RECORD_SIZE,
               sizeof(candidate.bytes));
        if (gp100_settings_validate(&candidate)) {
            *record = candidate;
            if (record_slot != NULL) {
                *record_slot = candidate_slot;
            }
            if (next_slot != NULL) {
                *next_slot = candidate_slot + 1u;
            }
            return true;
        }
    }
    if (record_slot != NULL) {
        *record_slot = 0u;
    }
    if (next_slot != NULL) {
        *next_slot = 0u;
    }
    return false;
}

static void install_legacy_global_eq(gp100_settings_record_t *record)
{
    (void)gp100_settings_set_factory_global_eq(record);
}

static bool legacy_record_is_valid(const uint8_t *bytes,
                                   size_t payload_size)
{
    return read_u32_le(bytes + payload_size) ==
           gp100_crc32(bytes, payload_size);
}

bool gp100_settings_find_latest_compatible(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_settings_record_t *record,
    gp100_settings_format_t *source_format,
    size_t *source_slot,
    size_t *next_slot)
{
    size_t slot;

    if (flash_region == NULL || record == NULL ||
        flash_region_size < GP100_SETTINGS_FLASH_BYTES) {
        return false;
    }
    if (gp100_settings_find_latest(
            flash_region, flash_region_size, record, source_slot,
            next_slot)) {
        if (source_format != NULL) {
            *source_format = GP100_SETTINGS_FORMAT_CURRENT_84;
        }
        return true;
    }
    for (slot = GP100_SETTINGS_LEGACY_48_SLOT_COUNT;
         slot != 0u; --slot) {
        const uint8_t *candidate = flash_region +
            (slot - 1u) * GP100_SETTINGS_LEGACY_48_RECORD_SIZE;
        if (legacy_record_is_valid(candidate, 0x44u)) {
            memcpy(record->bytes, candidate, 0x44u);
            install_legacy_global_eq(record);
            gp100_settings_update_crc(record);
            if (source_format != NULL) {
                *source_format = GP100_SETTINGS_FORMAT_LEGACY_48;
            }
            if (source_slot != NULL) {
                *source_slot = slot - 1u;
            }
            if (next_slot != NULL) {
                *next_slot = 0u;
            }
            return true;
        }
    }
    for (slot = GP100_SETTINGS_LEGACY_44_SLOT_COUNT;
         slot != 0u; --slot) {
        const uint8_t *candidate = flash_region +
            (slot - 1u) * GP100_SETTINGS_LEGACY_44_RECORD_SIZE;
        if (legacy_record_is_valid(candidate, 0x40u)) {
            memcpy(record->bytes, candidate, 0x40u);
            memset(record->bytes + 0x40u, 0, 4u);
            install_legacy_global_eq(record);
            gp100_settings_update_crc(record);
            if (source_format != NULL) {
                *source_format = GP100_SETTINGS_FORMAT_LEGACY_44;
            }
            if (source_slot != NULL) {
                *source_slot = slot - 1u;
            }
            if (next_slot != NULL) {
                *next_slot = 0u;
            }
            return true;
        }
    }
    if (source_slot != NULL) {
        *source_slot = 0u;
    }
    if (next_slot != NULL) {
        *next_slot = 0u;
    }
    return false;
}
