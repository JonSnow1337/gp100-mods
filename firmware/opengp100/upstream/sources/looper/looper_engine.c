#include "looper_engine.h"

#include <limits.h>
#include <string.h>

#define LEVEL_SLEW_PER_BLOCK 4u
#define RECORDING_PREROLL_BLOCKS 2u
#define RECORDING_EXIT_BLOCKS 2u
#define RECORDING_CAPACITY_HEADROOM_BLOCKS 4u
#define PLAYBACK_ENTRY_BLOCKS 3u
#define WRAP_LOOKAHEAD_BLOCKS 2u
#define CLEAR_EXIT_BLOCKS 2u

/* The input envelope for mode changes and forward wraps counts down. */
enum factory_transition_block {
    FACTORY_TRANSITION_INACTIVE = 0,
    FACTORY_TRANSITION_RISE_LAST = 1,
    FACTORY_TRANSITION_RISE_FIRST = 2,
    FACTORY_TRANSITION_SILENT = 3,
    FACTORY_TRANSITION_FALL_LAST = 4,
    FACTORY_TRANSITION_FALL_FIRST = 5
};

static bool position_is_valid(gp100_looper_position_t position)
{
    return position == GP100_LOOPER_POST_EFFECTS ||
           position == GP100_LOOPER_PRE_EFFECTS;
}

static uint8_t slew_level(uint8_t current, uint8_t target)
{
    if (current < target) {
        unsigned next = (unsigned)current + LEVEL_SLEW_PER_BLOCK;
        return (uint8_t)(next < target ? next : target);
    }
    if (current > target) {
        unsigned difference = (unsigned)current - target;
        return difference > LEVEL_SLEW_PER_BLOCK
            ? (uint8_t)(current - LEVEL_SLEW_PER_BLOCK)
            : target;
    }
    return current;
}

static int32_t scale_sample(int32_t sample, uint8_t level)
{
    return (int32_t)(((int64_t)sample * level) / 100);
}

static int32_t scale_q8(int32_t sample, uint16_t gain)
{
    return (int32_t)(((int64_t)sample * gain) /
                     GP100_LOOPER_TRANSITION_FRAMES);
}

static bool state_is_audible(gp100_looper_state_t state)
{
    return state == GP100_LOOPER_PLAYING ||
           state == GP100_LOOPER_OVERDUBBING;
}

static int32_t saturating_add(int32_t left, int32_t right)
{
    int64_t sum = (int64_t)left + right;

    if (sum > INT32_MAX) {
        return INT32_MAX;
    }
    if (sum < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)sum;
}

static int32_t overdub_add(int32_t loop, int32_t live)
{
    int64_t sum = (int64_t)loop + live;

    if (sum > GP100_LOOPER_SAMPLE_LIMIT) {
        return GP100_LOOPER_SAMPLE_LIMIT;
    }
    if (sum < -GP100_LOOPER_SAMPLE_LIMIT) {
        return -GP100_LOOPER_SAMPLE_LIMIT;
    }
    return (int32_t)sum;
}

static void clear_loop(gp100_looper_engine_t *engine)
{
    engine->recorded_samples = 0u;
    engine->playback_position = 0u;
    engine->state = GP100_LOOPER_IDLE;
    engine->playback_gain_q8 = 0u;
    engine->playback_gain_target_q8 = 0u;
    engine->fade_out_active = false;
    engine->recording_from_factory_blocks = false;
    engine->recording_preroll_blocks = 0u;
    engine->recording_exit_blocks = 0u;
    engine->playback_entry_blocks = 0u;
    engine->wrap_transition_blocks = 0u;
    engine->playback_mode_switch_blocks = 0u;
    engine->clear_blocks = 0u;
    engine->clear_tail_frames = 0u;
    engine->mono_overdub_backup_samples = 0u;
    engine->seam_frames_remaining = 0u;
    engine->beat_extension_samples = 0u;
}

bool gp100_looper_init(
    gp100_looper_engine_t *engine,
    int32_t *storage,
    size_t storage_capacity_samples,
    gp100_looper_position_t position,
    uint8_t record_level,
    uint8_t playback_level)
{
    if (engine == NULL || storage == NULL || storage_capacity_samples == 0u ||
        !position_is_valid(position) ||
        record_level > GP100_LOOPER_MAX_LEVEL ||
        playback_level > GP100_LOOPER_MAX_LEVEL) {
        return false;
    }
    memset(engine, 0, sizeof(*engine));
    engine->storage = storage;
    engine->storage_capacity_samples = storage_capacity_samples;
    if (engine->storage_capacity_samples >
        GP100_LOOPER_FACTORY_STORAGE_SAMPLES) {
        engine->storage_capacity_samples =
            GP100_LOOPER_FACTORY_STORAGE_SAMPLES;
    }
    engine->position = position;
    engine->state = GP100_LOOPER_IDLE;
    engine->record_level = record_level;
    engine->record_level_target = record_level;
    engine->playback_level = playback_level;
    engine->playback_level_target = playback_level;
    engine->sample_rate_hz = 44100u;
    engine->beat_extension_enabled = true;
    return true;
}

