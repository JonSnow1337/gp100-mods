#include "protocol/usb/usb_audio_capture.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.000001f;
}

static int test_gain_table(void)
{
    float gain;

    if (!gp100_usb_capture_gain_for_db(-20, &gain) ||
        !close_enough(gain, 0.1f) ||
        !gp100_usb_capture_gain_for_db(0, &gain) ||
        gain != 1.0f ||
        !gp100_usb_capture_gain_for_db(20, &gain) ||
        gain != 10.0f ||
        gp100_usb_capture_gain_for_db(-21, &gain) ||
        gp100_usb_capture_gain_for_db(21, &gain) ||
        gp100_usb_capture_gain_for_db(0, NULL)) {
        return 1;
    }
    return 0;
}

static int test_gain_table_against_artifact(const char *artifact_path)
{
    FILE *artifact = fopen(artifact_path, "rb");
    int16_t db;

    if (artifact == NULL) {
        return 1;
    }
    for (db = GP100_USB_CAPTURE_GAIN_MIN_DB;
         db <= GP100_USB_CAPTURE_GAIN_MAX_DB;
         ++db) {
        const long table_index =
            (long)(db - GP100_USB_CAPTURE_GAIN_MIN_DB) *
            (long)sizeof(uint32_t);
        uint32_t input_bits;
        uint32_t usb_bits;
        uint32_t reference_bits;
        float gain;

        if (fseek(artifact, 0x1cb760L + table_index, SEEK_SET) != 0 ||
            fread(&input_bits, sizeof(input_bits), 1u, artifact) != 1u ||
            fseek(artifact, 0x1cbe0cL + table_index, SEEK_SET) != 0 ||
            fread(&usb_bits, sizeof(usb_bits), 1u, artifact) != 1u ||
            !gp100_usb_capture_gain_for_db(db, &gain)) {
            fclose(artifact);
            return 2;
        }
        memcpy(&reference_bits, &gain, sizeof(reference_bits));
        if (reference_bits != input_bits || reference_bits != usb_bits) {
            fclose(artifact);
            return 3;
        }
    }
    fclose(artifact);
    return 0;
}

static int test_gain_slew_and_sample_conversion(void)
{
    gp100_usb_capture_ring_t capture;
    const int32_t samples[4] = {
        8388608, -8388608,
        8388608, -8388608
    };

    gp100_usb_capture_reset(&capture);
    if (!gp100_usb_capture_publish(&capture, samples, 2u, 0) ||
        capture.left[0] != 0 || capture.right[0] != 0 ||
        capture.left[1] != 83886 || capture.right[1] != -83886 ||
        capture.write_index != 2u ||
        !close_enough(capture.current_gain, 0.02f)) {
        return 1;
    }
    return 0;
}

static int test_source_selection(void)
{
    int32_t dry[4] = {1, 2, 3, 4};
    const int32_t effect[4] = {10, 20, 30, 40};
    int32_t output[4] = {0};

    if (!gp100_usb_capture_select_sources(
            dry, effect, output, 2u,
            GP100_USB_RECORD_SOURCE_DRY,
            GP100_USB_RECORD_SOURCE_EFFECT) ||
        output[0] != 1 || output[1] != 20 ||
        output[2] != 3 || output[3] != 40 ||
        !gp100_usb_capture_select_sources(
            dry, effect, dry, 2u,
            GP100_USB_RECORD_SOURCE_EFFECT,
            GP100_USB_RECORD_SOURCE_DRY) ||
        dry[0] != 10 || dry[1] != 2 || dry[2] != 30 || dry[3] != 4) {
        return 1;
    }
    if (gp100_usb_capture_select_sources(
            NULL, effect, output, 1u,
            GP100_USB_RECORD_SOURCE_DRY,
            GP100_USB_RECORD_SOURCE_EFFECT) ||
        gp100_usb_capture_select_sources(
            dry, effect, output, 1u,
            (gp100_usb_record_source_t)2,
            GP100_USB_RECORD_SOURCE_EFFECT) ||
        !gp100_usb_capture_select_sources(
            NULL, NULL, NULL, 0u,
            GP100_USB_RECORD_SOURCE_DRY,
            GP100_USB_RECORD_SOURCE_EFFECT)) {
        return 2;
    }
    return 0;
}

