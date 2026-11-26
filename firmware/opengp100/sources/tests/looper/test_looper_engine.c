#include "looper/looper_engine.h"
#include "app/memory_layout.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int test_post_record_play_and_clear(void)
{
    int32_t storage[8] = {0};
    int32_t record_block[4] = {100, -200, 300, -400};
    const int32_t expected_recording[4] = {99, -198, 297, -396};
    int32_t playback_block[6] = {1, 2, 3, 4, 5, 6};
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    bool changed;

    if (!gp100_looper_init(
            &engine, storage, 8u, GP100_LOOPER_POST_EFFECTS,
            99u, 99u) ||
        !gp100_looper_handle_event(
            &engine, GP100_LOOPER_RECORD_OR_OVERDUB, &changed) ||
        !changed || engine.state != GP100_LOOPER_RECORDING ||
        !gp100_looper_process(
            &engine, record_block, 2u, &result) ||
        engine.recorded_samples != 4u ||
        memcmp(storage, expected_recording, sizeof(expected_recording)) != 0) {
        return 1;
    }
    if (!gp100_looper_handle_event(
            &engine, GP100_LOOPER_PLAY, &changed) ||
        engine.state != GP100_LOOPER_PLAYING ||
        !gp100_looper_process(
            &engine, playback_block, 3u, &result) ||
        playback_block[0] != 1 || playback_block[1] != 2 ||
        playback_block[2] != 5 || playback_block[3] != 1 ||
        playback_block[4] != 6 || playback_block[5] != 4 ||
        !result.loop_wrapped) {
        return 2;
    }
    if (!gp100_looper_handle_event(
            &engine, GP100_LOOPER_CLEAR, &changed) ||
        !changed || engine.state != GP100_LOOPER_IDLE ||
        engine.recorded_samples != 0u) {
        return 3;
    }
    return 0;
}

static int test_pre_mono_and_capacity(void)
{
    int32_t storage[2] = {0};
    int32_t input[6] = {10, 100, 20, 200, 30, 300};
    int32_t playback[4] = {0, 1, 2, 3};
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;

    if (!gp100_looper_init(
            &engine, storage, 2u, GP100_LOOPER_PRE_EFFECTS,
            99u, 99u) ||
        !gp100_looper_handle_event(
            &engine, GP100_LOOPER_RECORD_OR_OVERDUB, NULL) ||
        !gp100_looper_process(&engine, input, 3u, &result) ||
        storage[0] != 9 || storage[1] != 19 ||
        engine.recorded_samples != 2u ||
        engine.state != GP100_LOOPER_PLAYING ||
        !result.capacity_reached || !result.state_changed) {
        return 1;
    }
    if (!gp100_looper_process(&engine, playback, 2u, &result) ||
        playback[0] != 0 || playback[1] != 1 ||
        playback[2] != 2 || playback[3] != 3 ||
        !result.loop_wrapped) {
        return 2;
    }
    return 0;
}

static int test_overdub_stop_levels_and_position(void)
{
    int32_t storage[4] = {
        GP100_LOOPER_SAMPLE_LIMIT - 1,
        -GP100_LOOPER_SAMPLE_LIMIT + 1,
        0,
        0
    };
    int32_t block[2] = {100, -100};
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    bool changed;

    if (!gp100_looper_init(
            &engine, storage, 4u, GP100_LOOPER_POST_EFFECTS,
            99u, 99u)) {
        return 1;
    }
    engine.recorded_samples = 2u;
    engine.state = GP100_LOOPER_PLAYING;
    engine.playback_gain_q8 = 256u;
    engine.playback_gain_target_q8 = 256u;
    if (!gp100_looper_handle_event(
            &engine, GP100_LOOPER_RECORD_OR_OVERDUB, &changed) ||
        engine.state != GP100_LOOPER_OVERDUBBING ||
        !gp100_looper_process(&engine, block, 1u, &result) ||
        storage[0] != GP100_LOOPER_SAMPLE_LIMIT ||
        storage[1] != -GP100_LOOPER_SAMPLE_LIMIT ||
        block[0] != 8304714 || block[1] != -8304714) {
        return 2;
    }
    engine.state = GP100_LOOPER_PLAYING;
    if (!gp100_looper_handle_event(
            &engine, GP100_LOOPER_STOP, NULL) ||
        engine.state != GP100_LOOPER_STOPPED ||
        !gp100_looper_set_levels(&engine, 0u, 0u)) {
        return 3;
    }
    block[0] = 0;
    block[1] = 0;
    engine.state = GP100_LOOPER_PLAYING;
    if (!gp100_looper_process(&engine, block, 1u, &result) ||
        engine.record_level != 95u || engine.playback_level != 95u ||
        !gp100_looper_set_position(
            &engine, GP100_LOOPER_PRE_EFFECTS) ||
        engine.state != GP100_LOOPER_IDLE ||
        engine.recorded_samples != 0u) {
        return 4;
    }
    return 0;
}