bool gp100_looper_set_position(
    gp100_looper_engine_t *engine,
    gp100_looper_position_t position)
{
    if (engine == NULL || !position_is_valid(position)) {
        return false;
    }
    if (engine->position != position) {
        engine->position = position;
        clear_loop(engine);
    }
    return true;
}

bool gp100_looper_set_mono_overdub_backup(
    gp100_looper_engine_t *engine,
    int32_t *scratch,
    size_t scratch_samples)
{
    if (engine == NULL || (scratch == NULL && scratch_samples != 0u) ||
        (scratch != NULL && scratch_samples < GP100_LOOPER_MONO_BACKUP_SAMPLES)) {
        return false;
    }
    engine->mono_overdub_backup = scratch;
    engine->mono_overdub_backup_samples = 0u;
    return true;
}

bool gp100_looper_set_levels(
    gp100_looper_engine_t *engine,
    uint8_t record_level,
    uint8_t playback_level)
{
    if (engine == NULL || record_level > GP100_LOOPER_MAX_LEVEL ||
        playback_level > GP100_LOOPER_MAX_LEVEL) {
        return false;
    }
    engine->record_level_target = record_level;
    engine->playback_level_target = playback_level;
    return true;
}

bool gp100_looper_set_tempo(
    gp100_looper_engine_t *engine,
    uint32_t sample_rate_hz,
    uint16_t bpm)
{
    if (engine == NULL || sample_rate_hz == 0u ||
        sample_rate_hz > 384000u || bpm > 400u) {
        return false;
    }
    engine->sample_rate_hz = sample_rate_hz;
    engine->tempo_bpm = bpm;
    return true;
}

static void extend_recording_to_beat(gp100_looper_engine_t *engine)
{
    size_t channels;
    size_t recorded_frames;
    size_t beat_frames;
    size_t remainder;
    size_t extension_frames;
    size_t tolerance_frames;
    size_t extension_samples;

    engine->beat_extension_samples = 0u;
    if (!engine->beat_extension_enabled || engine->tempo_bpm == 0u ||
        engine->recorded_samples == 0u) {
        return;
    }
    channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    recorded_frames = engine->recorded_samples / channels;
    {
        uint64_t beat_frames_wide =
            ((uint64_t)engine->sample_rate_hz * 60u +
             engine->tempo_bpm / 2u) / engine->tempo_bpm;

        if (beat_frames_wide == 0u || beat_frames_wide > SIZE_MAX) {
            return;
        }
        beat_frames = (size_t)beat_frames_wide;
    }
    if (beat_frames == 0u) {
        return;
    }
    tolerance_frames = (size_t)((uint64_t)beat_frames * 35u / 100u);
    if (tolerance_frames < engine->sample_rate_hz / 10u) {
        tolerance_frames = engine->sample_rate_hz / 10u;
    } else if (tolerance_frames > engine->sample_rate_hz / 2u) {
        tolerance_frames = engine->sample_rate_hz / 2u;
    }
    remainder = recorded_frames % beat_frames;
    extension_frames = remainder == 0u ? 0u : beat_frames - remainder;
    if (extension_frames == 0u || extension_frames > tolerance_frames ||
        extension_frames > SIZE_MAX / channels) {
        return;
    }
    extension_samples = extension_frames * channels;
    if (extension_samples > engine->storage_capacity_samples -
                            engine->recorded_samples) {
        return;
    }
    memset(&engine->storage[engine->recorded_samples], 0,
           extension_samples * sizeof(engine->storage[0]));
    engine->recorded_samples += extension_samples;
    engine->beat_extension_samples = extension_samples;
}

static void reserve_recording_tail(gp100_looper_engine_t *engine)
{
    size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    size_t tail_samples = GP100_LOOPER_FACTORY_BLOCK_FRAMES * channels;

    if (engine->recorded_samples == 0u ||
        tail_samples > engine->storage_capacity_samples ||
        engine->recorded_samples > engine->storage_capacity_samples - tail_samples) {
        return;
    }
    /* Reserve the last callback before planning any beat padding. Until the
     * callback arrives this part of the loop can safely be read as silence. */
    memset(engine->storage + engine->recorded_samples, 0,
           tail_samples * sizeof(engine->storage[0]));
    engine->recorded_samples += tail_samples;
    engine->recording_exit_blocks = RECORDING_EXIT_BLOCKS;
}

