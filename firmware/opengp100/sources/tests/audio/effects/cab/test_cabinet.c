#include "audio/effects/cab/cabinet.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static void write_i32_le(uint8_t *bytes, int32_t value)
{
    uint32_t raw = (uint32_t)value;

    bytes[0] = (uint8_t)raw;
    bytes[1] = (uint8_t)(raw >> 8u);
    bytes[2] = (uint8_t)(raw >> 16u);
    bytes[3] = (uint8_t)(raw >> 24u);
}

static void make_record(uint8_t *record, const char *name,
                        size_t tap_index, int32_t tap_value)
{
    size_t name_length = strlen(name);
    size_t tap_offset = GP100_CABINET_NAME_SIZE +
                        GP100_CABINET_RESERVED_SIZE + tap_index * 4u;

    memset(record, 0, GP100_CABINET_RECORD_SIZE);
    if (name_length > GP100_CABINET_NAME_SIZE) {
        name_length = GP100_CABINET_NAME_SIZE;
    }
    memcpy(record, name, name_length);
    write_i32_le(&record[tap_offset], tap_value);
}

static int test_offsets_and_record_decode(void)
{
    static gp100_cabinet_t cabinet;
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    uint32_t offset = 0u;

    gp100_cabinet_reset(&cabinet);
    make_record(record, "Identity", 0u, 0x00800000);
    if (!gp100_cabinet_flash_offset(0u, &offset) ||
        offset != 0x00680000u ||
        !gp100_cabinet_flash_offset(39u, &offset) ||
        offset != 0x006a74e0u ||
        !gp100_cabinet_flash_offset(40u, &offset) ||
        offset != 0x006e0000u ||
        !gp100_cabinet_flash_offset(59u, &offset) ||
        offset != 0x006f3260u ||
        gp100_cabinet_flash_offset(60u, &offset) ||
        !gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        strcmp(cabinet.name, "Identity") != 0 ||
        cabinet.impulse[0] != 1.0f || cabinet.impulse[1] != 0.0f) {
        return 1;
    }
    return 0;
}

static int test_identity_and_level_slew(void)
{
    static gp100_cabinet_t cabinet;
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    float audio[128] = {0.0f};
    float expected_level = 0.5f * 0.00022676f + 0.002f;
    float expected_scale = expected_level * 0.27f;

    audio[0] = 1.0f;
    audio[1] = -9.0f;
    audio[2] = -0.5f;
    audio[3] = 4.0f;
    gp100_cabinet_reset(&cabinet);
    make_record(record, "Identity", 0u, 0x00800000);
    if (!gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        !gp100_cabinet_process(&cabinet, 50.0f, audio, 64u, true) ||
        !close_enough(cabinet.level, expected_level, 0.00000001f) ||
        !close_enough(audio[0], expected_scale, 0.00000001f) ||
        !close_enough(audio[1], expected_scale, 0.00000001f) ||
        !close_enough(audio[2], -0.5f * expected_scale, 0.00000001f) ||
        !close_enough(audio[3], audio[2], 0.00000001f)) {
        return 1;
    }
    return 0;
}

static int test_delay_bypass_and_reload(void)
{
    static gp100_cabinet_t cabinet;
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    float bypassed[128] = {1.0f, -1.0f};
    float audio[128] = {0.5f, 7.0f, 0.0f, 8.0f};
    float retained_level;

    gp100_cabinet_reset(&cabinet);
    make_record(record, "Delay", 1u, 0x00400000);
    if (!gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        !gp100_cabinet_process(
            &cabinet, 99.0f, bypassed, 64u, false) ||
        bypassed[0] != 1.0f || bypassed[1] != -1.0f ||
        cabinet.convolution.spectrum_position != 0u ||
        !gp100_cabinet_process(&cabinet, 99.0f, audio, 64u, true) ||
        !close_enough(audio[0], 0.0f, 0.00000001f) ||
        !close_enough(audio[1], 0.0f, 0.00000001f) ||
        !close_enough(audio[2], 0.5f * 0.5f * cabinet.level * 0.27f,
                      0.00000001f)) {
        return 1;
    }

    retained_level = cabinet.level;
    make_record(record, "Replacement", 0u, 0x00800000);
    if (!gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        cabinet.level != retained_level ||
        cabinet.convolution.spectrum_position != 0u ||
        cabinet.input_spectra[0].real != 0.0f ||
        cabinet.input_spectra[0].imaginary != 0.0f) {
        return 2;
    }
    return 0;
}

