#include "rhythm_player.h"

#include <string.h>

#define RHYTHM_EVENT_TRIGGER_FIRST UINT16_C(0x8000)
#define RHYTHM_EVENT_TRIGGER_LAST UINT16_C(0xefff)
#define RHYTHM_EVENT_RESTART UINT16_C(0xffff)
#define RHYTHM_TRIGGER_VALUE_MASK UINT16_C(0x7fff)
#define RHYTHM_TRIGGER_GAIN_MASK UINT16_C(0x007f)

static void stop_clock(gp100_rhythm_player_t *player)
{
    if (player->clock.stop != NULL) {
        player->clock.stop(player->clock.context);
    }
}

static void reset_sequencer(gp100_rhythm_player_t *player)
{
    player->event_index = 0u;
    player->delay_ticks = 0u;
    player->surplus_ticks = 0u;
}

static bool assets_are_valid(
    const gp100_rhythm_pattern_t *patterns,
    size_t pattern_count,
    const gp100_rhythm_sample_t *samples,
    size_t sample_count)
{
    size_t index;

    if (patterns == NULL || pattern_count == 0u ||
        pattern_count > UINT16_MAX || samples == NULL ||
        sample_count == 0u || sample_count > UINT8_MAX) {
        return false;
    }
    for (index = 0u; index < pattern_count; ++index) {
        if (patterns[index].events == NULL ||
            patterns[index].event_count !=
                GP100_RHYTHM_PATTERN_EVENTS) {
            return false;
        }
    }
    for (index = 0u; index < sample_count; ++index) {
        if (samples[index].block_count != 0u &&
            samples[index].pcm == NULL) {
            return false;
        }
    }
    return true;
}

bool gp100_rhythm_player_init(
    gp100_rhythm_player_t *player,
    const gp100_rhythm_pattern_t *patterns,
    size_t pattern_count,
    const gp100_rhythm_sample_t *samples,
    size_t sample_count,
    const gp100_rhythm_clock_t *clock)
{
    if (player == NULL ||
        !assets_are_valid(patterns, pattern_count, samples, sample_count)) {
        return false;
    }

    memset(player, 0, sizeof(*player));
    player->patterns = patterns;
    player->pattern_count = pattern_count;
    player->samples = samples;
    player->sample_count = sample_count;
    if (clock != NULL) {
        player->clock = *clock;
    }
    player->current_pattern = UINT16_MAX;
    player->tempo_bpm = UINT16_MAX;
    return true;
}

void gp100_rhythm_player_reset(gp100_rhythm_player_t *player)
{
    const gp100_rhythm_pattern_t *patterns;
    const gp100_rhythm_sample_t *samples;
    size_t pattern_count;
    size_t sample_count;
    gp100_rhythm_clock_t clock;

    if (player == NULL) {
        return;
    }
    patterns = player->patterns;
    pattern_count = player->pattern_count;
    samples = player->samples;
    sample_count = player->sample_count;
    clock = player->clock;
    stop_clock(player);
    memset(player, 0, sizeof(*player));
    player->patterns = patterns;
    player->pattern_count = pattern_count;
    player->samples = samples;
    player->sample_count = sample_count;
    player->clock = clock;
    player->current_pattern = UINT16_MAX;
    player->tempo_bpm = UINT16_MAX;
}

bool gp100_rhythm_player_trigger(
    gp100_rhythm_player_t *player,
    uint16_t trigger)
{
    if (player == NULL) {
        return false;
    }
    if (player->trigger_count == GP100_RHYTHM_TRIGGER_QUEUE_LENGTH) {
        ++player->dropped_triggers;
        return false;
    }
    player->trigger_queue[player->trigger_write] = trigger;
    player->trigger_write = (uint8_t)(
        (player->trigger_write + 1u) %
        GP100_RHYTHM_TRIGGER_QUEUE_LENGTH);
    ++player->trigger_count;
    return true;
}

bool gp100_rhythm_player_advance(
    gp100_rhythm_player_t *player,
    uint32_t elapsed_ticks)
{
    uint32_t available;
    size_t events_examined = 0u;

    if (player == NULL || player->current_pattern == UINT16_MAX) {
        return false;
    }

    available = player->surplus_ticks + elapsed_ticks;
    if (available < player->delay_ticks) {
        player->delay_ticks -= available;
        player->surplus_ticks = 0u;
        return true;
    }
    player->surplus_ticks = available - player->delay_ticks;
    player->delay_ticks = 0u;

    while (events_examined < GP100_RHYTHM_PATTERN_EVENTS) {
        uint16_t event;

        if (player->event_index >= GP100_RHYTHM_PATTERN_EVENTS) {
            player->event_index = 0u;
        }
        event = player->pattern[player->event_index++];
        ++events_examined;

        if (event < RHYTHM_EVENT_TRIGGER_FIRST) {
            player->delay_ticks = event;
            return true;
        }
        if (event <= RHYTHM_EVENT_TRIGGER_LAST) {
            (void)gp100_rhythm_player_trigger(
                player, event & RHYTHM_TRIGGER_VALUE_MASK);
            continue;
        }
        if (event == RHYTHM_EVENT_RESTART) {
            player->event_index = 0u;
            continue;
        }

        /* Factory values 0xf000..0xfffe end this update without installing a
         * delay. They are retained as a legal pattern-control boundary. */
        return true;
    }

    /* A malformed pattern made a full pass without reaching a delay or
     * control boundary. The shipped loop would not terminate; report it. */
    return false;
}

