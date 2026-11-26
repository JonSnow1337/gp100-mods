#ifndef GP100_EXPRESSION_CALIBRATION_RECORD_H
#define GP100_EXPRESSION_CALIBRATION_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EXPRESSION_CALIBRATION_FLASH_BASE \
    UINT32_C(0x007fe000)
#define GP100_EXPRESSION_CALIBRATION_FLASH_BYTES 0x1000u
#define GP100_EXPRESSION_CALIBRATION_PAYLOAD_SIZE 6u
#define GP100_EXPRESSION_CALIBRATION_RECORD_SIZE 10u
#define GP100_EXPRESSION_CALIBRATION_SLOT_COUNT 409u

typedef struct gp100_expression_calibration_values {
    uint16_t heel;
    uint16_t toe;
    uint16_t trigger;
} gp100_expression_calibration_values_t;

typedef struct gp100_expression_calibration_record {
    uint8_t bytes[GP100_EXPRESSION_CALIBRATION_RECORD_SIZE];
} gp100_expression_calibration_record_t;

typedef enum gp100_expression_calibration_validation {
    GP100_EXPRESSION_CALIBRATION_INVALID_ARGUMENT = -1,
    GP100_EXPRESSION_CALIBRATION_VALID = 0,
    GP100_EXPRESSION_CALIBRATION_ERASED = 1,
    GP100_EXPRESSION_CALIBRATION_BAD_CRC = 2
} gp100_expression_calibration_validation_t;

typedef struct gp100_expression_calibration_writer {
    size_t next_slot;
} gp100_expression_calibration_writer_t;

typedef struct gp100_expression_calibration_writer_hooks {
    void *context;
    bool (*erase_sector)(void *context, uint32_t address);
    bool (*write)(void *context, uint32_t address,
                  const uint8_t *data, size_t size);
} gp100_expression_calibration_writer_hooks_t;

typedef enum gp100_expression_calibration_writer_result {
    GP100_EXPRESSION_CALIBRATION_WRITER_INVALID = -1,
    GP100_EXPRESSION_CALIBRATION_WRITER_WRITTEN = 0,
    GP100_EXPRESSION_CALIBRATION_WRITER_IO_ERROR = 1
} gp100_expression_calibration_writer_result_t;

gp100_expression_calibration_validation_t
gp100_expression_calibration_record_validate(
    const gp100_expression_calibration_record_t *record);

bool gp100_expression_calibration_record_decode(
    const gp100_expression_calibration_record_t *record,
    gp100_expression_calibration_values_t *values);

/* Factory writes assert that all three values fit the recovered byte domain. */
bool gp100_expression_calibration_record_encode(
    const gp100_expression_calibration_values_t *values,
    gp100_expression_calibration_record_t *record);

/* Require at least four counts of travel and a switch threshold beyond toe. */
bool gp100_expression_calibration_values_are_usable(
    const gp100_expression_calibration_values_t *values);

/* For usable values, put the mode switch between the normal toe stop and
 * the hard-press peak. The journal retains both measured endpoints. */
uint16_t gp100_expression_calibration_switch_threshold(
    const gp100_expression_calibration_values_t *values);

bool gp100_expression_calibration_slot_address(size_t slot,
                                                     uint32_t *address);

/* Reproduce the factory loader's newest-to-oldest CRC scan. */
bool gp100_expression_calibration_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_expression_calibration_values_t *values,
    size_t *record_slot,
    size_t *next_slot);

bool gp100_expression_calibration_writer_init(
    gp100_expression_calibration_writer_t *writer,
    size_t next_slot);

gp100_expression_calibration_writer_result_t
gp100_expression_calibration_writer_append(
    gp100_expression_calibration_writer_t *writer,
    const gp100_expression_calibration_values_t *values,
    const gp100_expression_calibration_writer_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
