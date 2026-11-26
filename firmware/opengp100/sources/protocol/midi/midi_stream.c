#include "midi_stream.h"

#include <string.h>

static size_t advance_index(size_t index)
{
    ++index;
    if (index == GP100_MIDI_STREAM_CAPACITY) {
        index = 0u;
    }
    return index;
}

static uint8_t peek_byte(
    const gp100_midi_stream_t *stream,
    size_t offset)
{
    size_t index = stream->read_index + offset;

    if (index >= GP100_MIDI_STREAM_CAPACITY) {
        index -= GP100_MIDI_STREAM_CAPACITY;
    }
    return stream->bytes[index];
}

bool gp100_midi_stream_read(
    gp100_midi_stream_t *stream,
    uint8_t *destination,
    size_t byte_count,
    bool consume)
{
    size_t read_index;
    size_t offset;

    if (stream == NULL || (byte_count != 0u && destination == NULL) ||
        byte_count > stream->used) {
        return false;
    }
    read_index = stream->read_index;
    for (offset = 0u; offset < byte_count; ++offset) {
        destination[offset] = stream->bytes[read_index];
        read_index = advance_index(read_index);
    }
    if (consume) {
        stream->read_index = read_index;
        stream->used -= byte_count;
    }
    return true;
}

void gp100_midi_stream_reset(gp100_midi_stream_t *stream)
{
    if (stream == NULL) {
        return;
    }
    stream->read_index = 0u;
    stream->write_index = 0u;
    stream->used = 0u;
}

size_t gp100_midi_stream_count(
    const gp100_midi_stream_t *stream)
{
    return stream == NULL ? 0u : stream->used;
}

bool gp100_midi_stream_push(
    gp100_midi_stream_t *stream,
    const uint8_t *bytes,
    size_t byte_count)
{
    size_t offset;

    if (stream == NULL || (byte_count != 0u && bytes == NULL) ||
        byte_count > GP100_MIDI_STREAM_CAPACITY - stream->used) {
        return false;
    }
    for (offset = 0u; offset < byte_count; ++offset) {
        stream->bytes[stream->write_index] = bytes[offset];
        stream->write_index = advance_index(stream->write_index);
    }
    stream->used += byte_count;
    return true;
}

static size_t channel_message_length(uint8_t status)
{
    uint8_t kind = status & 0xf0u;

    if (kind == 0xc0u || kind == 0xd0u) {
        return 2u;
    }
    if (kind >= 0x80u && kind <= 0xe0u) {
        return 3u;
    }
    return 0u;
}

static size_t complete_sysex_length(
    const gp100_midi_stream_t *stream)
{
    size_t offset;

    for (offset = 1u; offset < stream->used; ++offset) {
        if (peek_byte(stream, offset) == 0xf7u) {
            return offset + 1u;
        }
    }
    return 0u;
}

gp100_midi_poll_result_t gp100_midi_stream_poll(
    gp100_midi_stream_t *stream,
    uint8_t *message,
    size_t message_capacity,
    size_t *message_length)
{
    uint8_t status;
    size_t required;

    if (message_length != NULL) {
        *message_length = 0u;
    }
    if (stream == NULL || message == NULL || message_length == NULL) {
        return GP100_MIDI_POLL_IDLE;
    }
    if (stream->used == 0u) {
        return GP100_MIDI_POLL_IDLE;
    }

    status = peek_byte(stream, 0u);
    required = channel_message_length(status);
    if (required == 0u && status == 0xf0u) {
        required = complete_sysex_length(stream);
        if (required == 0u) {
            return GP100_MIDI_POLL_WAITING;
        }
    } else if (required == 0u && status == 0xffu) {
        if (stream->used < 3u) {
            return GP100_MIDI_POLL_WAITING;
        }
        required = (size_t)peek_byte(stream, 2u) + 3u;
    } else if (required == 0u) {
        stream->read_index = advance_index(stream->read_index);
        --stream->used;
        return GP100_MIDI_POLL_DISCARDED;
    }

    if (stream->used < required) {
        return GP100_MIDI_POLL_WAITING;
    }
    if (message_capacity < required) {
        *message_length = required;
        return GP100_MIDI_POLL_OUTPUT_TOO_SMALL;
    }

    if (!gp100_midi_stream_read(
            stream, message, required, true)) {
        return GP100_MIDI_POLL_IDLE;
    }
    *message_length = required;
    return GP100_MIDI_POLL_MESSAGE;
}
