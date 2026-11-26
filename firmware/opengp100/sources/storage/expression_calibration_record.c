#include "expression_calibration_record.h"

#include "preset_record.h"

#include <string.h>

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

gp100_expression_calibration_validation_t
gp100_expression_calibration_record_validate(
    const gp100_expression_calibration_record_t *record)
{
    uint32_t calculated;
    size_t index;

    if (record == NULL) {
        return GP100_EXPRESSION_CALIBRATION_INVALID_ARGUMENT;
    }
    for (index = 0u; index < sizeof(record->bytes); ++index) {
        if (record->bytes[index] != UINT8_MAX) {
            break;
        }
    }
    if (index == sizeof(record->bytes)) {
        return GP100_EXPRESSION_CALIBRATION_ERASED;
    }
    calculated = gp100_crc32(
        record->bytes, GP100_EXPRESSION_CALIBRATION_PAYLOAD_SIZE);
    return calculated == read_u32_le(
                             record->bytes +
                             GP100_EXPRESSION_CALIBRATION_PAYLOAD_SIZE)
               ? GP100_EXPRESSION_CALIBRATION_VALID
               : GP100_EXPRESSION_CALIBRATION_BAD_CRC;
}

bool gp100_expression_calibration_record_decode(
    const gp100_expression_calibration_record_t *record,
    gp100_expression_calibration_values_t *values)
{
    if (values == NULL ||
        gp100_expression_calibration_record_validate(record) !=
            GP100_EXPRESSION_CALIBRATION_VALID) {
        return false;
    }
    values->heel = read_u16_le(record->bytes);
    values->toe = read_u16_le(record->bytes + 2u);
    values->trigger = read_u16_le(record->bytes + 4u);
    return true;
}

bool gp100_expression_calibration_record_encode(
    const gp100_expression_calibration_values_t *values,
    gp100_expression_calibration_record_t *record)
{
    uint32_t crc;

    if (values == NULL || record == NULL ||
        values->heel > UINT8_MAX || values->toe > UINT8_MAX ||
        values->trigger > UINT8_MAX) {
        return false;
    }
    write_u16_le(record->bytes, values->heel);
    write_u16_le(record->bytes + 2u, values->toe);
    write_u16_le(record->bytes + 4u, values->trigger);
    crc = gp100_crc32(
        record->bytes, GP100_EXPRESSION_CALIBRATION_PAYLOAD_SIZE);
    write_u32_le(
        record->bytes + GP100_EXPRESSION_CALIBRATION_PAYLOAD_SIZE,
        crc);
    return true;
}

bool gp100_expression_calibration_values_are_usable(
    const gp100_expression_calibration_values_t *values)
{
    return values != NULL && values->heel <= UINT8_MAX &&
           values->toe <= UINT8_MAX && values->trigger <= UINT8_MAX &&
           (uint32_t)values->heel + 3u < values->toe &&
           values->toe < values->trigger;
}

uint16_t gp100_expression_calibration_switch_threshold(
    const gp100_expression_calibration_values_t *values)
{
    if (!gp100_expression_calibration_values_are_usable(values)) {
        return 0u;
    }
    return (uint16_t)(((uint32_t)values->toe + values->trigger + 1u) / 2u);
}

bool gp100_expression_calibration_slot_address(size_t slot,
                                                     uint32_t *address)
{
    if (slot >= GP100_EXPRESSION_CALIBRATION_SLOT_COUNT ||
        address == NULL) {
        return false;
    }
    *address = GP100_EXPRESSION_CALIBRATION_FLASH_BASE +
               (uint32_t)(slot *
                   GP100_EXPRESSION_CALIBRATION_RECORD_SIZE);
    return true;
}

bool gp100_expression_calibration_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_expression_calibration_values_t *values,
    size_t *record_slot,
    size_t *next_slot)
{
    size_t slot;

    if (flash_region == NULL || values == NULL ||
        flash_region_size < GP100_EXPRESSION_CALIBRATION_FLASH_BYTES) {
        return false;
    }
    for (slot = GP100_EXPRESSION_CALIBRATION_SLOT_COUNT;
         slot != 0u; --slot) {
        gp100_expression_calibration_record_t candidate;
        size_t candidate_slot = slot - 1u;

        memcpy(candidate.bytes,
               flash_region + candidate_slot *
                                  GP100_EXPRESSION_CALIBRATION_RECORD_SIZE,
               sizeof(candidate.bytes));
        if (gp100_expression_calibration_record_decode(
                &candidate, values)) {
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

bool gp100_expression_calibration_writer_init(
    gp100_expression_calibration_writer_t *writer,
    size_t next_slot)
{
    if (writer == NULL ||
        next_slot > GP100_EXPRESSION_CALIBRATION_SLOT_COUNT) {
        return false;
    }
    writer->next_slot = next_slot;
    return true;
}

gp100_expression_calibration_writer_result_t
gp100_expression_calibration_writer_append(
    gp100_expression_calibration_writer_t *writer,
    const gp100_expression_calibration_values_t *values,
    const gp100_expression_calibration_writer_hooks_t *hooks)
{
    gp100_expression_calibration_record_t record;
    size_t slot;
    uint32_t address;

    if (writer == NULL || hooks == NULL ||
        hooks->erase_sector == NULL || hooks->write == NULL ||
        writer->next_slot >
            GP100_EXPRESSION_CALIBRATION_SLOT_COUNT ||
        !gp100_expression_calibration_record_encode(values, &record)) {
        return GP100_EXPRESSION_CALIBRATION_WRITER_INVALID;
    }
    slot = writer->next_slot;
    if (slot == GP100_EXPRESSION_CALIBRATION_SLOT_COUNT) {
        if (!hooks->erase_sector(
                hooks->context,
                GP100_EXPRESSION_CALIBRATION_FLASH_BASE)) {
            return GP100_EXPRESSION_CALIBRATION_WRITER_IO_ERROR;
        }
        slot = 0u;
    }
    if (!gp100_expression_calibration_slot_address(slot, &address) ||
        !hooks->write(hooks->context, address,
                      record.bytes, sizeof(record.bytes))) {
        return GP100_EXPRESSION_CALIBRATION_WRITER_IO_ERROR;
    }
    writer->next_slot = slot + 1u;
    return GP100_EXPRESSION_CALIBRATION_WRITER_WRITTEN;
}
