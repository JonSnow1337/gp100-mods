#include "audio/effects/pitch/hammy.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

static void seed_read_head(gp100_hammy_t *hammy, size_t write_index,
                           int history_position, float read_position,
                           float position)
{
    hammy->write_index = write_index;
    hammy->history_position = history_position;
    hammy->read_position = read_position;
    hammy->volume = 0.99f;
    hammy->position = position;
}

static int test_factory_read_heads(void)
{
    gp100_hammy_t hammy;
    float audio[2] = {0.0f, 0.0f};

    if (!gp100_hammy_reset(&hammy, 4u, false, 99u, 50u)) {
        return 1;
    }
    seed_read_head(&hammy, 300u, 300, 200.25f, 0.5f);
    hammy.delay[201] = 0.2f;
    hammy.delay[202] = 0.6f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.1485f, 0.00001f) ||
        audio[0] != audio[1] || hammy.splice_active) {
        return 2;
    }
    seed_read_head(&hammy, 90u, 1000, 908.75f, 0.5f);
    hammy.delay[GP100_HAMMY_DELAY_SAMPLES - 1u] = 0.2f;
    hammy.delay[0] = 0.6f;
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        !close_enough(audio[0], 0.2475f, 0.00001f)) {
        return 3;
    }
    if (!gp100_hammy_reset(&hammy, 4u, false, 99u, 75u)) {
        return 4;
    }
    seed_read_head(&hammy, 1000u, 1000, 940.0f, 0.75f);
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 75u, audio, 1u, true) ||
        !hammy.splice_active ||
        !close_enough(hammy.splice_span, 58.585815f, 0.0001f) ||
        !close_enough(hammy.read_position, 841.414185f, 0.0001f)) {
        return 5;
    }
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 75u, audio, 1u, true) ||
        !close_enough(hammy.read_position, 742.8284f, 0.0001f) ||
        !close_enough(hammy.splice_phase, hammy.ratio, 0.0001f)) {
        return 13;
    }
    if (!gp100_hammy_reset(&hammy, 0u, false, 99u, 50u)) {
        return 6;
    }
    seed_read_head(&hammy, 1000u, 1000, 760.0f, 0.5f);
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 0u, false, 99u, 50u, audio, 1u, true) ||
        !hammy.splice_active ||
        hammy.read_position != 960.5f ||
        !close_enough(hammy.splice_span, 128.0f, 0.0001f)) {
        return 7;
    }
    if (!gp100_hammy_reset(&hammy, 4u, false, 99u, 50u)) {
        return 8;
    }
    seed_read_head(&hammy, 300u, 300, 200.25f, 0.5f);
    hammy.delay[201] = 0.2f;
    hammy.delay[202] = 0.2f;
    hammy.delay[203] = 0.2f;
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        hammy.transient_hold != 4410u || !hammy.soft_transient) {
        return 9;
    }
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        hammy.transient_hold != 4409u || !hammy.soft_transient) {
        return 10;
    }
    if (!gp100_hammy_reset(&hammy, 0u, false, 99u, 50u) ||
        !gp100_hammy_process(
            &hammy, 0u, false, 99u, 50u, audio, 1u, true) ||
        !hammy.splice_active || hammy.period_search_count != 0u ||
        hammy.read_position != 3624.5f) {
        return 11;
    }
    hammy.splice_active = false;
    seed_read_head(&hammy, 1000u, 1000, 879.5f, 0.5f);
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 0u, false, 99u, 50u, audio, 1u, true) ||
        hammy.splice_active || hammy.read_position != 880.0f) {
        return 12;
    }
    return 0;
}

static int test_factory_detector_trajectory(void)
{
    static const struct {
        size_t samples;
        size_t crossings;
        int anchor;
        float raw;
        float stable;
    } checkpoints[] = {
        {512u, 5u, 4, 100.4104004f, 100.1490860f},
        {832u, 12u, 10, 200.6911621f, 200.6911621f},
        {1216u, 19u, 18, 401.1054688f, 401.1054688f},
        {8192u, 49u, 47, 1302.9504395f, 1302.9527588f},
    };
    gp100_hammy_t hammy;
    float audio[128];
    size_t first;
    size_t checkpoint = 0u;

    if (!gp100_hammy_reset(&hammy, 0u, false, 99u, 50u)) {
        return 0;
    }
    for (first = 0u; first < 8192u; first += 64u) {
        size_t frame;

        for (frame = 0u; frame < 64u; ++frame) {
            double phase = 6.28318530717958647692 * 440.0 *
                           (double)(first + frame) / 44100.0;
            float input = (float)(0.2 * sin(phase));

            audio[2u * frame] = input;
            audio[2u * frame + 1u] = input;
        }
        if (!gp100_hammy_process(
                &hammy, 0u, false, 99u, 50u, audio, 64u, true)) {
            return 0;
        }
        if (first + 64u == checkpoints[checkpoint].samples) {
            if (hammy.crossing_count != checkpoints[checkpoint].crossings ||
                hammy.crossing_anchor != checkpoints[checkpoint].anchor ||
                !close_enough(hammy.raw_period, checkpoints[checkpoint].raw, 0.002f) ||
                !close_enough(hammy.detected_period, checkpoints[checkpoint].stable, 0.002f)) {
                return 0;
            }
            ++checkpoint;
        }
    }
    return checkpoint == sizeof(checkpoints) / sizeof(checkpoints[0]);
}