static void begin_playback_gain(gp100_looper_engine_t *engine)
{
    engine->playback_gain_q8 = 0u;
    engine->playback_gain_target_q8 = GP100_LOOPER_TRANSITION_FRAMES;
    engine->fade_out_active = false;
}

static void request_clear(gp100_looper_engine_t *engine)
{
    size_t start = engine->playback_position;
    size_t frames = 0u;
    bool active = engine->state != GP100_LOOPER_IDLE;
    bool finish_transition = active && engine->last_callback_was_factory_block;

    if (!active && engine->recorded_samples == 0u && engine->clear_blocks != 0u) {
        return;
    }
    if (finish_transition && engine->recording_exit_blocks == 0u &&
        engine->playback_entry_blocks <= 1u && start < engine->recorded_samples &&
        (state_is_audible(engine->state) || engine->fade_out_active)) {
        size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
        frames = (engine->recorded_samples - start) / channels;
        if (frames > GP100_LOOPER_FACTORY_BLOCK_FRAMES) {
            frames = GP100_LOOPER_FACTORY_BLOCK_FRAMES;
        }
    }
    clear_loop(engine);
    if (finish_transition) {
        /* The public loop is empty while the audio path finishes its fade. */
        engine->clear_tail_start = start;
        engine->clear_tail_frames = (uint8_t)frames;
        engine->clear_blocks = CLEAR_EXIT_BLOCKS;
    }
}

bool gp100_looper_handle_event(
    gp100_looper_engine_t *engine,
    gp100_looper_event_t event,
    bool *changed)
{
    gp100_looper_state_t previous;
    gp100_looper_state_t next;
    bool had_recording;

    if (engine == NULL || event > GP100_LOOPER_CLEAR) {
        return false;
    }
    previous = engine->state;
    if (event == GP100_LOOPER_CLEAR) {
        had_recording = engine->recorded_samples != 0u;
        request_clear(engine);
        if (changed != NULL) {
            *changed = previous != engine->state || had_recording;
        }
        return true;
    }

    next = gp100_looper_next_state(previous, event);
    if (next == previous) {
        if (changed != NULL) {
            *changed = false;
        }
        return true;
    }
    if (next == GP100_LOOPER_RECORDING &&
        previous == GP100_LOOPER_IDLE) {
        engine->recorded_samples = 0u;
        engine->playback_position = 0u;
        engine->recording_from_factory_blocks = false;
        engine->recording_preroll_blocks = RECORDING_PREROLL_BLOCKS;
        engine->recording_exit_blocks = 0u;
    } else if (next == GP100_LOOPER_PLAYING &&
               (previous == GP100_LOOPER_RECORDING ||
                previous == GP100_LOOPER_STOPPED)) {
        if (previous == GP100_LOOPER_RECORDING) {
            if (engine->recording_from_factory_blocks) {
                reserve_recording_tail(engine);
            }
            engine->playback_entry_blocks =
                engine->recording_exit_blocks != 0u ? 1u : 0u;
            engine->recording_from_factory_blocks = false;
            extend_recording_to_beat(engine);
        } else {
            engine->playback_entry_blocks = PLAYBACK_ENTRY_BLOCKS;
        }
        engine->playback_position = 0u;
        if (engine->recorded_samples == 0u) {
            next = GP100_LOOPER_IDLE;
        }
    } else if (next == GP100_LOOPER_STOPPED) {
        engine->playback_entry_blocks = 0u;
        engine->fade_out_active = state_is_audible(previous) &&
                                  engine->recorded_samples != 0u &&
                                  engine->recording_exit_blocks == 0u;
    }
    if (!state_is_audible(previous) && state_is_audible(next)) {
        begin_playback_gain(engine);
    } else if (state_is_audible(previous) && !state_is_audible(next)) {
        engine->playback_gain_target_q8 = 0u;
    }
    engine->wrap_transition_blocks = 0u;
    if (state_is_audible(previous) && engine->recording_exit_blocks == 0u) {
        if (engine->playback_mode_switch_blocks < FACTORY_TRANSITION_FALL_LAST) {
            engine->playback_mode_before_switch = previous;
        }
        engine->playback_mode_switch_blocks = FACTORY_TRANSITION_FALL_FIRST;
        engine->playback_entry_blocks = 0u;
    } else {
        engine->playback_mode_switch_blocks = 0u;
    }
    engine->state = next;
    if (changed != NULL) {
        *changed = previous != next;
    }
    return true;
}