static int test_transition_seam_and_beat_extension(void)
{
    int32_t storage[320];
    int32_t block[160];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t index;

    memset(storage, 0, sizeof(storage));
    for (index = 0u; index < 80u; ++index) {
        block[index * 2u] = 1000;
        block[index * 2u + 1u] = -1000;
    }
    if (!gp100_looper_init(
            &engine, storage, 320u, GP100_LOOPER_PRE_EFFECTS,
            99u, 99u) ||
        !gp100_looper_set_tempo(&engine, 100u, 60u) ||
        !gp100_looper_handle_event(
            &engine, GP100_LOOPER_RECORD_OR_OVERDUB, NULL) ||
        !gp100_looper_process(&engine, block, 80u, &result) ||
        !gp100_looper_handle_event(
            &engine, GP100_LOOPER_PLAY, NULL) ||
        engine.recorded_samples != 100u ||
        engine.beat_extension_samples != 20u || storage[99] != 0) {
        return 1;
    }

    for (index = 0u; index < 300u; ++index) {
        storage[index] = (int32_t)index;
    }
    engine.tempo_bpm = 0u;
    engine.recorded_samples = 300u;
    engine.playback_position = 299u;
    engine.state = GP100_LOOPER_PLAYING;
    engine.playback_gain_q8 = 256u;
    engine.playback_gain_target_q8 = 256u;
    engine.seam_frames_remaining = 0u;
    block[0] = 0;
    block[1] = 0;
    block[2] = 0;
    block[3] = 0;
    if (!gp100_looper_process(&engine, block, 2u, &result) ||
        block[0] != 296 || block[1] != 296 ||
        block[2] != 296 || block[3] != 296 ||
        !result.loop_wrapped || !result.seam_smoothed ||
        engine.seam_frames_remaining != 127u ||
        !gp100_looper_handle_event(
            &engine, GP100_LOOPER_STOP, NULL) ||
        !engine.fade_out_active || engine.state != GP100_LOOPER_STOPPED) {
        return 2;
    }
    block[0] = 0;
    block[1] = 0;
    if (!gp100_looper_process(&engine, block, 1u, &result) ||
        engine.playback_gain_q8 != 255u || block[0] == 0 ||
        block[0] != block[1]) {
        return 3;
    }
    if (gp100_looper_set_tempo(NULL, 44100u, 120u) ||
        gp100_looper_set_tempo(&engine, 0u, 120u) ||
        gp100_looper_set_tempo(&engine, 384001u, 120u) ||
        gp100_looper_set_tempo(&engine, 44100u, 401u)) {
        return 4;
    }
    return 0;
}

static int test_factory_capacity(void)
{
    int32_t dummy;
    gp100_looper_engine_t engine;

    if (!gp100_memory_region_contains(
            &gp100_sdram_regions[GP100_SDRAM_LOOPER_INDEX],
            GP100_LOOPER_FACTORY_STORAGE_ADDRESS,
            (size_t)GP100_LOOPER_FACTORY_STORAGE_SAMPLES *
                sizeof(int32_t)) ||
        !gp100_looper_init(
            &engine, &dummy,
            GP100_LOOPER_FACTORY_STORAGE_SAMPLES,
            GP100_LOOPER_PRE_EFFECTS, 99u, 99u) ||
        gp100_looper_capacity_seconds(&engine, 44100u) != 91.0 ||
        !gp100_looper_set_position(
            &engine, GP100_LOOPER_POST_EFFECTS) ||
        gp100_looper_capacity_seconds(&engine, 44100u) != 45.5) {
        return 1;
    }
    return 0;
}