static int test_saturation_wrap_and_validation(void)
{
    gp100_usb_capture_ring_t capture;
    const int32_t samples[4] = {
        INT32_MAX, INT32_MIN,
        8388608, -8388608
    };

    gp100_usb_capture_reset(&capture);
    capture.current_gain = 10.0f;
    capture.write_index = GP100_USB_CAPTURE_RING_FRAMES - 1u;
    if (!gp100_usb_capture_publish(&capture, samples, 2u, 20) ||
        capture.left[319] != 8388607 || capture.right[319] != -8388607 ||
        capture.left[0] != 8388607 || capture.right[0] != -8388607 ||
        capture.write_index != 1u ||
        gp100_usb_capture_publish(NULL, samples, 2u, 0) ||
        gp100_usb_capture_publish(&capture, NULL, 2u, 0) ||
        gp100_usb_capture_publish(&capture, samples, SIZE_MAX, 0) ||
        gp100_usb_capture_publish(&capture, samples, 2u, 21) ||
        !gp100_usb_capture_publish(&capture, NULL, 0u, 0)) {
        return 1;
    }

    capture.write_index = GP100_USB_CAPTURE_RING_FRAMES;
    if (gp100_usb_capture_publish(&capture, samples, 2u, 0)) {
        return 2;
    }
    return 0;
}

static int test_reader_startup(void)
{
    gp100_usb_capture_ring_t capture;
    int32_t output[4] = {1, 2, 3, 4};

    gp100_usb_capture_reset(&capture);
    capture.write_index = 191u;
    if (gp100_usb_capture_available(&capture) != 191u ||
        gp100_usb_capture_read(&capture, output, 2u) !=
            GP100_USB_CAPTURE_READ_BUFFERING ||
        capture.started || output[0] != 0 || output[3] != 0) {
        return 1;
    }

    output[0] = 1;
    output[3] = 4;
    capture.write_index = 192u;
    if (gp100_usb_capture_read(&capture, output, 2u) !=
            GP100_USB_CAPTURE_READ_BUFFERING ||
        !capture.started || output[0] != 0 || output[3] != 0) {
        return 2;
    }
    return 0;
}

static int test_reader_phase_control(void)
{
    gp100_usb_capture_ring_t capture;
    int32_t output[4];

    gp100_usb_capture_reset(&capture);
    capture.started = true;
    capture.write_index = 200u;
    capture.left[319] = 640;
    capture.right[319] = -640;
    capture.left[0] = 1280;
    capture.right[0] = -1280;
    if (gp100_usb_capture_read(&capture, output, 2u) !=
            GP100_USB_CAPTURE_READ_AUDIO ||
        output[0] != 640 || output[1] != -640 ||
        output[2] != 1280 || output[3] != -1280 ||
        capture.read_index != 2u || capture.phase != 0u) {
        return 1;
    }

    capture.read_index = 0u;
    capture.write_index = 100u;
    capture.phase = 0u;
    if (gp100_usb_capture_read(&capture, output, 2u) !=
            GP100_USB_CAPTURE_READ_AUDIO ||
        capture.read_index != 1u || capture.phase != 62u) {
        return 2;
    }

    capture.read_index = 0u;
    capture.write_index = 300u;
    capture.phase = 63u;
    if (gp100_usb_capture_read(&capture, output, 1u) !=
            GP100_USB_CAPTURE_READ_AUDIO ||
        capture.read_index != 2u || capture.phase != 0u) {
        return 3;
    }
    return 0;
}

static int test_reader_phase_boundaries(void)
{
    static const struct {
        size_t occupancy;
        size_t expected_index;
        uint16_t expected_phase;
    } cases[] = {
        {128u, 0u, 63u},
        {129u, 1u, 0u},
        {255u, 1u, 0u},
        {256u, 1u, 1u}
    };
    gp100_usb_capture_ring_t capture;
    int32_t output[2];
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        gp100_usb_capture_reset(&capture);
        capture.started = true;
        capture.write_index = cases[index].occupancy;
        if (gp100_usb_capture_read(&capture, output, 1u) !=
                GP100_USB_CAPTURE_READ_AUDIO ||
            capture.read_index != cases[index].expected_index ||
            capture.phase != cases[index].expected_phase) {
            return (int)index + 1;
        }
    }
    return 0;
}