static int test_factory_dump(const char *path)
{
    static gp100_cabinet_t cabinet;
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    FILE *file = fopen(path, "rb");

    if (file == NULL || fseek(file, 0x00680000L, SEEK_SET) != 0 ||
        fread(record, 1u, sizeof(record), file) != sizeof(record)) {
        if (file != NULL) {
            fclose(file);
        }
        return 1;
    }
    fclose(file);

    gp100_cabinet_reset(&cabinet);
    if (!gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        strcmp(cabinet.name, "TWD 2x12") != 0 ||
        !close_enough(cabinet.impulse[0],
                      -62353.0f / 8388608.0f, 0.000000001f)) {
        return 2;
    }
    return 0;
}

static int test_last_partition(void)
{
    static gp100_cabinet_t cabinet;
    static float audio[GP100_CABINET_TAP_COUNT * 2u];
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    const size_t output_sample =
        (GP100_CABINET_TAP_COUNT - 1u) * 2u;

    memset(audio, 0, sizeof(audio));
    audio[0] = 0.5f;
    gp100_cabinet_reset(&cabinet);
    make_record(record, "Long delay", GP100_CABINET_TAP_COUNT - 1u,
                0x00400000);
    if (!gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        !gp100_cabinet_process(
            &cabinet, 99.0f, audio, GP100_CABINET_TAP_COUNT, true) ||
        !close_enough(
            audio[output_sample], 0.25f * cabinet.level * 0.27f,
            0.00000001f) ||
        !close_enough(audio[output_sample + 1u], audio[output_sample],
                      0.00000001f)) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    static gp100_cabinet_t cabinet;
    uint8_t record[GP100_CABINET_RECORD_SIZE] = {0};
    float audio[128] = {0.0f};

    gp100_cabinet_reset(&cabinet);
    if (gp100_cabinet_process(&cabinet, 50.0f, audio, 64u, true) ||
        gp100_cabinet_load_record(NULL, record, sizeof(record)) ||
        gp100_cabinet_load_record(
            &cabinet, record, GP100_CABINET_RECORD_SIZE - 1u) ||
        !gp100_cabinet_load_record(&cabinet, record, sizeof(record)) ||
        gp100_cabinet_process(&cabinet, -1.0f, audio, 64u, true) ||
        gp100_cabinet_process(&cabinet, 100.0f, audio, 64u, true) ||
        gp100_cabinet_process(&cabinet, 50.0f, NULL, 64u, true) ||
        gp100_cabinet_process(&cabinet, 50.0f, audio, 63u, true)) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FLASH_DUMP\n", argv[0]);
        return 2;
    }
    result = test_offsets_and_record_decode();
    if (result != 0) {
        fprintf(stderr, "record test failed: %d\n", result);
        return 10 + result;
    }
    result = test_identity_and_level_slew();
    if (result != 0) {
        fprintf(stderr, "identity test failed: %d\n", result);
        return 20 + result;
    }
    result = test_delay_bypass_and_reload();
    if (result != 0) {
        fprintf(stderr, "delay test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_dump(argv[1]);
    if (result != 0) {
        fprintf(stderr, "dump test failed: %d\n", result);
        return 40 + result;
    }
    result = test_last_partition();
    if (result != 0) {
        fprintf(stderr, "last-partition test failed: %d\n", result);
        return 45 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
