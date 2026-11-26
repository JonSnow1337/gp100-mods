#include "protocol/midi/midi_stream.h"

#include <stdio.h>
#include <string.h>

static int expect_message(
    gp100_midi_stream_t *stream,
    const uint8_t *expected,
    size_t expected_length)
{
    uint8_t message[32];
    size_t length = 0u;

    if (gp100_midi_stream_poll(
            stream, message, sizeof(message), &length) !=
            GP100_MIDI_POLL_MESSAGE ||
        length != expected_length ||
        memcmp(message, expected, expected_length) != 0) {
        return 1;
    }
    return 0;
}

static int test_channel_messages(void)
{
    gp100_midi_stream_t stream;
    const uint8_t input[] = {0x90u, 60u, 100u, 0xc2u, 7u};

    gp100_midi_stream_reset(&stream);
    if (!gp100_midi_stream_push(&stream, input, sizeof(input)) ||
        expect_message(&stream, input, 3u) != 0 ||
        expect_message(&stream, input + 3u, 2u) != 0 ||
        gp100_midi_stream_count(&stream) != 0u) {
        return 1;
    }
    return 0;
}

static int test_waiting_and_output_capacity(void)
{
    gp100_midi_stream_t stream;
    const uint8_t partial[] = {0xb0u, 7u};
    const uint8_t value = 99u;
    uint8_t message[2];
    size_t length = 123u;

    gp100_midi_stream_reset(&stream);
    if (!gp100_midi_stream_push(&stream, partial, sizeof(partial)) ||
        gp100_midi_stream_poll(
            &stream, message, sizeof(message), &length) !=
            GP100_MIDI_POLL_WAITING ||
        gp100_midi_stream_count(&stream) != 2u ||
        !gp100_midi_stream_push(&stream, &value, 1u) ||
        gp100_midi_stream_poll(
            &stream, message, sizeof(message), &length) !=
            GP100_MIDI_POLL_OUTPUT_TOO_SMALL ||
        length != 3u || gp100_midi_stream_count(&stream) != 3u) {
        return 1;
    }
    return 0;
}

static int test_sysex_and_length_frame(void)
{
    gp100_midi_stream_t stream;
    const uint8_t sysex_a[] = {0xf0u, 0x21u, 0x25u};
    const uint8_t sysex_b[] = {0x47u, 0x50u, 0xf7u};
    const uint8_t sysex[] = {
        0xf0u, 0x21u, 0x25u, 0x47u, 0x50u, 0xf7u
    };
    const uint8_t frame[] = {0xffu, 0x44u, 3u, 1u, 2u, 3u};
    uint8_t message[16];
    size_t length;

    gp100_midi_stream_reset(&stream);
    if (!gp100_midi_stream_push(&stream, sysex_a, sizeof(sysex_a)) ||
        gp100_midi_stream_poll(
            &stream, message, sizeof(message), &length) !=
            GP100_MIDI_POLL_WAITING ||
        !gp100_midi_stream_push(&stream, sysex_b, sizeof(sysex_b)) ||
        expect_message(&stream, sysex, sizeof(sysex)) != 0 ||
        !gp100_midi_stream_push(&stream, frame, sizeof(frame)) ||
        expect_message(&stream, frame, sizeof(frame)) != 0) {
        return 1;
    }
    return 0;
}

static int test_wrap_and_discard(void)
{
    gp100_midi_stream_t stream;
    const uint8_t message[] = {0xe1u, 0u, 64u};
    uint8_t output[3];
    size_t length;

    gp100_midi_stream_reset(&stream);
    stream.read_index = GP100_MIDI_STREAM_CAPACITY - 1u;
    stream.write_index = stream.read_index;
    if (!gp100_midi_stream_push(&stream, message, sizeof(message)) ||
        expect_message(&stream, message, sizeof(message)) != 0 ||
        !gp100_midi_stream_push(
            &stream, (const uint8_t[]){0x45u}, 1u) ||
        gp100_midi_stream_poll(
            &stream, output, sizeof(output), &length) !=
            GP100_MIDI_POLL_DISCARDED ||
        gp100_midi_stream_count(&stream) != 0u) {
        return 1;
    }
    return 0;
}

static int test_full_capacity_is_unambiguous(void)
{
    gp100_midi_stream_t stream;
    uint8_t byte = 0u;
    size_t index;

    gp100_midi_stream_reset(&stream);
    for (index = 0u; index < GP100_MIDI_STREAM_CAPACITY; ++index) {
        if (!gp100_midi_stream_push(&stream, &byte, 1u)) {
            return 1;
        }
    }
    if (gp100_midi_stream_count(&stream) !=
            GP100_MIDI_STREAM_CAPACITY ||
        gp100_midi_stream_push(&stream, &byte, 1u)) {
        return 2;
    }
    return 0;
}

static int test_bounded_peek_and_consume(void)
{
    gp100_midi_stream_t stream;
    const uint8_t input[] = {1u, 2u, 3u, 4u};
    uint8_t output[4] = {0};

    gp100_midi_stream_reset(&stream);
    stream.read_index = GP100_MIDI_STREAM_CAPACITY - 2u;
    stream.write_index = stream.read_index;
    if (!gp100_midi_stream_push(&stream, input, sizeof(input)) ||
        !gp100_midi_stream_read(
            &stream, output, sizeof(output), false) ||
        memcmp(output, input, sizeof(input)) != 0 ||
        gp100_midi_stream_count(&stream) != sizeof(input) ||
        stream.read_index != GP100_MIDI_STREAM_CAPACITY - 2u ||
        gp100_midi_stream_read(
            &stream, output, sizeof(output) + 1u, false) ||
        !gp100_midi_stream_read(&stream, output, 3u, true) ||
        memcmp(output, input, 3u) != 0 ||
        gp100_midi_stream_count(&stream) != 1u ||
        stream.read_index != 1u ||
        !gp100_midi_stream_read(&stream, output, 0u, true) ||
        gp100_midi_stream_read(&stream, NULL, 1u, false)) {
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
    result = test_waiting_and_output_capacity();
    if (result != 0) {
        fprintf(stderr, "waiting/capacity test failed: %d\n", result);
        return 20 + result;
    }
    result = test_sysex_and_length_frame();
    if (result != 0) {
        fprintf(stderr, "SysEx/length-frame test failed: %d\n", result);
        return 30 + result;
    }
    result = test_wrap_and_discard();
    if (result != 0) {
        fprintf(stderr, "wrap/discard test failed: %d\n", result);
        return 40 + result;
    }
    result = test_full_capacity_is_unambiguous();
    if (result != 0) {
        fprintf(stderr, "full-capacity test failed: %d\n", result);
        return 50 + result;
    }
    result = test_bounded_peek_and_consume();
    if (result != 0) {
        fprintf(stderr, "peek/consume test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