static int test_clear_live_buffer(void)
{
    gp100_usb_capture_ring_t capture;
    const int32_t first_frame[] = {2000, -2000};
    size_t index;

    gp100_usb_capture_reset(&capture);
    capture.current_gain = 0.5f;
    capture.left[319] = 1;
    capture.right[319] = -1;
    capture.write_index = 23u;
    capture.read_index = 5u;
    capture.phase = 12u;
    capture.started = true;
    gp100_usb_capture_clear_buffer(&capture);
    if (capture.write_index != 0u || capture.read_index != 0u ||
        capture.phase != 0u || capture.started ||
        capture.current_gain != 0.5f) {
        return 1;
    }
    for (index = 0u; index < GP100_USB_CAPTURE_RING_FRAMES; ++index) {
        if (capture.left[index] != 0 || capture.right[index] != 0) {
            return 2;
        }
    }
    if (!gp100_usb_capture_publish(&capture, first_frame, 1u, 0) ||
        capture.left[0] != 999 || capture.right[0] != -999) {
        return 3;
    }
    return 0;
}

static int test_reader_validation(void)
{
    gp100_usb_capture_ring_t capture;
    int32_t output[2];

    gp100_usb_capture_reset(&capture);
    if (gp100_usb_capture_read(NULL, output, 1u) !=
            GP100_USB_CAPTURE_READ_ERROR ||
        gp100_usb_capture_read(&capture, NULL, 1u) !=
            GP100_USB_CAPTURE_READ_ERROR ||
        gp100_usb_capture_read(&capture, output, SIZE_MAX) !=
            GP100_USB_CAPTURE_READ_ERROR ||
        gp100_usb_capture_read(&capture, NULL, 0u) !=
            GP100_USB_CAPTURE_READ_BUFFERING) {
        return 1;
    }
    capture.phase = 64u;
    if (gp100_usb_capture_read(&capture, output, 1u) !=
            GP100_USB_CAPTURE_READ_ERROR) {
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s APP_SDRAM_BIN\n", argv[0]);
        return 1;
    }
    result = test_gain_table();
    if (result != 0) {
        fprintf(stderr, "gain-table test failed: %d\n", result);
        return 10 + result;
    }
    result = test_gain_table_against_artifact(argv[1]);
    if (result != 0) {
        fprintf(stderr, "artifact-table test failed: %d\n", result);
        return 15 + result;
    }
    result = test_gain_slew_and_sample_conversion();
    if (result != 0) {
        fprintf(stderr, "gain/samples test failed: %d\n", result);
        return 20 + result;
    }
    result = test_source_selection();
    if (result != 0) {
        fprintf(stderr, "source-selection test failed: %d\n", result);
        return 25 + result;
    }
    result = test_saturation_wrap_and_validation();
    if (result != 0) {
        fprintf(stderr, "saturation/wrap test failed: %d\n", result);
        return 30 + result;
    }
    result = test_reader_startup();
    if (result != 0) {
        fprintf(stderr, "reader-startup test failed: %d\n", result);
        return 40 + result;
    }
    result = test_reader_phase_control();
    if (result != 0) {
        fprintf(stderr, "reader-phase test failed: %d\n", result);
        return 50 + result;
    }
    result = test_reader_validation();
    if (result != 0) {
        fprintf(stderr, "reader-validation test failed: %d\n", result);
        return 60 + result;
    }
    result = test_reader_phase_boundaries();
    if (result != 0) {
        fprintf(stderr, "reader phase-boundary test failed: %d\n", result);
        return 70 + result;
    }
    result = test_clear_live_buffer();
    if (result != 0) {
        fprintf(stderr, "clear live buffer test failed: %d\n", result);
        return 80 + result;
    }
    return 0;
}