static int test_factory_block_wrap(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES, LOOP_FRAMES = 94 };
    int32_t storage[LOOP_FRAMES * 2u];
    int32_t block[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t channels;

    for (channels = 1u; channels <= 2u; ++channels) {
        size_t frame;
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;

        if (!gp100_looper_init(
                &engine, storage, LOOP_FRAMES * channels, position, 99u, 99u)) {
            return 1;
        }
        for (frame = 0u; frame < LOOP_FRAMES; ++frame) {
            storage[frame * channels] = (int32_t)(100u + frame * 100u);
            if (channels == 2u) {
                storage[frame * channels + 1u] = -storage[frame * channels];
            }
        }
        engine.recorded_samples = LOOP_FRAMES * channels;
        engine.playback_position = FRAMES * channels;
        engine.state = GP100_LOOPER_PLAYING;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        engine.playback_gain_target_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        for (frame = 0u; frame < FRAMES; ++frame) {
            block[frame * 2u] = 7;
            block[frame * 2u + 1u] = -9;
        }
        if (!gp100_looper_process_factory_block(&engine, block, &result) ||
            !result.loop_wrapped || engine.playback_position != 0u ||
            result.frames_processed != FRAMES) {
            return 2;
        }
        for (frame = 0u; frame < FRAMES; ++frame) {
            int32_t wet = frame < LOOP_FRAMES - FRAMES
                ? (int32_t)((FRAMES + frame + 1u) * 99u) : 0;

            if (block[frame * 2u] != 7 + wet ||
                block[frame * 2u + 1u] != -9 + (channels == 2u ? -wet : wet)) {
                return 3;
            }
        }
        memset(block, 0, sizeof(block));
        if (!gp100_looper_process_factory_block(&engine, block, &result) ||
            result.loop_wrapped || engine.playback_position != FRAMES * channels ||
            block[0] != 99 || block[1] != (channels == 2u ? -99 : 99)) {
            return 4;
        }
        engine.playback_gain_q8 = 0u;
        if (!gp100_looper_process_factory_block(&engine, block, &result) ||
            !result.loop_wrapped || engine.playback_gain_q8 != FRAMES * 2u) {
            return 5;
        }
        engine.state = GP100_LOOPER_OVERDUBBING;
        engine.playback_position = FRAMES * channels;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        engine.wrap_transition_blocks = 0u;
        for (frame = 0u; frame < FRAMES; ++frame) {
            block[frame * 2u] = 100;
            block[frame * 2u + 1u] = -200;
        }
        if (!gp100_looper_process_factory_block(&engine, block, &result) ||
            !result.loop_wrapped || engine.playback_position != 0u ||
            block[0] != 100 + (int32_t)((FRAMES + 1u) * 99u) ||
            block[FRAMES * 2u - 2u] != 100 ||
            storage[0] != 100 ||
            storage[(LOOP_FRAMES - 1u) * channels] != LOOP_FRAMES * 100 + 99 ||
            (channels == 2u && storage[LOOP_FRAMES * channels - 1u] !=
                -(LOOP_FRAMES * 100 + 198))) {
            return 6;
        }
    }
    return 0;
}

static int test_factory_short_aligned_wrap(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    static const size_t heads[] = {
        0u, 0u, 0u, 0u, 0u, 0u, 64u, 128u, 192u, 0u, 64u, 128u, 0u
    };
    int32_t storage[4u * FRAMES * 2u];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t channels;

    for (channels = 1u; channels <= 2u; ++channels) {
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;
        size_t callback;
        size_t index;

        for (index = 0u; index < sizeof(storage) / sizeof(storage[0]); ++index) {
            storage[index] = 12345;
        }
        if (!gp100_looper_init(&engine, storage, 4u * FRAMES * channels,
                position, 99u, 99u) ||
            !gp100_looper_handle_event(
                &engine, GP100_LOOPER_RECORD_OR_OVERDUB, NULL)) {
            return 1;
        }
        for (callback = 0u; callback < sizeof(heads) / sizeof(heads[0]); ++callback) {
            size_t frame;

            if (callback == 4u && !gp100_looper_handle_event(
                    &engine, GP100_LOOPER_PLAY, NULL)) {
                return 2;
            }
            for (frame = 0u; frame < FRAMES; ++frame) {
                audio[frame * 2u] = 30100;
                audio[frame * 2u + 1u] = -40200;
            }
            if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
                engine.playback_position != heads[callback] * channels ||
                result.loop_wrapped != (callback == 9u || callback == 12u)) {
                return 3;
            }
            if (callback == 8u &&
                (audio[0] != 30100 + 14692 ||
                 audio[1] != -40200 + (channels == 1u ? 7403 : -9811))) {
                return 4;
            }
            if (callback == 9u) {
                for (frame = 0u; frame < FRAMES; ++frame) {
                    if (audio[frame * 2u] != 30100 || audio[frame * 2u + 1u] != -40200) {
                        return 5;
                    }
                }
            }
        }
        if (engine.recorded_samples != 192u * channels ||
            storage[engine.recorded_samples] != 12345) {
            return 6;
        }
    }
    return 0;
}