bool gp100_rhythm_player_update(
    void *context,
    uint16_t pattern_index,
    bool enabled,
    uint16_t tempo_bpm)
{
    gp100_rhythm_player_t *player =
        (gp100_rhythm_player_t *)context;
    uint32_t pending = 0u;

    if (player == NULL || pattern_index >= player->pattern_count ||
        tempo_bpm < GP100_RHYTHM_MIN_TEMPO_BPM ||
        tempo_bpm > GP100_RHYTHM_MAX_TEMPO_BPM) {
        return false;
    }

    if (enabled != player->enabled) {
        if (enabled) {
            if (player->clock.start != NULL &&
                !player->clock.start(player->clock.context)) {
                return false;
            }
            player->enabled = true;
        } else {
            stop_clock(player);
            player->enabled = false;
            reset_sequencer(player);
        }
    }

    if (pattern_index != player->current_pattern) {
        stop_clock(player);
        player->enabled = false;
        reset_sequencer(player);
        memcpy(player->pattern, player->patterns[pattern_index].events,
               sizeof(player->pattern));
        player->current_pattern = pattern_index;
        return true;
    }

    if (tempo_bpm != player->tempo_bpm) {
        if (player->clock.set_tempo_bpm != NULL &&
            !player->clock.set_tempo_bpm(
                player->clock.context, tempo_bpm)) {
            return false;
        }
        player->tempo_bpm = tempo_bpm;
    }

    if (!player->enabled) {
        return true;
    }
    if (player->clock.drain_pending != NULL) {
        pending = player->clock.drain_pending(player->clock.context);
    }
    return gp100_rhythm_player_advance(player, pending);
}

static void start_queued_voices(gp100_rhythm_player_t *player)
{
    while (player->trigger_count != 0u) {
        uint16_t trigger = player->trigger_queue[player->trigger_read];
        size_t voice_index;
        size_t sample_index;

        player->trigger_read = (uint8_t)(
            (player->trigger_read + 1u) %
            GP100_RHYTHM_TRIGGER_QUEUE_LENGTH);
        --player->trigger_count;

        /* Zero is the factory queue sentinel. It consumes one entry and ends
         * this drain, leaving later entries for the next render call. */
        if (trigger == 0u) {
            break;
        }
        sample_index = trigger >> 8;
        if (sample_index >= player->sample_count) {
            ++player->dropped_triggers;
            continue;
        }
        for (voice_index = 0u;
             voice_index < GP100_RHYTHM_MAX_VOICES;
             ++voice_index) {
            gp100_rhythm_voice_t *voice =
                &player->voices[voice_index];

            if (voice->blocks_remaining == 0u) {
                voice->trigger = trigger;
                voice->next_block = player->samples[sample_index].pcm;
                voice->blocks_remaining =
                    player->samples[sample_index].block_count;
                break;
            }
        }
        if (voice_index == GP100_RHYTHM_MAX_VOICES) {
            ++player->dropped_triggers;
        }
    }
}

static int32_t clamp_mix(int32_t value)
{
    if (value > GP100_RHYTHM_MIX_LIMIT) {
        return GP100_RHYTHM_MIX_LIMIT;
    }
    if (value < -GP100_RHYTHM_MIX_LIMIT) {
        return -GP100_RHYTHM_MIX_LIMIT;
    }
    return value;
}

bool gp100_rhythm_player_render(
    void *context,
    int32_t *mono_samples,
    size_t sample_count)
{
    gp100_rhythm_player_t *player =
        (gp100_rhythm_player_t *)context;
    size_t sample_index;
    size_t voice_index;

    if (player == NULL || mono_samples == NULL ||
        sample_count != GP100_RHYTHM_BLOCK_SAMPLES) {
        return false;
    }

    start_queued_voices(player);
    for (sample_index = 0u;
         sample_index < GP100_RHYTHM_BLOCK_SAMPLES;
         ++sample_index) {
        int32_t mixed = 0;

        for (voice_index = 0u;
             voice_index < GP100_RHYTHM_MAX_VOICES;
             ++voice_index) {
            const gp100_rhythm_voice_t *voice =
                &player->voices[voice_index];
            int32_t contribution =
                (int32_t)voice->block[sample_index] *
                (int32_t)(voice->trigger & RHYTHM_TRIGGER_GAIN_MASK);

            mixed = clamp_mix(mixed + contribution);
        }
        mono_samples[sample_index] = mixed;
    }

    for (voice_index = 0u;
         voice_index < GP100_RHYTHM_MAX_VOICES;
         ++voice_index) {
        gp100_rhythm_voice_t *voice = &player->voices[voice_index];

        if (voice->blocks_remaining == 0u) {
            memset(voice->block, 0, sizeof(voice->block));
        } else {
            memcpy(voice->block, voice->next_block, sizeof(voice->block));
            voice->next_block += GP100_RHYTHM_BLOCK_SAMPLES;
            --voice->blocks_remaining;
        }
    }
    return true;
}

uint32_t gp100_rhythm_player_dropped_triggers(
    const gp100_rhythm_player_t *player)
{
    return player == NULL ? 0u : player->dropped_triggers;
}
