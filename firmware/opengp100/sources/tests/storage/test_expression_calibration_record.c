#include "storage/expression_calibration_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 1;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 2;
    }
    *data = malloc((size_t)length);
    if (*data == NULL ||
        fread(*data, 1u, (size_t)length, file) != (size_t)length) {
        free(*data);
        fclose(file);
        return 3;
    }
    fclose(file);
    *size = (size_t)length;
    return 0;
}

static int test_record_codec(void)
{
    gp100_expression_calibration_values_t input = {51u, 117u, 129u};
    gp100_expression_calibration_values_t output = {0u, 0u, 0u};
    gp100_expression_calibration_record_t record;
    uint32_t address;

    if (!gp100_expression_calibration_record_encode(&input, &record) ||
        gp100_expression_calibration_record_validate(&record) !=
            GP100_EXPRESSION_CALIBRATION_VALID ||
        !gp100_expression_calibration_record_decode(&record, &output) ||
        memcmp(&input, &output, sizeof(input)) != 0 ||
        !gp100_expression_calibration_values_are_usable(&output)) {
        return 1;
    }
    if (record.bytes[6] != 0x04u || record.bytes[7] != 0x2du ||
        record.bytes[8] != 0x55u || record.bytes[9] != 0x4du) {
        return 2;
    }
    record.bytes[0] ^= 1u;
    if (gp100_expression_calibration_record_validate(&record) !=
            GP100_EXPRESSION_CALIBRATION_BAD_CRC ||
        gp100_expression_calibration_record_decode(&record, &output)) {
        return 3;
    }
    memset(&record, 0xff, sizeof(record));
    if (gp100_expression_calibration_record_validate(&record) !=
            GP100_EXPRESSION_CALIBRATION_ERASED) {
        return 4;
    }
    input.trigger = 256u;
    if (gp100_expression_calibration_record_encode(&input, &record) ||
        !gp100_expression_calibration_slot_address(408u, &address) ||
        address != UINT32_C(0x007feff0) ||
        gp100_expression_calibration_slot_address(409u, &address)) {
        return 5;
    }
    input.heel = 100u;
    input.toe = 103u;
    input.trigger = 120u;
    if (gp100_expression_calibration_values_are_usable(&input)) {
        return 6;
    }
    input.toe = 110u;
    input.trigger = 110u;
    if (gp100_expression_calibration_values_are_usable(&input)) {
        return 7;
    }
    input = (gp100_expression_calibration_values_t){39u, 87u, 99u};
    if (gp100_expression_calibration_switch_threshold(&input) != 93u) {
        return 8;
    }
    return 0;
}

static int test_backward_scan(void)
{
    uint8_t region[GP100_EXPRESSION_CALIBRATION_FLASH_BYTES];
    gp100_expression_calibration_values_t first = {10u, 200u, 220u};
    gp100_expression_calibration_values_t second = {20u, 210u, 230u};
    gp100_expression_calibration_values_t loaded;
    gp100_expression_calibration_record_t record;
    size_t slot;
    size_t next_slot;

    memset(region, 0xff, sizeof(region));
    if (!gp100_expression_calibration_record_encode(&first, &record)) {
        return 1;
    }
    memcpy(region, record.bytes, sizeof(record.bytes));
    if (!gp100_expression_calibration_record_encode(&second, &record)) {
        return 2;
    }
    memcpy(region + 3u * sizeof(record.bytes),
           record.bytes, sizeof(record.bytes));
    if (!gp100_expression_calibration_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 3u || next_slot != 4u ||
        memcmp(&loaded, &second, sizeof(loaded)) != 0) {
        return 3;
    }
    region[3u * sizeof(record.bytes)] ^= 1u;
    if (!gp100_expression_calibration_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 1u ||
        memcmp(&loaded, &first, sizeof(loaded)) != 0) {
        return 4;
    }
    memset(region, 0xff, sizeof(region));
    if (gp100_expression_calibration_find_latest(
            region, sizeof(region), &loaded, &slot, &next_slot) ||
        slot != 0u || next_slot != 0u) {
        return 5;
    }
    return 0;
}