static int test_factory_recording_fades(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    static const int32_t first_rise[] = {0, 0, 0, -4, 5, -13, 13, -26};
    static const int32_t second_rise[] = {
        14899, -10028, 7624, -10342, 7862, -10662, 8103, -10986
    };
    static const int32_t falling[] = {
        29682, -39334, 29219, -38718, 28760, -38107, 28304, -37501
    };
    int32_t storage[FRAMES * 6u * 2u];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t channels;
    size_t frame;

    for (channels = 1u; channels <= 2u; ++channels) {
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;
        size_t padding_frames = channels == 1u ? 30u : 0u;
        size_t callback;
        size_t i;

        for (frame = 0u; frame < FRAMES; ++frame) {
            audio[frame * 2u] = 30100;
            audio[frame * 2u + 1u] = -40200;
        }
        if (!gp100_looper_init(
                &engine, storage, sizeof(storage) / sizeof(storage[0]),
                position, 99u, 99u) ||
            !gp100_looper_handle_event(
                &engine, GP100_LOOPER_RECORD_OR_OVERDUB, NULL)) {
            return 1;
        }
        for (callback = 0u; callback < 6u; ++callback) {
            size_t expected_frames = callback < 2u ? 0u : (callback - 1u) * FRAMES;

            if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
                result.frames_processed != FRAMES ||
                engine.recorded_samples != expected_frames * channels) {
                return 2;
            }
        }
        for (i = 0u; i < FRAMES * channels; ++i) {
            if (storage[i] != 0) {
                return 3;
            }
        }
        for (i = 0u; i < 4u * channels; ++i) {
            size_t word = channels == 1u ? i * 2u : i;
            if (storage[FRAMES * channels + i] != first_rise[word] ||
                storage[2u * FRAMES * channels + i] != second_rise[word] ||
                storage[3u * FRAMES * channels + i] !=
                    (word % 2u == 0u ? 29799 : -39798)) {
                return 4;
            }
        }
        engine.beat_extension_enabled = padding_frames != 0u;
        if (!gp100_looper_set_tempo(&engine, 350u, 60u) ||
            !gp100_looper_handle_event(
                &engine, GP100_LOOPER_PLAY, NULL) ||
            engine.state != GP100_LOOPER_PLAYING ||
            engine.recorded_samples != (5u * FRAMES + padding_frames) * channels ||
            engine.beat_extension_samples != padding_frames * channels ||
            !gp100_looper_process_factory_block(&engine, audio, &result) ||
            engine.playback_position != 0u) {
            return 5;
        }
        for (i = 0u; i < 4u * channels; ++i) {
            size_t word = channels == 1u ? i * 2u : i;
            if (storage[4u * FRAMES * channels + i] != falling[word]) {
                return 6;
            }
        }
        for (i = 5u * FRAMES * channels; i < engine.recorded_samples; ++i) {
            if (storage[i] != 0) {
                return 7;
            }
        }
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            engine.playback_position != 0u || audio[0] != 37359 || audio[1] != -42550) {
            return 8;
        }
        for (frame = 0u; frame < FRAMES; ++frame) {
            audio[frame * 2u] = 30100;
            audio[frame * 2u + 1u] = -40200;
        }
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            engine.playback_position != FRAMES * channels) {
            return 8;
        }
        for (frame = 0u; frame < FRAMES; ++frame) {
            if (audio[frame * 2u] != 30100 || audio[frame * 2u + 1u] != -40200) {
                return 9;
            }
        }
        engine.playback_position = 5u * FRAMES * channels - channels;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        memset(audio, 0, sizeof(audio));
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            !result.loop_wrapped || result.seam_smoothed || audio[0] == 0) {
            return 10;
        }
        memset(audio, 0, sizeof(audio));
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            result.seam_smoothed || engine.seam_frames_remaining != 0u ||
            engine.playback_position != FRAMES * channels || audio[0] != 0) {
            return 11;
        }
    }
    if (!gp100_looper_init(
            &engine, storage, sizeof(storage) / sizeof(storage[0]),
            GP100_LOOPER_POST_EFFECTS, 99u, 99u)) {
        return 12;
    }
    engine.state = GP100_LOOPER_RECORDING;
    engine.recorded_samples = FRAMES * 2u;
    engine.recording_from_factory_blocks = true;
    for (frame = 0u; frame < FRAMES; ++frame) {
        audio[frame * 2u] = 30100;
        audio[frame * 2u + 1u] = -40200;
    }
    if (!gp100_looper_handle_event(&engine, GP100_LOOPER_PLAY, NULL) ||
        !gp100_looper_handle_event(&engine, GP100_LOOPER_STOP, NULL) ||
        engine.state != GP100_LOOPER_STOPPED ||
        !gp100_looper_process_factory_block(&engine, audio, &result) ||
        engine.recorded_samples != 4u * FRAMES ||
        memcmp(storage + 2u * FRAMES, falling, sizeof(falling)) != 0 ||
        !gp100_looper_process_factory_block(&engine, audio, &result) ||
        audio[0] != 37359 || audio[1] != -42550 ||
        !gp100_looper_process_factory_block(&engine, audio, &result) ||
        engine.playback_position != 0u || engine.fade_out_active) {
        return 13;
    }
    return 0;
}

