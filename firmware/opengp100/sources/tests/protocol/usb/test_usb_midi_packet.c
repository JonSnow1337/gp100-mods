#include "protocol/usb/usb_midi_packet.h"

#include <stdio.h>
#include <string.h>

static int test_channel_messages(void)
{
    const uint8_t note[] = {0x92u, 60u, 100u};
    const uint8_t program[] = {0xc2u, 7u};
    const uint8_t expected_note[] = {0x19u, 0x92u, 60u, 100u};
    const uint8_t expected_program[] = {0x1cu, 0xc2u, 7u, 0u};
    uint8_t output[8];
    size_t output_length;

    if (!gp100_usb_midi_encode(
            1u, note, sizeof(note), output, sizeof(output), &output_length) ||
        output_length != sizeof(expected_note) ||
        memcmp(output, expected_note, sizeof(expected_note)) != 0 ||
        !gp100_usb_midi_encode(
            1u, program, sizeof(program),
            output, sizeof(output), &output_length) ||
        output_length != sizeof(expected_program) ||
        memcmp(output, expected_program, sizeof(expected_program)) != 0) {
        return 1;
    }
    return 0;
}

static int test_sysex_packets(void)
{
    const uint8_t sysex[] = {
        0xf0u, 0x21u, 0x25u, 0x01u, 0x47u, 0x50u, 0xf7u
    };
    const uint8_t expected[] = {
        0x24u, 0xf0u, 0x21u, 0x25u,
        0x24u, 0x01u, 0x47u, 0x50u,
        0x25u, 0xf7u, 0u, 0u
    };
    uint8_t output[16];
    size_t output_length;

    if (gp100_usb_midi_encoded_size(sysex, sizeof(sysex)) !=
            sizeof(expected) ||
        !gp100_usb_midi_encode(
            2u, sysex, sizeof(sysex),
            output, sizeof(output), &output_length) ||
        output_length != sizeof(expected) ||
        memcmp(output, expected, sizeof(expected)) != 0) {
        return 1;
    }
    return 0;
}

static int test_exact_three_byte_sysex_end(void)
{
    const uint8_t sysex[] = {0xf0u, 0x01u, 0xf7u};
    const uint8_t expected[] = {0x07u, 0xf0u, 0x01u, 0xf7u};
    uint8_t output[4];
    size_t output_length;

    if (!gp100_usb_midi_encode(
            0u, sysex, sizeof(sysex),
            output, sizeof(output), &output_length) ||
        output_length != sizeof(expected) ||
        memcmp(output, expected, sizeof(expected)) != 0) {
        return 1;
    }
    return 0;
}

static int test_validation_and_capacity(void)
{
    const uint8_t note[] = {0x90u, 60u, 100u};
    const uint8_t incomplete[] = {0x90u, 60u};
    uint8_t output[4];
    size_t output_length = 99u;

    if (gp100_usb_midi_encode(
            0u, note, sizeof(note), output, 3u, &output_length) ||
        output_length != 4u ||
        gp100_usb_midi_encode(
            0u, incomplete, sizeof(incomplete),
            output, sizeof(output), &output_length) ||
        output_length != 0u ||
        gp100_usb_midi_encode(
            16u, note, sizeof(note),
            output, sizeof(output), &output_length)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_channel_messages();
    if (result != 0) {
        fprintf(stderr, "channel-message test failed: %d\n", result);
        return 10 + result;
    }
    result = test_sysex_packets();
    if (result != 0) {
        fprintf(stderr, "SysEx packet test failed: %d\n", result);
        return 20 + result;
    }
    result = test_exact_three_byte_sysex_end();
    if (result != 0) {
        fprintf(stderr, "three-byte SysEx end test failed: %d\n", result);
        return 30 + result;
    }
    result = test_validation_and_capacity();
    if (result != 0) {
        fprintf(stderr, "validation/capacity test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