static bool append_recording_sample(
    gp100_looper_engine_t *engine,
    int32_t sample)
{
    if (engine->recorded_samples >= engine->storage_capacity_samples) {
        return false;
    }
    engine->storage[engine->recorded_samples++] = sample;
    return true;
}

static int32_t multiply_transition_q8(int32_t sample, size_t gain)
{
    int64_t product = (int64_t)sample * (int64_t)gain;

    /* ASR rounds negative products down. Widening also accepts int32 inputs
     * outside the factory's signed 24-bit work range. */
    return (int32_t)((product < 0 ? product - 255 : product) / 256);
}

static int32_t apply_transition_q8(
    int32_t sample, size_t gain, size_t preceding_gain)
{
    return multiply_transition_q8(
        multiply_transition_q8(sample, preceding_gain), gain);
}

static int32_t apply_recording_entry_fade(int32_t sample, size_t word)
{
    const size_t block_words = GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u;

    if (word < block_words) {
        return 0;
    }
    word -= block_words;
    if (word >= GP100_LOOPER_TRANSITION_FRAMES) {
        return sample;
    }

    /* Adjacent operations overlap in the factory work block. Every word
     * after a block's first receives the preceding and current Q8 gains. */
    return apply_transition_q8(sample, word, word % block_words == 0u
        ? GP100_LOOPER_TRANSITION_FRAMES : word - 1u);
}

static int32_t apply_falling_block(int32_t sample, size_t word, size_t start_gain)
{
    size_t gain = start_gain - word - 1u;

    return apply_transition_q8(sample, gain, word == 0u
        ? GP100_LOOPER_TRANSITION_FRAMES : gain + 1u);
}

static int32_t apply_block_transition(int32_t sample, size_t word, uint8_t blocks)
{
    const size_t block_words = GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u;

    if (blocks == FACTORY_TRANSITION_INACTIVE) {
        return sample;
    }
    if (blocks > FACTORY_TRANSITION_SILENT) {
        size_t start_gain = (size_t)(blocks - FACTORY_TRANSITION_SILENT) * block_words;

        return apply_falling_block(sample, word, start_gain);
    }
    return apply_recording_entry_fade(
        sample, (size_t)(FACTORY_TRANSITION_SILENT - blocks) * block_words + word);
}

static void record_falling_block(
    gp100_looper_engine_t *engine, const int32_t *interleaved_stereo)
{
    size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    size_t tail_samples = GP100_LOOPER_FACTORY_BLOCK_FRAMES * channels;
    int32_t *output = engine->storage + engine->recorded_samples -
                      engine->beat_extension_samples - tail_samples;
    size_t frame;

    for (frame = 0u; frame < GP100_LOOPER_FACTORY_BLOCK_FRAMES; ++frame) {
        size_t channel;

        for (channel = 0u; channel < channels; ++channel) {
            size_t word = frame * 2u + channel;
            int32_t sample =
                scale_sample(interleaved_stereo[word], engine->record_level);

            *output++ = apply_falling_block(
                sample, word, GP100_LOOPER_TRANSITION_FRAMES);
        }
    }
}

static int32_t next_playback_sample(
    gp100_looper_engine_t *engine,
    bool factory_block,
    bool *wrapped)
{
    int32_t sample;

    if (engine->recorded_samples == 0u) {
        return 0;
    }
    if (engine->playback_position >= engine->recorded_samples) {
        engine->playback_position = 0u;
        *wrapped = true;
    }
    sample = engine->storage[engine->playback_position++];
    if (engine->playback_position >= engine->recorded_samples) {
        if (!factory_block) {
            engine->playback_position = 0u;
        }
        *wrapped = true;
    }
    return sample;
}

static void slew_playback_gain(
    gp100_looper_engine_t *engine, size_t frames)
{
    uint16_t current = engine->playback_gain_q8;
    uint16_t target = engine->playback_gain_target_q8;

    if (current < target) {
        size_t distance = (size_t)target - current;
        engine->playback_gain_q8 = frames < distance
            ? (uint16_t)(current + frames) : target;
    } else if (current > target) {
        size_t distance = (size_t)current - target;
        engine->playback_gain_q8 = frames < distance
            ? (uint16_t)(current - frames) : target;
    }
}