static int test_factory_hard_transient_marker(void)
{
    gp100_hammy_t hammy;
    float audio[2] = {0.0f, 0.0f};

    if (!gp100_hammy_reset(&hammy, 4u, false, 99u, 50u)) {
        return 0;
    }
    seed_read_head(&hammy, 3999u, 3999, 3890.0f, 0.5f);
    hammy.crossing_count = 3u;
    hammy.crossings[0] = 300.25f;
    hammy.crossings[1] = 3600.5f;
    hammy.crossings[2] = 3700.75f;
    hammy.delay[3891] = 0.2f;
    hammy.delay[3892] = 0.2f;
    hammy.delay[3893] = 0.2f;
    hammy.delay[3894] = 0.2f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        !hammy.hard_transient || hammy.hard_transient_marker != 3444 ||
        hammy.history_position != 3744 || hammy.read_position != 3635.0f ||
        hammy.write_index != 0u || hammy.crossing_count != 2u ||
        hammy.crossings[1] != 3444.75f) {
        return 0;
    }
    hammy.crossings[1] = 3457.125f;
    audio[0] = audio[1] = 0.0f;
    if (!gp100_hammy_process(
            &hammy, 4u, false, 99u, 50u, audio, 1u, true) ||
        hammy.hard_transient_marker != 3444) {
        return 0;
    }
    hammy.position = 0.75f;
    audio[0] = audio[1] = 0.0f;
    return gp100_hammy_process(
               &hammy, 4u, false, 99u, 75u, audio, 1u, true) &&
           hammy.splice_active && hammy.hard_transient_marker == -1;
}

static int test_sustained_note(uint8_t range, uint8_t position, float expected_frequency)
{
    gp100_hammy_t hammy;
    float audio[128];
    float previous = 0.0f;
    unsigned crossings = 0u;
    size_t first;

    if (!gp100_hammy_reset(&hammy, range, false, 99u, position)) {
        return 0;
    }
    for (first = 0u; first < 8192u; first += 64u) {
        size_t frame;

        for (frame = 0u; frame < 64u; ++frame) {
            float phase = 6.2831855f * 440.0f * (float)(first + frame) / 44100.0f;
            float input = 0.2f * sinf(phase);

            audio[2u * frame] = input;
            audio[2u * frame + 1u] = input;
        }
        if (!gp100_hammy_process(
                &hammy, range, false, 99u, position, audio, 64u, true)) {
            return 0;
        }
        if (first >= 4096u) {
            for (frame = 0u; frame < 64u; ++frame) {
                float sample = audio[2u * frame];

                if (previous < 0.0f && sample >= 0.0f) {
                    ++crossings;
                }
                previous = sample;
            }
        }
    }
    return close_enough((float)crossings * 44100.0f / 4096.0f, expected_frequency, 11.0f);
}

static int test_factory_downward_crossfade(void)
{
    gp100_hammy_t hammy;
    float first[128] = {0.0f};
    float second[128] = {0.0f};
    size_t index;

    if (!gp100_hammy_reset(&hammy, 0u, false, 99u, 50u)) {
        return 0;
    }
    seed_read_head(&hammy, 1000u, 1000, 820.0f, 0.5f);
    for (index = 820u; index < 1000u; ++index) {
        hammy.delay[index] = index < 900u ? 0.2f : 0.6f;
    }
    return gp100_hammy_process(&hammy, 0u, false, 99u, 50u, first, 64u, true) &&
           hammy.splice_active && hammy.splice_phase == 32.0f &&
           gp100_hammy_process(&hammy, 0u, false, 99u, 50u, second, 64u, true) &&
           !hammy.splice_active &&
           close_enough(first[0], 0.099f, 0.000001f) &&
           close_enough(second[0], 0.198f, 0.000001f) &&
           close_enough(second[126], 0.29697016f, 0.000001f);
}