static int test_supplied_flash(const char *path)
{
    uint8_t *flash = NULL;
    size_t flash_size = 0u;
    gp100_expression_calibration_values_t values;
    size_t slot;
    size_t next_slot;
    int result = load_file(path, &flash, &flash_size);

    if (result != 0) {
        return result;
    }
    if (flash_size < GP100_EXPRESSION_CALIBRATION_FLASH_BASE +
                         GP100_EXPRESSION_CALIBRATION_FLASH_BYTES ||
        !gp100_expression_calibration_find_latest(
            flash + GP100_EXPRESSION_CALIBRATION_FLASH_BASE,
            flash_size - GP100_EXPRESSION_CALIBRATION_FLASH_BASE,
            &values, &slot, &next_slot)) {
        free(flash);
        return 4;
    }
    free(flash);
    if (slot != 5u || next_slot != 6u || values.heel != 51u ||
        values.toe != 117u || values.trigger != 129u ||
        !gp100_expression_calibration_values_are_usable(&values)) {
        return 5;
    }
    return 0;
}

typedef struct writer_log {
    size_t erase_count;
    uint32_t erase_address;
    size_t write_count;
    uint32_t write_address;
    gp100_expression_calibration_record_t record;
} writer_log_t;

static bool writer_erase(void *context, uint32_t address)
{
    writer_log_t *log = context;
    ++log->erase_count;
    log->erase_address = address;
    return true;
}

static bool writer_write(void *context, uint32_t address,
                         const uint8_t *data, size_t size)
{
    writer_log_t *log = context;
    if (size != sizeof(log->record.bytes)) {
        return false;
    }
    ++log->write_count;
    log->write_address = address;
    memcpy(log->record.bytes, data, size);
    return true;
}

static int test_writer(void)
{
    gp100_expression_calibration_values_t values = {51u, 117u, 129u};
    gp100_expression_calibration_values_t decoded;
    gp100_expression_calibration_writer_t writer;
    writer_log_t log = {0u, 0u, 0u, 0u, {{0u}}};
    gp100_expression_calibration_writer_hooks_t hooks = {
        &log, writer_erase, writer_write
    };

    if (!gp100_expression_calibration_writer_init(&writer, 6u) ||
        gp100_expression_calibration_writer_append(
            &writer, &values, &hooks) !=
            GP100_EXPRESSION_CALIBRATION_WRITER_WRITTEN ||
        log.erase_count != 0u || log.write_count != 1u ||
        log.write_address != UINT32_C(0x007fe03c) ||
        writer.next_slot != 7u ||
        !gp100_expression_calibration_record_decode(
            &log.record, &decoded) || decoded.heel != values.heel ||
        decoded.toe != values.toe || decoded.trigger != values.trigger) {
        return 1;
    }
    memset(&log, 0, sizeof(log));
    if (!gp100_expression_calibration_writer_init(
            &writer, GP100_EXPRESSION_CALIBRATION_SLOT_COUNT) ||
        gp100_expression_calibration_writer_append(
            &writer, &values, &hooks) !=
            GP100_EXPRESSION_CALIBRATION_WRITER_WRITTEN ||
        log.erase_count != 1u ||
        log.erase_address !=
            GP100_EXPRESSION_CALIBRATION_FLASH_BASE ||
        log.write_address !=
            GP100_EXPRESSION_CALIBRATION_FLASH_BASE ||
        writer.next_slot != 1u) {
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result = test_record_codec();

    if (result != 0) {
        fprintf(stderr, "expression calibration codec failed: %d\n", result);
        return result;
    }
    result = test_backward_scan();
    if (result != 0) {
        fprintf(stderr, "expression calibration scan failed: %d\n", result);
        return 10 + result;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s <valeton_spi_flash.bin>\n", argv[0]);
        return 20;
    }
    result = test_supplied_flash(argv[1]);
    if (result != 0) {
        fprintf(stderr, "expression calibration flash failed: %d\n", result);
        return 20 + result;
    }
    result = test_writer();
    if (result != 0) {
        fprintf(stderr, "expression calibration writer failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