static int test_factory_recording_capacity(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    static const size_t last_full_block[] = {4012864u, 4012672u};
    static const int32_t falling[] = {29682, -39334, 29219, -38718};
    static int32_t storage[GP100_LOOPER_FACTORY_STORAGE_SAMPLES];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t frame;
    size_t channels;

    for (channels = 1u; channels <= 2u; ++channels) {
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;
        size_t block_samples = FRAMES * channels;
        size_t tail = last_full_block[channels - 1u];

        for (frame = 0u; frame < FRAMES; ++frame) {
            audio[frame * 2u] = 30100;
            audio[frame * 2u + 1u] = -40200;
        }
        if (!gp100_looper_init(
                &engine, storage, sizeof(storage) / sizeof(storage[0]),
                position, 99u, 99u)) {
            return 1;
        }
        engine.state = GP100_LOOPER_RECORDING;
        engine.recorded_samples = tail - 2u * block_samples;
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            result.capacity_reached || engine.recorded_samples != tail - block_samples ||
            engine.state != GP100_LOOPER_RECORDING ||
            !gp100_looper_process_factory_block(&engine, audio, &result) ||
            !result.capacity_reached || !result.state_changed ||
            engine.state != GP100_LOOPER_PLAYING ||
            engine.recorded_samples != tail + block_samples ||
            engine.recording_exit_blocks != 2u || storage[tail] != 0) {
            return 2;
        }
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            result.capacity_reached || engine.playback_position != 0u ||
            storage[tail] != falling[0] ||
            storage[tail + 1u] != falling[channels == 1u ? 2u : 1u] ||
            !gp100_looper_process_factory_block(&engine, audio, &result) ||
            engine.playback_position != 0u ||
            !gp100_looper_process_factory_block(&engine, audio, &result) ||
            engine.playback_position != block_samples) {
            return 3;
        }
    }
    return 0;
}

static int test_factory_overdub_wrap_envelope(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    static const int32_t ordinary[] = {29799, -39798};
    static const int32_t first_fall[] = {29682, -39334};
    static const int32_t second_fall[] = {14783, -9718};
    static const int32_t first_rise_end[] = {36960, -49516};
    static const int32_t second_rise_start[] = {44698, -49826};
    int32_t storage[9u * FRAMES * 2u];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t channels;

    for (channels = 1u; channels <= 2u; ++channels) {
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;
        size_t frames = channels == 1u ? 512u : 530u;
        size_t blocks = (frames + FRAMES - 1u) / FRAMES;
        size_t callback;
        size_t channel;

        memset(storage, 0, sizeof(storage));
        if (!gp100_looper_init(
                &engine, storage, sizeof(storage) / sizeof(storage[0]),
                position, 99u, 99u)) {
            return 1;
        }
        engine.state = GP100_LOOPER_OVERDUBBING;
        engine.recorded_samples = frames * channels;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        engine.playback_gain_target_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        for (callback = 0u; callback < blocks + 3u; ++callback) {
            size_t frame;

            for (frame = 0u; frame < FRAMES; ++frame) {
                audio[2u * frame] = 30100;
                audio[2u * frame + 1u] = -40200;
            }
            if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
                result.loop_wrapped != (callback == blocks - 1u) || result.seam_smoothed) {
                return 2;
            }
            if (callback >= blocks &&
                (audio[0] != 59601 || audio[1] != (channels == 1u ? -10699 : -79600))) {
                return 3;
            }
        }
        for (channel = 0u; channel < channels; ++channel) {
            if (storage[channel] != ordinary[channel] ||
                storage[((blocks - 2u) * FRAMES) * channels + channel] != first_fall[channel] ||
                storage[((blocks - 1u) * FRAMES) * channels + channel] != second_fall[channel] ||
                storage[(2u * FRAMES - 1u) * channels + channel] != first_rise_end[channel] ||
                storage[(2u * FRAMES) * channels + channel] != second_rise_start[channel]) {
                return 4;
            }
        }
    }
    return 0;
}

