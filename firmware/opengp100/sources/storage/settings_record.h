#ifndef GP100_SETTINGS_RECORD_H
#define GP100_SETTINGS_RECORD_H

#include "protocol/midi/sysex/vendor_sysex_global.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SETTINGS_FLASH_BASE UINT32_C(0x007f0000)
#define GP100_SETTINGS_FLASH_BYTES 0x9000u
#define GP100_SETTINGS_SECTOR_COUNT 9u
#define GP100_SETTINGS_RECORD_SIZE 0x84u
#define GP100_SETTINGS_EXTENDED_RECORD_SIZE 0x8cu
#define GP100_SETTINGS_PAYLOAD_SIZE 0x80u
#define GP100_SETTINGS_CRC_OFFSET 0x80u
#define GP100_SETTINGS_SLOT_COUNT 279u
#define GP100_SETTINGS_LEGACY_48_RECORD_SIZE 0x48u
#define GP100_SETTINGS_LEGACY_48_SLOT_COUNT 512u
#define GP100_SETTINGS_LEGACY_44_RECORD_SIZE 0x44u
#define GP100_SETTINGS_LEGACY_44_SLOT_COUNT 542u
#define GP100_SETTINGS_V210_VERSION_WORD UINT16_C(0x0201)
#define GP100_SETTINGS_V210_DATA_REVISION UINT16_C(0x0012)

typedef enum gp100_settings_repair {
    GP100_SETTINGS_REPAIR_NONE = 0u,
    GP100_SETTINGS_REPAIR_CURRENT_PRESET = UINT32_C(1) << 0,
    GP100_SETTINGS_REPAIR_FOOTSWITCH = UINT32_C(1) << 1,
    GP100_SETTINGS_REPAIR_RHYTHM = UINT32_C(1) << 2,
    GP100_SETTINGS_REPAIR_TUNER = UINT32_C(1) << 3,
    GP100_SETTINGS_REPAIR_LANGUAGE = UINT32_C(1) << 4,
    GP100_SETTINGS_REPAIR_LOOPER = UINT32_C(1) << 5,
    GP100_SETTINGS_REPAIR_LEVELS = UINT32_C(1) << 6,
    GP100_SETTINGS_REPAIR_USB = UINT32_C(1) << 7,
    GP100_SETTINGS_REPAIR_TAP_SYNC = UINT32_C(1) << 8,
    GP100_SETTINGS_REPAIR_OUTPUT = UINT32_C(1) << 9,
    GP100_SETTINGS_REPAIR_REVISION = UINT32_C(1) << 10,
    GP100_SETTINGS_REPAIR_GLOBAL_EQ = UINT32_C(1) << 11,
    GP100_SETTINGS_REPAIR_RESERVED_FLAGS = UINT32_C(1) << 12,
    GP100_SETTINGS_REPAIR_LIMITER = UINT32_C(1) << 13
} gp100_settings_repair_t;

typedef enum gp100_settings_format {
    GP100_SETTINGS_FORMAT_CURRENT_84 = 0,
    GP100_SETTINGS_FORMAT_LEGACY_48,
    GP100_SETTINGS_FORMAT_LEGACY_44
} gp100_settings_format_t;

typedef struct gp100_settings_record {
    uint8_t bytes[GP100_SETTINGS_EXTENDED_RECORD_SIZE];
} gp100_settings_record_t;