int main(void)
{
    gp100_hammy_t hammy;
    gp100_hammy_t detector;
    float bypass[2] = {0.25f, -0.5f};
    float audio[256];
    float detector_audio[256];
    size_t block;
    size_t frame;

    if (!gp100_hammy_reset(&hammy, 2u, false, 99u, 0u) ||
        !close_enough(hammy.semitones, 6.0f, 0.000001f) ||
        !close_enough(hammy.ratio, 1.4142135f, 0.00001f) ||
        !gp100_hammy_process(
            &hammy, 2u, true, 50u, 25u, bypass, 2u, false) ||
        bypass[0] != 0.25f || bypass[1] != -0.5f || hammy.write_index != 0u ||
        !close_enough(hammy.volume, 0.998f, 0.000001f) ||
        !close_enough(hammy.harmony_mix, 0.998f, 0.000001f) ||
        !close_enough(hammy.position, 0.48f, 0.000001f)) {
        fprintf(stderr, "Hammy mapping/bypass test failed\n");
        return 1;
    }
    for (frame = 0u; frame < 128u; ++frame) {
        audio[frame * 2u] = sinf(6.2831855f * 440.0f * (float)frame / 44100.0f);
        audio[frame * 2u + 1u] = -1.0f;
    }
    if (!gp100_hammy_process(
            &hammy, 4u, true, 99u, 99u, audio, 128u, true) ||
        hammy.write_index != 128u || hammy.semitones <= 11.0f ||
        hammy.ratio <= 1.8f || !isfinite(hammy.detected_period) ||
        hammy.splice_count == 0u ||
        hammy.splice_span <= 0.0f ||
        hammy.splice_span > hammy.detected_period) {
        fprintf(stderr, "Hammy processing test failed\n");
        return 2;
    }
    for (frame = 0u; frame < 128u; ++frame) {
        if (!isfinite(audio[frame * 2u]) ||
            audio[frame * 2u] != audio[frame * 2u + 1u]) {
            fprintf(stderr, "Hammy output test failed at %lu\n",
                    (unsigned long)frame);
            return 3;
        }
    }
    if (!test_factory_detector_trajectory()) {
        fprintf(stderr, "Hammy factory detector trajectory mismatch\n");
        return 6;
    }
    if (!gp100_hammy_reset(&detector, 0u, false, 99u, 99u)) {
        fprintf(stderr, "Hammy downward scheduler reset failed\n");
        return 7;
    }
    for (block = 0u; block < 48u; ++block) {
        for (frame = 0u; frame < 128u; ++frame) {
            size_t sample = block * 128u + frame;
            float input = sinf(
                6.2831855f * 220.0f * (float)sample / 44100.0f);

            detector_audio[frame * 2u] = input;
            detector_audio[frame * 2u + 1u] = input;
        }
        if (!gp100_hammy_process(
                &detector,
                0u,
                false,
                99u,
                99u,
                detector_audio,
                128u,
                true)) {
            fprintf(stderr, "Hammy downward scheduler processing failed\n");
            return 8;
        }
    }
    if (detector.ratio >= 0.3f || detector.splice_count == 0u ||
        !close_enough(
            detector.splice_span, detector.ratio * 256.0f, 0.001f) ||
        !isfinite(detector_audio[254]) ||
        detector_audio[254] != detector_audio[255]) {
        fprintf(
            stderr,
            "Hammy downward scheduler mismatch: ratio=%f splices=%lu\n",
            (double)detector.ratio,
            (unsigned long)detector.splice_count);
        return 9;
    }
    if (gp100_hammy_reset(NULL, 0u, false, 0u, 0u) ||
        gp100_hammy_reset(&hammy, 6u, false, 0u, 0u) ||
        gp100_hammy_process(
            &hammy, 0u, false, 100u, 0u, audio, 1u, true) ||
        gp100_hammy_process(
            &hammy, 0u, false, 0u, 0u, NULL, 1u, true)) {
        fprintf(stderr, "Hammy validation test failed\n");
        return 10;
    }
    {
        int result = test_factory_read_heads();

        if (result != 0) {
            fprintf(stderr, "Hammy factory read head test failed: %d\n", result);
            return 20 + result;
        }
    }
    if (!test_sustained_note(0u, 50u, 220.0f) ||
        !test_sustained_note(4u, 75u, 622.25f)) {
        fprintf(stderr, "Hammy sustained-note pitch test failed\n");
        return 11;
    }
    if (!test_factory_downward_crossfade()) {
        fprintf(stderr, "Hammy downward crossfade test failed\n");
        return 12;
    }
    if (!test_factory_hard_transient_marker()) {
        fprintf(stderr, "Hammy hard transient marker test failed\n");
        return 13;
    }
    return 0;
}