static int test_factory_wet_transition(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    static const int32_t rise[2][8] = {
        {0, 0, 0, -3, 4, -12, 12, -25},
        {14899, -10027, 7623, -10342, 7861, -10662, 8102, -10986}
    };
    static const int32_t falling[] = {
        29682, -39333, 29218, -38717, 28759, -38107, 28304, -37501
    };
    int32_t storage[8u * FRAMES * 2u];
    int32_t silence[FRAMES * 2u] = {0};
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t i;

    if (!gp100_looper_init(
            &engine, storage, sizeof(storage) / sizeof(storage[0]),
            GP100_LOOPER_POST_EFFECTS, 99u, 99u)) {
        return 1;
    }
    for (i = 0u; i < sizeof(storage) / sizeof(storage[0]); i += 2u) {
        storage[i] = 30100;
        storage[i + 1u] = -40200;
    }
    engine.recorded_samples = sizeof(storage) / sizeof(storage[0]);
    engine.state = GP100_LOOPER_STOPPED;
    if (!gp100_looper_handle_event(&engine, GP100_LOOPER_PLAY, NULL)) {
        return 2;
    }
    for (i = 0u; i < 3u; ++i) {
        int32_t expected_left = 0;
        int32_t expected_right = 0;

        memset(audio, 0, sizeof(audio));
        if (i == 1u) {
            audio[0] = 30100;
            audio[1] = -40200;
            expected_left = 37359;
            expected_right = -42550;
        }
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            audio[0] != expected_left || audio[1] != expected_right ||
            memcmp(silence, audio + 2u, sizeof(silence) - 2u * sizeof(audio[0])) != 0 ||
            engine.playback_position != (i == 2u ? FRAMES * 2u : 0u)) {
            return 7;
        }
    }
    for (i = 0u; i < 2u; ++i) {
        memset(audio, 0, sizeof(audio));
        if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
            memcmp(rise[i], audio, sizeof(rise[i])) != 0) {
            return 3;
        }
    }
    memset(audio, 0, sizeof(audio));
    if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
        audio[0] != 29799 || audio[1] != -39798 ||
        !gp100_looper_handle_event(&engine, GP100_LOOPER_STOP, NULL)) {
        return 4;
    }
    memset(audio, 0, sizeof(audio));
    if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
        memcmp(falling, audio, sizeof(falling)) != 0 ||
        !engine.fade_out_active || engine.playback_position != 5u * FRAMES * 2u) {
        return 5;
    }
    memset(audio, 0, sizeof(audio));
    if (!gp100_looper_handle_event(&engine, GP100_LOOPER_STOP, NULL) ||
        !engine.fade_out_active ||
        !gp100_looper_process_factory_block(&engine, audio, &result) ||
        memcmp(silence, audio, sizeof(silence)) != 0 ||
        engine.fade_out_active || engine.playback_position != 0u) {
        return 6;
    }
    return 0;
}