static void apply_factory_wet_transition(
    gp100_looper_engine_t *engine,
    size_t frame,
    bool switch_fall,
    int32_t *left,
    int32_t *right)
{
    const size_t unity = GP100_LOOPER_TRANSITION_FRAMES;
    size_t gain = engine->playback_gain_q8;

    if (switch_fall) {
        *left = apply_falling_block(*left, frame * 2u, unity);
        *right = apply_falling_block(*right, frame * 2u + 1u, unity);
    } else if (engine->fade_out_active) {
        size_t elapsed = frame * 2u;

        if (elapsed >= gain) {
            *left = 0;
            *right = 0;
            return;
        }
        gain -= elapsed + 1u;
        *left = apply_transition_q8(*left, gain, frame == 0u ? unity : gain + 1u);
        *right = apply_transition_q8(*right, gain == 0u ? 0u : gain - 1u, gain);
    } else if (gain < engine->playback_gain_target_q8) {
        *left = apply_transition_q8(*left, gain, frame == 0u ? unity : gain - 1u);
        *right = apply_transition_q8(*right, gain + 1u, gain);
        slew_playback_gain(engine, 2u);
    } else if (gain == 0u) {
        *left = 0;
        *right = 0;
    }
}

static void finish_playback_fade_out(gp100_looper_engine_t *engine)
{
    engine->playback_gain_q8 = 0u;
    engine->fade_out_active = false;
    engine->playback_position = 0u;
    engine->seam_frames_remaining = 0u;
}

static void mix_switch_settle_block(
    const gp100_looper_engine_t *engine,
    int32_t *interleaved_stereo)
{
    const size_t half_gain = GP100_LOOPER_TRANSITION_FRAMES / 2u;
    size_t word;

    for (word = 0u; word < GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u; ++word) {
        int32_t input = interleaved_stereo[word];
        int32_t wet = scale_sample(input, engine->record_level);

        /* The processor holds the read head while the work input passes
         * through the remaining halves of both transition envelopes. */
        wet = apply_falling_block(wet, word, half_gain);
        wet = apply_falling_block(wet, word, half_gain);
        interleaved_stereo[word] = saturating_add(
            input, scale_sample(wet, engine->playback_level));
    }
}

static void mix_clear_tail(
    gp100_looper_engine_t *engine,
    int32_t *interleaved_stereo)
{
    size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    size_t start = engine->clear_tail_start;
    size_t frame;

    for (frame = 0u; frame < engine->clear_tail_frames; ++frame) {
        size_t word = frame * 2u;
        int32_t left = engine->storage[start + frame * channels];
        int32_t right = engine->storage[start + frame * channels + channels - 1u];

        apply_factory_wet_transition(engine, frame, true, &left, &right);
        interleaved_stereo[word] = saturating_add(
            interleaved_stereo[word], scale_sample(left, engine->playback_level));
        interleaved_stereo[word + 1u] = saturating_add(
            interleaved_stereo[word + 1u], scale_sample(right, engine->playback_level));
    }
}

static int32_t bridge_seam(int32_t from, int32_t current, uint16_t remaining)
{
    uint32_t current_weight = GP100_LOOPER_SEAM_FRAMES - remaining;

    return (int32_t)(((int64_t)from * remaining +
                      (int64_t)current * current_weight) /
                     GP100_LOOPER_SEAM_FRAMES);
}

static void finish_factory_playback_block(
    gp100_looper_engine_t *engine,
    uint8_t input_transition_blocks,
    bool stays_within_recording,
    bool *wrapped)
{
    size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    size_t remaining = engine->recorded_samples - engine->playback_position;
    size_t lookahead = WRAP_LOOKAHEAD_BLOCKS *
                      GP100_LOOPER_FACTORY_BLOCK_FRAMES * channels;

    if (engine->wrap_transition_blocks != 0u) {
        --engine->wrap_transition_blocks;
    }
    if (input_transition_blocks <= FACTORY_TRANSITION_RISE_LAST &&
        stays_within_recording && remaining <= lookahead) {
        /* The helper arms the next input fade before testing an exact end.
         * In that case the read head reaches zero on the following callback. */
        engine->wrap_transition_blocks = FACTORY_TRANSITION_FALL_FIRST;
        *wrapped = false;
    } else if (*wrapped) {
        engine->playback_position = 0u;
    }
}

static void save_mono_overdub_block(gp100_looper_engine_t *engine)
{
    size_t saved = engine->mono_overdub_backup_samples;
    size_t start = engine->playback_position;
    size_t count;
    int32_t *backup;

    if (engine->mono_overdub_backup == NULL ||
        engine->position != GP100_LOOPER_PRE_EFFECTS ||
        saved >= GP100_LOOPER_MONO_BACKUP_SAMPLES ||
        start > engine->recorded_samples || engine->recorded_samples == 0u) {
        return;
    }
    if (saved == 0u) {
        engine->mono_overdub_backup_start = start;
    }
    count = engine->recorded_samples - start;
    if (count > GP100_LOOPER_FACTORY_BLOCK_FRAMES) {
        count = GP100_LOOPER_FACTORY_BLOCK_FRAMES;
    }
    backup = engine->mono_overdub_backup + saved;
    memcpy(backup, engine->storage + start, count * sizeof(*backup));
    /* The firmware reserves a whole callback even at a partial tail. */
    memset(backup + count, 0,
           (GP100_LOOPER_FACTORY_BLOCK_FRAMES - count) * sizeof(*backup));
    engine->mono_overdub_backup_samples = saved + GP100_LOOPER_FACTORY_BLOCK_FRAMES;
}

