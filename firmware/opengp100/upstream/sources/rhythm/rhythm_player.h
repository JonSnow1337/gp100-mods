#ifndef GP100_RHYTHM_PLAYER_H
#define GP100_RHYTHM_PLAYER_H

#include "rhythm_engine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_RHYTHM_FACTORY_SAMPLE_COUNT 13u
#define GP100_RHYTHM_TRIGGER_QUEUE_LENGTH 16u
#define GP100_RHYTHM_TICKS_PER_QUARTER 120u
#define GP100_RHYTHM_MIN_TEMPO_BPM 40u
#define GP100_RHYTHM_MAX_TEMPO_BPM 250u
#define GP100_RHYTHM_MIX_LIMIT INT32_C(0x007ffff8)

typedef struct gp100_rhythm_pattern {
    const uint16_t *events;
    size_t event_count;
} gp100_rhythm_pattern_t;

/* PCM is signed 16-bit mono in consecutive 64-sample blocks. */
typedef struct gp100_rhythm_sample {
    const int16_t *pcm;
    uint16_t block_count;
} gp100_rhythm_sample_t;

typedef struct gp100_rhythm_clock {
    bool (*start)(void *context);
    void (*stop)(void *context);
    bool (*set_tempo_bpm)(void *context, uint16_t tempo_bpm);
    uint32_t (*drain_pending)(void *context);
    void *context;
} gp100_rhythm_clock_t;

typedef struct gp100_rhythm_voice {
    uint16_t trigger;
    uint16_t blocks_remaining;
    const int16_t *next_block;
    int16_t block[GP100_RHYTHM_BLOCK_SAMPLES];
} gp100_rhythm_voice_t;

typedef struct gp100_rhythm_player {
    const gp100_rhythm_pattern_t *patterns;
    size_t pattern_count;
    const gp100_rhythm_sample_t *samples;
    size_t sample_count;
    gp100_rhythm_clock_t clock;

    uint16_t pattern[GP100_RHYTHM_PATTERN_EVENTS];
    uint16_t current_pattern;
    uint16_t tempo_bpm;
    uint16_t event_index;
    bool enabled;
    uint32_t delay_ticks;
    uint32_t surplus_ticks;

    uint16_t trigger_queue[GP100_RHYTHM_TRIGGER_QUEUE_LENGTH];
    uint8_t trigger_read;
    uint8_t trigger_write;
    uint8_t trigger_count;
    uint32_t dropped_triggers;
    gp100_rhythm_voice_t voices[GP100_RHYTHM_MAX_VOICES];
} gp100_rhythm_player_t;

bool gp100_rhythm_player_init(
    gp100_rhythm_player_t *player,
    const gp100_rhythm_pattern_t *patterns,
    size_t pattern_count,
    const gp100_rhythm_sample_t *samples,
    size_t sample_count,
    const gp100_rhythm_clock_t *clock);

void gp100_rhythm_player_reset(gp100_rhythm_player_t *player);

/* These two functions match gp100_rhythm_hooks_t and can be assigned
 * directly to update_sequencer and render_voices. */
bool gp100_rhythm_player_update(
    void *context,
    uint16_t pattern_index,
    bool enabled,
    uint16_t tempo_bpm);
bool gp100_rhythm_player_render(
    void *context,
    int32_t *mono_samples,
    size_t sample_count);

/* Advance from an explicitly supplied elapsed-tick count. This is useful for
 * host tests and firmware that does not use the clock callback. */
bool gp100_rhythm_player_advance(
    gp100_rhythm_player_t *player,
    uint32_t elapsed_ticks);

/* Queue one factory-format trigger: high byte sample ID, low seven bits gain. */
bool gp100_rhythm_player_trigger(
    gp100_rhythm_player_t *player,
    uint16_t trigger);

uint32_t gp100_rhythm_player_dropped_triggers(
    const gp100_rhythm_player_t *player);

#ifdef __cplusplus
}
#endif

#endif