typedef enum gp100_settings_field {
    GP100_SETTINGS_CURRENT_PRESET = 0x00,
    GP100_SETTINGS_FOOTSWITCH_MODE = 0x02,
    GP100_SETTINGS_DRUM_GENRE = 0x04,
    GP100_SETTINGS_DRUM_STYLE = 0x06,
    GP100_SETTINGS_DRUM_STYLE_IN_GENRE = 0x08,
    GP100_SETTINGS_DRUM_TEMPO = 0x0a,
    GP100_SETTINGS_DRUM_VOLUME = 0x0c,
    GP100_SETTINGS_TUNER_REFERENCE_HZ = 0x0e,
    GP100_SETTINGS_TUNER_MODE = 0x10,
    GP100_SETTINGS_LANGUAGE_INDEX = 0x12,
    GP100_SETTINGS_LOOPER_POSITION = 0x14,
    GP100_SETTINGS_LOOP_RECORD_VOLUME = 0x1a,
    GP100_SETTINGS_LOOP_PLAYBACK_VOLUME = 0x1c,
    GP100_SETTINGS_MASTER_VOLUME = 0x1e,
    GP100_SETTINGS_INPUT_LEVEL_DB = 0x20,
    GP100_SETTINGS_USB_MONITOR_LEVEL_DB = 0x22,
    GP100_SETTINGS_USB_RECORD_SOURCE_LEFT = 0x24,
    GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT = 0x26,
    GP100_SETTINGS_FIRMWARE_VERSION = 0x28,
    GP100_SETTINGS_USB_RECORD_LEVEL_DB = 0x2a,
    GP100_SETTINGS_DATA_REVISION = 0x2c,
    GP100_SETTINGS_TAP_SYNC_PRE = 0x2e,
    GP100_SETTINGS_TAP_SYNC_WAH = 0x30,
    GP100_SETTINGS_TAP_SYNC_DISTORTION = 0x32,
    GP100_SETTINGS_TAP_SYNC_AMPLIFIER = 0x34,
    GP100_SETTINGS_TAP_SYNC_NOISE_GATE = 0x36,
    GP100_SETTINGS_TAP_SYNC_CABINET = 0x38,
    GP100_SETTINGS_TAP_SYNC_MODULATION = 0x3a,
    GP100_SETTINGS_TAP_SYNC_DELAY = 0x3c,
    GP100_SETTINGS_TAP_SYNC_REVERB = 0x3e,
    GP100_SETTINGS_NO_CAB_LEFT = 0x40,
    GP100_SETTINGS_NO_CAB_RIGHT = 0x42,
    GP100_SETTINGS_LIMITER_ENABLED = 0x84,
    GP100_SETTINGS_LIMITER_THRESHOLD_DB = 0x86,
    GP100_SETTINGS_LIMITER_CEILING_DB = 0x88,
    GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC = 0x8a
} gp100_settings_field_t;

uint32_t gp100_settings_stored_crc(
    const gp100_settings_record_t *record);
bool gp100_settings_validate(
    const gp100_settings_record_t *record);
void gp100_settings_update_crc(gp100_settings_record_t *record);
bool gp100_settings_equal(const gp100_settings_record_t *left,
                          const gp100_settings_record_t *right);

bool gp100_settings_get_i16(
    const gp100_settings_record_t *record,
    gp100_settings_field_t field,
    int16_t *value);
bool gp100_settings_set_i16(
    gp100_settings_record_t *record,
    gp100_settings_field_t field,
    int16_t value);

/* Copy the five drum settings and report whether any value changed. */
bool gp100_settings_copy_drums(gp100_settings_record_t *target,
                               const gp100_settings_record_t *source);

bool gp100_settings_get_global_eq(
    const gp100_settings_record_t *record,
    gp100_sysex_global_state_t *global_eq);
bool gp100_settings_set_global_eq(
    gp100_settings_record_t *record,
    const gp100_sysex_global_state_t *global_eq);

/* Install the 15 recovered factory Global EQ words. */
bool gp100_settings_set_factory_global_eq(
    gp100_settings_record_t *record);

/* Refresh the two metadata words stamped by the V2.1 settings-snapshot path.
 * This does not update the journal CRC. */
bool gp100_settings_stamp_v210_snapshot(
    gp100_settings_record_t *record);

/* Apply the field-repair policy recovered from factory routine 0x14f10.
 * The return value is a bitwise gp100_settings_repair_t summary. A
 * changed record receives a fresh current-format CRC. language_count is the
 * number of identifiers installed in the runtime language catalog. */
uint32_t gp100_settings_normalize(
    gp100_settings_record_t *record,
    size_t language_count);

bool gp100_settings_slot_address(size_t slot, uint32_t *address);

/* Scan the current-format journal newest to oldest, as loader 0x19c34 does. */
bool gp100_settings_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_settings_record_t *record,
    size_t *record_slot,
    size_t *next_slot);

/* Scan current format, then the two factory legacy formats and migrate. */
bool gp100_settings_find_latest_compatible(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_settings_record_t *record,
    gp100_settings_format_t *source_format,
    size_t *source_slot,
    size_t *next_slot);

#ifdef __cplusplus
}
#endif

#endif