static int test_factory_overdub_mode_switch(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES, LOOP_FRAMES = 1024u };
    static const size_t head[] = {320u, 320u, 384u, 448u, 512u, 576u};
    static const int32_t wet[][2] = {
        {11833, -14677}, {7259, -2350}, {0, 0},
        {0, 0}, {5940, -3742}, {11880, -14850}
    };
    int32_t storage[LOOP_FRAMES * 2u];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    unsigned entering;

    for (entering = 0u; entering < 2u; ++entering) {
        gp100_looper_event_t event = entering != 0u
            ? GP100_LOOPER_RECORD_OR_OVERDUB : GP100_LOOPER_PLAY;
        gp100_looper_state_t requested = entering != 0u
            ? GP100_LOOPER_OVERDUBBING : GP100_LOOPER_PLAYING;
        size_t frame;
        size_t callback;

        if (!gp100_looper_init(
                &engine, storage, LOOP_FRAMES * 2u,
                GP100_LOOPER_POST_EFFECTS, 99u, 99u)) {
            return 1;
        }
        for (frame = 0u; frame < LOOP_FRAMES; ++frame) {
            storage[2u * frame] = 12000;
            storage[2u * frame + 1u] = -15000;
        }
        engine.state = entering != 0u
            ? GP100_LOOPER_PLAYING : GP100_LOOPER_OVERDUBBING;
        engine.playback_position = 256u * 2u;
        engine.recorded_samples = LOOP_FRAMES * 2u;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        engine.playback_gain_target_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        if (!gp100_looper_handle_event(&engine, event, NULL) ||
            engine.state != requested) {
            return 2;
        }
        for (callback = 0u; callback < sizeof(head) / sizeof(head[0]); ++callback) {
            for (frame = 0u; frame < FRAMES; ++frame) {
                audio[2u * frame] = 30100;
                audio[2u * frame + 1u] = -40200;
            }
            if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
                engine.playback_position != head[callback] * 2u ||
                audio[0] != 30100 + wet[callback][0] ||
                audio[1] != -40200 + wet[callback][1]) {
                return 3;
            }
        }
        if (storage[256u * 2u] != (entering != 0u ? 12000 : 41682) ||
            storage[320u * 2u] != 12000 ||
            storage[448u * 2u] != (entering != 0u ? 26899 : 12000) ||
            storage[512u * 2u] != (entering != 0u ? 41799 : 12000)) {
            return 4;
        }
    }
    return 0;
}

