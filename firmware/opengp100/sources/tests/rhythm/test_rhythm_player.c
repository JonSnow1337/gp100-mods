#include "rhythm/rhythm_player.h"

#include <stdio.h>
#include <string.h>

typedef struct clock_log {
    unsigned start_calls;
    unsigned stop_calls;
    unsigned tempo_calls;
    unsigned drain_calls;
    uint16_t tempo_bpm;
    uint32_t pending;
    bool fail_start;
    bool fail_tempo;
} clock_log_t;

static bool clock_start(void *context)
{
    clock_log_t *log = (clock_log_t *)context;

    ++log->start_calls;
    return !log->fail_start;
}

static void clock_stop(void *context)
{
    clock_log_t *log = (clock_log_t *)context;

    ++log->stop_calls;
}

static bool clock_set_tempo(void *context, uint16_t tempo_bpm)
{
    clock_log_t *log = (clock_log_t *)context;

    ++log->tempo_calls;
    log->tempo_bpm = tempo_bpm;
    return !log->fail_tempo;
}

static uint32_t clock_drain(void *context)
{
    clock_log_t *log = (clock_log_t *)context;
    uint32_t pending = log->pending;

    ++log->drain_calls;
    log->pending = 0u;
    return pending;
}

static void fill_pattern(uint16_t events[GP100_RHYTHM_PATTERN_EVENTS],
                         uint16_t value)
{
    size_t index;

    for (index = 0u; index < GP100_RHYTHM_PATTERN_EVENTS; ++index) {
        events[index] = value;
    }
}

static int test_sequencer_and_voice_pipeline(void)
{
    uint16_t events[GP100_RHYTHM_PATTERN_EVENTS];
    int16_t pcm[GP100_RHYTHM_BLOCK_SAMPLES];
    gp100_rhythm_player_t player;
    clock_log_t log = {0};
    const gp100_rhythm_clock_t clock = {
        clock_start, clock_stop, clock_set_tempo, clock_drain, &log
    };
    gp100_rhythm_pattern_t pattern = {events, sizeof(events) /
                                                    sizeof(events[0])};
    gp100_rhythm_sample_t sample = {pcm, 1u};
    int32_t output[GP100_RHYTHM_BLOCK_SAMPLES];
    size_t index;

    fill_pattern(events, UINT16_C(0xf000));
    events[0] = UINT16_C(0x8002);
    events[1] = 3u;
    events[2] = UINT16_C(0x8004);
    for (index = 0u; index < GP100_RHYTHM_BLOCK_SAMPLES; ++index) {
        pcm[index] = 1;
    }

    if (!gp100_rhythm_player_init(
            &player, &pattern, 1u, &sample, 1u, &clock)) {
        return 1;
    }
    /* Factory ordering observes enable first, then pattern change. Selecting a
     * new pattern therefore starts and immediately stops the timer. */
    if (!gp100_rhythm_player_update(&player, 0u, true, 120u) ||
        log.start_calls != 1u || log.stop_calls != 1u || player.enabled) {
        return 2;
    }
    if (!gp100_rhythm_player_update(&player, 0u, true, 120u) ||
        log.start_calls != 2u || log.tempo_calls != 1u ||
        log.tempo_bpm != 120u || player.delay_ticks != 3u ||
        player.trigger_count != 1u) {
        return 3;
    }

    if (!gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != 0 ||
        !gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != 2 || output[63] != 2) {
        return 4;
    }

    log.pending = 2u;
    if (!gp100_rhythm_player_update(&player, 0u, true, 120u) ||
        player.delay_ticks != 1u || player.trigger_count != 0u) {
        return 5;
    }
    log.pending = 1u;
    if (!gp100_rhythm_player_update(&player, 0u, true, 120u) ||
        player.delay_ticks != 0u || player.trigger_count != 1u) {
        return 6;
    }
    if (!gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != 0 ||
        !gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != 4) {
        return 7;
    }
    return 0;
}

static int test_saturation_and_queue_limit(void)
{
    uint16_t events[GP100_RHYTHM_PATTERN_EVENTS];
    int16_t pcm[GP100_RHYTHM_BLOCK_SAMPLES];
    gp100_rhythm_player_t player;
    gp100_rhythm_pattern_t pattern = {events, sizeof(events) /
                                                    sizeof(events[0])};
    gp100_rhythm_sample_t sample = {pcm, 1u};
    int32_t output[GP100_RHYTHM_BLOCK_SAMPLES];
    size_t index;

    fill_pattern(events, UINT16_C(0xf000));
    for (index = 0u; index < GP100_RHYTHM_BLOCK_SAMPLES; ++index) {
        pcm[index] = INT16_MAX;
    }
    if (!gp100_rhythm_player_init(
            &player, &pattern, 1u, &sample, 1u, NULL)) {
        return 1;
    }
    for (index = 0u; index < GP100_RHYTHM_TRIGGER_QUEUE_LENGTH;
         ++index) {
        if (!gp100_rhythm_player_trigger(&player, UINT16_C(0x007f))) {
            return 2;
        }
    }
    if (gp100_rhythm_player_trigger(&player, UINT16_C(0x007f)) ||
        gp100_rhythm_player_dropped_triggers(&player) != 1u) {
        return 3;
    }
    if (!gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != 0 ||
        !gp100_rhythm_player_render(
            &player, output, GP100_RHYTHM_BLOCK_SAMPLES) ||
        output[0] != GP100_RHYTHM_MIX_LIMIT) {
        return 4;
    }
    return 0;
}

static int test_validation_and_malformed_pattern(void)
{
    uint16_t events[GP100_RHYTHM_PATTERN_EVENTS];
    int16_t pcm[GP100_RHYTHM_BLOCK_SAMPLES] = {0};
    gp100_rhythm_player_t player;
    gp100_rhythm_pattern_t pattern = {events, sizeof(events) /
                                                    sizeof(events[0])};
    gp100_rhythm_sample_t sample = {pcm, 1u};

    fill_pattern(events, UINT16_C(0xffff));
    if (!gp100_rhythm_player_init(
            &player, &pattern, 1u, &sample, 1u, NULL) ||
        !gp100_rhythm_player_update(&player, 0u, false, 120u) ||
        gp100_rhythm_player_advance(&player, 0u)) {
        return 1;
    }
    pattern.event_count = GP100_RHYTHM_PATTERN_EVENTS - 1u;
    if (gp100_rhythm_player_init(
            &player, &pattern, 1u, &sample, 1u, NULL) ||
        gp100_rhythm_player_init(
            NULL, &pattern, 1u, &sample, 1u, NULL)) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result = test_sequencer_and_voice_pipeline();

    if (result != 0) {
        fprintf(stderr, "sequencer/voice test failed: %d\n", result);
        return 10 + result;
    }
    result = test_saturation_and_queue_limit();
    if (result != 0) {
        fprintf(stderr, "saturation/queue test failed: %d\n", result);
        return 20 + result;
    }
    result = test_validation_and_malformed_pattern();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