static void finish_mono_overdub(gp100_looper_engine_t *engine)
{
    size_t count = engine->mono_overdub_backup_samples;
    size_t start = engine->mono_overdub_backup_start;
    size_t length = engine->recorded_samples;

    if (engine->state == GP100_LOOPER_STOPPED && count != 0u &&
        count < GP100_LOOPER_MONO_BACKUP_SAMPLES && start <= length) {
        size_t tail = length - start;
        size_t first = count < tail ? count : tail;
        size_t restart = tail + 1u;
        size_t end = count < length ? count : length;

        memcpy(engine->storage + start, engine->mono_overdub_backup,
               first * sizeof(*engine->storage));
        /* At the boundary the factory resets the destination base and keeps
         * incrementing its backup index. Only restore playable samples. */
        if (restart < end) {
            memcpy(engine->storage + restart, engine->mono_overdub_backup + restart,
                   (end - restart) * sizeof(*engine->storage));
        }
    }
    engine->mono_overdub_backup_samples = 0u;
}

static bool process_frames(
    gp100_looper_engine_t *engine,
    int32_t *interleaved_stereo,
    size_t frame_count,
    bool factory_block,
    gp100_looper_process_result_t *result)
{
    size_t frame;
    gp100_looper_state_t playback_state;
    uint8_t switch_blocks = 0u;
    uint8_t record_transition_blocks;
    bool mute_playback;
    bool past_recording;

    if (engine == NULL || interleaved_stereo == NULL || result == NULL ||
        engine->storage == NULL || !position_is_valid(engine->position) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    engine->record_level = slew_level(
        engine->record_level, engine->record_level_target);
    engine->playback_level = slew_level(
        engine->playback_level, engine->playback_level_target);
    engine->last_callback_was_factory_block = factory_block;

    if (factory_block && engine->clear_blocks != 0u) {
        if (engine->clear_blocks == CLEAR_EXIT_BLOCKS) {
            mix_clear_tail(engine, interleaved_stereo);
        } else {
            mix_switch_settle_block(engine, interleaved_stereo);
        }
        --engine->clear_blocks;
        result->frames_processed = frame_count;
        return true;
    }
    engine->clear_blocks = 0u;

    if (engine->state == GP100_LOOPER_RECORDING) {
        engine->recording_from_factory_blocks = factory_block;
    }
    if (factory_block && engine->state == GP100_LOOPER_RECORDING &&
        engine->recording_preroll_blocks != 0u) {
        --engine->recording_preroll_blocks;
        result->frames_processed = frame_count;
        return true;
    }
    if (factory_block && engine->recording_exit_blocks != 0u) {
        if (engine->recording_exit_blocks == RECORDING_EXIT_BLOCKS) {
            record_falling_block(engine, interleaved_stereo);
        } else {
            mix_switch_settle_block(engine, interleaved_stereo);
        }
        --engine->recording_exit_blocks;
        result->frames_processed = frame_count;
        return true;
    }
    if (!factory_block) {
        engine->recording_exit_blocks = 0u;
        engine->playback_entry_blocks = 0u;
        engine->wrap_transition_blocks = 0u;
        engine->playback_mode_switch_blocks = 0u;
        engine->mono_overdub_backup_samples = 0u;
    } else {
        engine->seam_frames_remaining = 0u;
        switch_blocks = engine->playback_mode_switch_blocks;
        if (switch_blocks != 0u) {
            --engine->playback_mode_switch_blocks;
        }
    }
    if (switch_blocks == FACTORY_TRANSITION_FALL_LAST) {
        if (engine->playback_mode_before_switch == GP100_LOOPER_OVERDUBBING) {
            finish_mono_overdub(engine);
        }
        mix_switch_settle_block(engine, interleaved_stereo);
        if (state_is_audible(engine->state)) {
            begin_playback_gain(engine);
            engine->playback_entry_blocks = 1u;
        } else {
            finish_playback_fade_out(engine);
            engine->playback_mode_switch_blocks = 0u;
        }
        result->frames_processed = frame_count;
        return true;
    }
    if (engine->playback_entry_blocks > 1u) {
        if (engine->playback_entry_blocks == 2u) {
            mix_switch_settle_block(engine, interleaved_stereo);
        }
        --engine->playback_entry_blocks;
        result->frames_processed = frame_count;
        return true;
    }
    /* The final entry callback consumes the first wet block while muted.
     * The two-block Q8 rise starts on the next callback. */
    mute_playback = engine->playback_entry_blocks == 1u;
    engine->playback_entry_blocks = 0u;
    playback_state = switch_blocks == FACTORY_TRANSITION_FALL_FIRST
        ? engine->playback_mode_before_switch : engine->state;
    if (factory_block && mute_playback && switch_blocks == 0u) {
        engine->wrap_transition_blocks = FACTORY_TRANSITION_SILENT;
    }
    record_transition_blocks = switch_blocks != 0u
        ? switch_blocks : engine->wrap_transition_blocks;
    past_recording = factory_block && engine->recorded_samples != 0u &&
                     engine->playback_position == engine->recorded_samples &&
                     (state_is_audible(playback_state) || engine->fade_out_active);
    if (factory_block && playback_state == GP100_LOOPER_OVERDUBBING) {
        save_mono_overdub_block(engine);
    }

    for (frame = 0u; !past_recording && frame < frame_count; ++frame) {
        int32_t input_left = interleaved_stereo[frame * 2u];
        int32_t input_right = interleaved_stereo[frame * 2u + 1u];

        if (engine->state == GP100_LOOPER_RECORDING) {
            bool mono = engine->position == GP100_LOOPER_PRE_EFFECTS;
            int32_t record_left = scale_sample(input_left, engine->record_level);
            int32_t record_right = scale_sample(input_right, engine->record_level);
            bool stored;

            if (factory_block) {
                size_t word = mono ? engine->recorded_samples * 2u
                                   : engine->recorded_samples;
                record_left = apply_recording_entry_fade(record_left, word);
                record_right = apply_recording_entry_fade(record_right, word + 1u);
            }
            if (mono) {
                stored = append_recording_sample(engine, record_left);
            } else {
                stored = engine->recorded_samples + 1u <
                             engine->storage_capacity_samples &&
                         append_recording_sample(engine, record_left) &&
                         append_recording_sample(engine, record_right);
            }
            if (!stored) {
                engine->state = engine->recorded_samples == 0u
                    ? GP100_LOOPER_IDLE
                    : GP100_LOOPER_PLAYING;
                playback_state = engine->state;
                engine->playback_position = 0u;
                if (engine->state == GP100_LOOPER_PLAYING) {
                    begin_playback_gain(engine);
                }
                result->capacity_reached = true;
                result->state_changed = true;
                continue;
            }
        }

        if (state_is_audible(playback_state) || engine->fade_out_active) {
            int32_t loop_left;
            int32_t loop_right;
            size_t stored_left_position;
            size_t stored_right_position;
            bool wrapped = false;
            size_t channels = engine->position ==
                    GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
            size_t recorded_frames = engine->recorded_samples / channels;

            if (engine->playback_position >= engine->recorded_samples) {
                engine->playback_position = 0u;
            }
            stored_left_position = engine->playback_position;
            loop_left = next_playback_sample(engine, factory_block, &wrapped);
            if (engine->position == GP100_LOOPER_PRE_EFFECTS) {
                loop_right = loop_left;
                stored_right_position = stored_left_position;
            } else {
                stored_right_position = engine->playback_position;
                loop_right = next_playback_sample(
                    engine, factory_block, &wrapped);
            }

            if (playback_state == GP100_LOOPER_OVERDUBBING) {
                int32_t record_left = scale_sample(
                    input_left, engine->record_level);
                int32_t record_right = scale_sample(
                    input_right, engine->record_level);

                if (factory_block) {
                    record_left = apply_block_transition(
                        record_left, frame * 2u, record_transition_blocks);
                    record_right = apply_block_transition(
                        record_right, frame * 2u + 1u, record_transition_blocks);
                }
                engine->storage[stored_left_position] =
                    overdub_add(loop_left, record_left);
                if (engine->position == GP100_LOOPER_POST_EFFECTS) {
                    engine->storage[stored_right_position] =
                        overdub_add(loop_right, record_right);
                }
            }

            if (!factory_block && engine->seam_frames_remaining != 0u) {
                loop_left = bridge_seam(
                    engine->seam_from_left, loop_left,
                    engine->seam_frames_remaining);
                loop_right = bridge_seam(
                    engine->seam_from_right, loop_right,
                    engine->seam_frames_remaining);
                --engine->seam_frames_remaining;
                result->seam_smoothed = true;
            }
            if (wrapped) {
                result->loop_wrapped = true;
                if (!factory_block &&
                    recorded_frames > GP100_LOOPER_SEAM_FRAMES * 2u) {
                    engine->seam_from_left = loop_left;
                    engine->seam_from_right = loop_right;
                    engine->seam_frames_remaining =
                        GP100_LOOPER_SEAM_FRAMES;
                }
            }

            if (mute_playback) {
                loop_left = 0;
                loop_right = 0;
            } else if (factory_block) {
                apply_factory_wet_transition(
                    engine, frame, switch_blocks == FACTORY_TRANSITION_FALL_FIRST,
                    &loop_left, &loop_right);
                loop_left = scale_sample(loop_left, engine->playback_level);
                loop_right = scale_sample(loop_right, engine->playback_level);
            } else {
                slew_playback_gain(engine, 1u);
                loop_left = scale_q8(
                    scale_sample(loop_left, engine->playback_level),
                    engine->playback_gain_q8);
                loop_right = scale_q8(
                    scale_sample(loop_right, engine->playback_level),
                    engine->playback_gain_q8);
            }
            interleaved_stereo[frame * 2u] = saturating_add(
                input_left, loop_left);
            interleaved_stereo[frame * 2u + 1u] = saturating_add(
                input_right, loop_right);
            if (!factory_block && engine->fade_out_active &&
                engine->playback_gain_q8 == 0u) {
                finish_playback_fade_out(engine);
            }
            if (wrapped && factory_block) {
                /* The remaining frames already contain the live signal. */
                if (!mute_playback && !engine->fade_out_active &&
                    switch_blocks != FACTORY_TRANSITION_FALL_FIRST) {
                    slew_playback_gain(engine, (frame_count - frame - 1u) * 2u);
                }
                break;
            }
        }
    }
    if (past_recording) {
        result->loop_wrapped = true;
        if (!mute_playback && !engine->fade_out_active &&
            switch_blocks != FACTORY_TRANSITION_FALL_FIRST) {
            slew_playback_gain(engine, frame_count * 2u);
        }
    }
    if (factory_block && engine->recorded_samples != 0u &&
        (state_is_audible(playback_state) || engine->fade_out_active)) {
        bool stays_within_recording = !past_recording &&
            (!result->loop_wrapped || frame + 1u == frame_count);

        finish_factory_playback_block(engine, record_transition_blocks,
                                      stays_within_recording, &result->loop_wrapped);
    }
    if (factory_block && engine->fade_out_active && switch_blocks == 0u) {
        finish_playback_fade_out(engine);
    }
    if (factory_block && engine->state == GP100_LOOPER_RECORDING &&
        engine->storage_capacity_samples == GP100_LOOPER_FACTORY_STORAGE_SAMPLES) {
        size_t channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
        size_t headroom = RECORDING_CAPACITY_HEADROOM_BLOCKS * frame_count * channels;

        if (engine->recorded_samples >= engine->storage_capacity_samples - headroom) {
            /* The factory stops on a block boundary with space for the final fade. */
            reserve_recording_tail(engine);
            engine->recording_from_factory_blocks = false;
            engine->state = GP100_LOOPER_PLAYING;
            engine->playback_position = 0u;
            engine->playback_entry_blocks = 1u;
            begin_playback_gain(engine);
            result->capacity_reached = true;
            result->state_changed = true;
        }
    }
    result->frames_processed = frame_count;
    return true;
}

bool gp100_looper_process(
    gp100_looper_engine_t *engine,
    int32_t *interleaved_stereo,
    size_t frame_count,
    gp100_looper_process_result_t *result)
{
    return process_frames(engine, interleaved_stereo, frame_count, false, result);
}

bool gp100_looper_process_factory_block(
    gp100_looper_engine_t *engine,
    int32_t interleaved_stereo[GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u],
    gp100_looper_process_result_t *result)
{
    return process_frames(
        engine, interleaved_stereo, GP100_LOOPER_FACTORY_BLOCK_FRAMES,
        true, result);
}

double gp100_looper_capacity_seconds(
    const gp100_looper_engine_t *engine,
    uint32_t sample_rate_hz)
{
    size_t channels;

    if (engine == NULL || sample_rate_hz == 0u ||
        !position_is_valid(engine->position)) {
        return 0.0;
    }
    channels = engine->position == GP100_LOOPER_PRE_EFFECTS ? 1u : 2u;
    return (double)engine->storage_capacity_samples /
           ((double)sample_rate_hz * (double)channels);
}