static int test_factory_mono_overdub_backup(void)
{
    static const struct {
        size_t length;
        size_t start;
        size_t overdub_blocks;
        bool stop;
        int32_t expected_at_start;
    } cases[] = {
        {1024u, 256u, 3u, true, 5792},
        {1024u, 256u, 3u, false, 25592},
        {530u, 320u, 5u, true, 4448},
        {24000u, 256u, 338u, true, 5792},
        {24000u, 256u, 339u, true, 25592},
    };
    static int32_t scratch[GP100_LOOPER_MONO_BACKUP_SAMPLES];
    static int32_t storage[24001];
    int32_t audio[GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    size_t test;

    for (test = 0u; test < sizeof(cases) / sizeof(cases[0]); ++test) {
        size_t index;
        size_t block;

        if (!gp100_looper_init(&engine, storage, cases[test].length + 1u,
                GP100_LOOPER_PRE_EFFECTS, 99u, 99u) ||
            !gp100_looper_set_mono_overdub_backup(&engine, scratch,
                sizeof(scratch) / sizeof(scratch[0]))) {
            return 1;
        }
        for (index = 0u; index < cases[test].length; ++index) {
            storage[index] = 4000 + (int32_t)(index * 7u);
        }
        storage[cases[test].length] = 12345;
        engine.state = GP100_LOOPER_OVERDUBBING;
        engine.recorded_samples = cases[test].length;
        engine.playback_position = cases[test].start;
        engine.playback_gain_q8 = GP100_LOOPER_TRANSITION_FRAMES;
        engine.playback_gain_target_q8 = GP100_LOOPER_TRANSITION_FRAMES;

        for (block = 0u; block < cases[test].overdub_blocks + 2u; ++block) {
            if (block == cases[test].overdub_blocks &&
                (!gp100_looper_handle_event(&engine, GP100_LOOPER_PLAY, NULL) ||
                 (cases[test].stop && !gp100_looper_handle_event(
                     &engine, GP100_LOOPER_STOP, NULL)))) {
                return 2;
            }
            for (index = 0u; index < GP100_LOOPER_FACTORY_BLOCK_FRAMES; ++index) {
                audio[2u * index] = 20000;
                audio[2u * index + 1u] = -30000;
            }
            if (!gp100_looper_process_factory_block(&engine, audio, &result)) {
                return 3;
            }
        }
        if (storage[cases[test].start] != cases[test].expected_at_start ||
            storage[cases[test].length] != 12345 ||
            engine.mono_overdub_backup_samples != 0u) {
            return 4;
        }
        if (cases[test].length == 530u &&
            (storage[64] != 24170 || storage[211] != 0 || storage[256] != 4000 ||
             storage[529] != 7703)) {
            return 5;
        }
    }
    return 0;
}

static int test_factory_clear_transition(void)
{
    enum { FRAMES = GP100_LOOPER_FACTORY_BLOCK_FRAMES };
    int32_t storage[8u * FRAMES * 2u];
    int32_t audio[FRAMES * 2u];
    gp100_looper_engine_t engine;
    gp100_looper_process_result_t result;
    gp100_looper_state_t state;
    size_t channels;

    for (channels = 1u; channels <= 2u; ++channels) {
        gp100_looper_position_t position = channels == 1u
            ? GP100_LOOPER_PRE_EFFECTS : GP100_LOOPER_POST_EFFECTS;

        for (state = GP100_LOOPER_RECORDING;
                state <= GP100_LOOPER_STOPPED; ++state) {
            bool audible = state == GP100_LOOPER_PLAYING ||
                           state == GP100_LOOPER_OVERDUBBING;
            size_t callback;
            size_t sample;
            bool changed;

            if (!gp100_looper_init(&engine, storage,
                    sizeof(storage) / sizeof(storage[0]), position, 99u, 99u)) {
                return 1;
            }
            for (sample = 0u; sample < 8u * FRAMES * channels; ++sample) {
                storage[sample] = sample % channels == 0u ? 11000 : 9000;
            }
            engine.state = state;
            engine.last_callback_was_factory_block = true;
            engine.recorded_samples = 8u * FRAMES * channels;
            engine.playback_position = audible ? 2u * FRAMES * channels : 0u;
            if (!gp100_looper_handle_event(
                    &engine, GP100_LOOPER_CLEAR, &changed) || !changed ||
                engine.state != GP100_LOOPER_IDLE ||
                engine.recorded_samples != 0u || engine.playback_position != 0u) {
                return 2;
            }
            for (callback = 0u; callback < 3u; ++callback) {
                for (sample = 0u; sample < FRAMES; ++sample) {
                    audio[2u * sample] = 30100;
                    audio[2u * sample + 1u] = -40200;
                }
                if (!gp100_looper_process_factory_block(&engine, audio, &result) ||
                    result.frames_processed != FRAMES) {
                    return 3;
                }
                if (callback == 0u && audible) {
                    if (audio[0] != 30100 + 10847 ||
                        audio[1] != -40200 + (channels == 1u ? 10762 : 8804)) {
                        return 4;
                    }
                } else if (callback == 1u) {
                    if (audio[0] != 30100 + 7259 || audio[1] != -40200 - 2350) {
                        return 5;
                    }
                } else if (audio[0] != 30100 || audio[1] != -40200) {
                    return 6;
                }
            }
        }
    }
    return 0;
}

int main(void)
{
    int result;

    result = test_post_record_play_and_clear();
    if (result != 0) {
        fprintf(stderr, "post record/play test failed: %d\n", result);
        return 10 + result;
    }
    result = test_pre_mono_and_capacity();
    if (result != 0) {
        fprintf(stderr, "pre/capacity test failed: %d\n", result);
        return 20 + result;
    }
    result = test_overdub_stop_levels_and_position();
    if (result != 0) {
        fprintf(stderr, "overdub/level test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_capacity();
    if (result != 0) {
        fprintf(stderr, "factory capacity test failed: %d\n", result);
        return 40 + result;
    }
    result = test_transition_seam_and_beat_extension();
    if (result != 0) {
        fprintf(stderr, "transition/seam/beat test failed: %d\n", result);
        return 50 + result;
    }
    result = test_factory_block_wrap();
    if (result != 0) {
        fprintf(stderr, "factory block wrap test failed: %d\n", result);
        return 60 + result;
    }
    result = test_factory_recording_fades();
    if (result != 0) {
        fprintf(stderr, "factory recording fade test failed: %d\n", result);
        return 70 + result;
    }
    result = test_factory_wet_transition();
    if (result != 0) {
        fprintf(stderr, "factory wet transition test failed: %d\n", result);
        return 90 + result;
    }
    result = test_factory_recording_capacity();
    if (result != 0) {
        fprintf(stderr, "factory recording capacity test failed: %d\n", result);
        return 95 + result;
    }
    result = test_factory_overdub_wrap_envelope();
    if (result != 0) {
        fprintf(stderr, "factory overdub wrap test failed: %d\n", result);
        return 100 + result;
    }
    result = test_factory_overdub_mode_switch();
    if (result != 0) {
        fprintf(stderr, "factory overdub mode switch test failed: %d\n", result);
        return 110 + result;
    }
    result = test_factory_mono_overdub_backup();
    if (result != 0) {
        fprintf(stderr, "factory mono overdub backup test failed: %d\n", result);
        return 120 + result;
    }
    result = test_factory_short_aligned_wrap();
    if (result != 0) {
        fprintf(stderr, "factory short aligned wrap test failed: %d\n", result);
        return 130 + result;
    }
    result = test_factory_clear_transition();
    if (result != 0) {
        fprintf(stderr, "factory clear transition test failed: %d\n", result);
        return 140 + result;
    }
    return 0;
}
