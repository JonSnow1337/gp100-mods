#ifndef GP100_MIDI_STREAM_H
#define GP100_MIDI_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_MIDI_STREAM_CAPACITY 0x2400u

typedef struct gp100_midi_stream {
    size_t read_index;
    size_t write_index;
    size_t used;
    uint8_t bytes[GP100_MIDI_STREAM_CAPACITY];
} gp100_midi_stream_t;

typedef enum gp100_midi_poll_result {
    GP100_MIDI_POLL_IDLE = 0,
    GP100_MIDI_POLL_WAITING,
    GP100_MIDI_POLL_DISCARDED,
    GP100_MIDI_POLL_OUTPUT_TOO_SMALL,
    GP100_MIDI_POLL_MESSAGE
} gp100_midi_poll_result_t;

void gp100_midi_stream_reset(gp100_midi_stream_t *stream);

size_t gp100_midi_stream_count(
    const gp100_midi_stream_t *stream);

bool gp100_midi_stream_push(
    gp100_midi_stream_t *stream,
    const uint8_t *bytes,
    size_t byte_count);

/* Copy a bounded range from the read cursor. When consume is true, advance
 * the cursor and remove the bytes; otherwise leave the stream unchanged. */
bool gp100_midi_stream_read(
    gp100_midi_stream_t *stream,
    uint8_t *destination,
    size_t byte_count,
    bool consume);

/*
 * Extract at most one factory-recognized message. The accepted forms are:
 * standard two- or three-byte channel messages, F0..F7 System Exclusive, and
 * the observed FF xx length payload frame whose total size is byte[2] + 3.
 */
gp100_midi_poll_result_t gp100_midi_stream_poll(
    gp100_midi_stream_t *stream,
    uint8_t *message,
    size_t message_capacity,
    size_t *message_length);

#ifdef __cplusplus
}
#endif

#endif
