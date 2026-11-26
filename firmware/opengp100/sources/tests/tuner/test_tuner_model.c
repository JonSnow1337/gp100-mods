#include "tuner/tuner_model.h"

#include <stdio.h>

static float absolute_value(float value)
{
    return value < 0.0f ? -value : value;
}

static int test_defaults_and_controls(void)
{
    gp100_tuner_model_t model;

    gp100_tuner_reset(&model);
    if (GP100_TUNER_OUTPUT_MUTE != 0 ||
        GP100_TUNER_OUTPUT_THRU != 1 ||
        GP100_TUNER_OUTPUT_BYPASS != 2 ||
        model.reference_hz != 440u ||
        model.output_mode != GP100_TUNER_OUTPUT_MUTE ||
        gp100_tuner_audio_source(model.output_mode) !=
            GP100_TUNER_AUDIO_SILENCE ||
        !gp100_tuner_set_output_mode(
            &model, GP100_TUNER_OUTPUT_THRU) ||
        gp100_tuner_audio_source(model.output_mode) !=
            GP100_TUNER_AUDIO_EFFECTS ||
        !gp100_tuner_set_output_mode(
            &model, GP100_TUNER_OUTPUT_MUTE) ||
        gp100_tuner_audio_source(model.output_mode) !=
            GP100_TUNER_AUDIO_SILENCE ||
        gp100_tuner_set_output_mode(
            &model, (gp100_tuner_output_mode_t)3) ||
        gp100_tuner_set_output_mode(
            &model, (gp100_tuner_output_mode_t)-1) ||
        gp100_tuner_set_reference(&model, 434u) ||
        gp100_tuner_set_reference(&model, 446u) ||
        !gp100_tuner_set_reference(&model, 435u) ||
        !gp100_tuner_set_reference(&model, 445u)) {
        return 1;
    }
    return 0;
}

static int test_note_table_and_exact_notes(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;
    float a4;
    float c4;

    gp100_tuner_reset(&model);
    a4 = gp100_tuner_note_frequency(440u, 53u);
    c4 = gp100_tuner_note_frequency(440u, 44u);
    if (a4 != 440.0f || absolute_value(c4 - 261.6256f) > 0.02f ||
        gp100_tuner_note_frequency(434u, 53u) != 0.0f ||
        gp100_tuner_note_frequency(440u, 93u) != 0.0f ||
        !gp100_tuner_update_frequency(&model, a4, &reading) ||
        !reading.valid || reading.current_note_index != 53u ||
        reading.stable_note_index != 53u || reading.note_letter != 'A' ||
        reading.sharp || reading.octave != 4u ||
        absolute_value(reading.cents) > 0.01f || reading.needle_step != 0) {
        return 1;
    }

    gp100_tuner_reset(&model);
    if (!gp100_tuner_update_frequency(&model, c4, &reading) ||
        reading.current_note_index != 44u || reading.note_letter != 'C' ||
        reading.sharp || reading.octave != 4u) {
        return 2;
    }
    return 0;
}

static int test_factory_period_conversion(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;
    float a4;

    gp100_tuner_reset(&model);
    a4 = gp100_tuner_frequency_from_factory_period(109091u);
    if (gp100_tuner_frequency_from_factory_period(0u) != 0.0f ||
        absolute_value(a4 - 440.0f) > 0.01f ||
        !gp100_tuner_update_factory_period(
            &model, 109091u, &reading) ||
        reading.current_note_index != 53u || reading.note_letter != 'E' ||
        reading.sharp || reading.octave != 0u ||
        absolute_value(reading.cents) > 0.01f ||
        gp100_tuner_update_factory_period(&model, 0u, &reading)) {
        return 1;
    }
    return 0;
}

static int expect_reduction(
    const uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    uint32_t expected_period,
    gp100_tuner_reduction_path_t expected_path)
{
    gp100_tuner_period_result_t result;

    if (!gp100_tuner_reduce_factory_intervals(intervals, &result) ||
        result.normalized_period != expected_period ||
        result.path != expected_path) {
        fprintf(
            stderr,
            "reduction mismatch: got period %u path %u, expected %u/%u\n",
            (unsigned)result.normalized_period,
            (unsigned)result.path,
            (unsigned)expected_period,
            (unsigned)expected_path);
        return 1;
    }
    return 0;
}

static int test_factory_interval_reducer(void)
{
    static const uint32_t single[12] = {
        109091u, 109091u, 109091u, 109091u,
        109091u, 109091u, 109091u, 109091u,
        109091u, 109091u, 109091u, 109091u
    };
    static const uint32_t paired[12] = {
        40000u, 60000u, 40000u, 60000u,
        40000u, 60000u, 40000u, 60000u,
        40000u, 60000u, 40000u, 60000u
    };
    static const uint32_t displaced[12] = {
        200000u, 100000u, 100000u, 200000u,
        90000u, 90000u, 10000u, 30000u,
        20000u, 10000u, 30000u, 20000u
    };
    static const uint32_t three_special[12] = {
        80000u, 40000u, 30000u, 75000u,
        45000u, 30000u, 100000u, 10000u,
        10000u, 50000u, 20000u, 10000u
    };
    static const uint32_t three_at_float_threshold[12] = {
        80000u, 40000u, 30000u, 75000u,
        45000u, 28500u, 100000u, 10000u,
        10000u, 50000u, 20000u, 10000u
    };
    static const uint32_t three_normal[12] = {
        100000u, 120000u, 80000u, 100000u,
        119000u, 80000u, 101000u, 120000u,
        80000u, 10000u, 30000u, 20000u
    };
    static const uint32_t four_special[12] = {
        100000u, 100000u, 300000u, 100000u,
        100000u, 300000u, 100000u, 100000u,
        100000u, 100000u, 100000u, 100000u
    };
    static const uint32_t four_normal[12] = {
        100000u, 100000u, 500000u, 100000u,
        100000u, 500000u, 100000u, 100000u,
        100000u, 100000u, 500000u, 100000u
    };
    static const uint32_t rejected[12] = {
        1000u, 2000u, 3000u, 4000u,
        5000u, 6000u, 7000u, 8000u,
        9000u, 10000u, 11000u, 12000u
    };
    static const uint32_t first_interval_out_of_range[12] = {
        1000u, 20000u, 1000u, 20000u, 1000u, 20000u,
        1000u, 20000u, 1000u, 20000u, 1000u, 20000u
    };
    gp100_tuner_period_result_t result;
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;

    if (expect_reduction(
            single, 109091u,
            GP100_TUNER_REDUCTION_SINGLE_INTERVAL) != 0 ||
        expect_reduction(
            paired, 100000u,
            GP100_TUNER_REDUCTION_TWO_INTERVAL) != 0 ||
        expect_reduction(
            displaced, 195000u,
            GP100_TUNER_REDUCTION_DISPLACED_TWO_INTERVAL) != 0 ||
        expect_reduction(
            three_special, 150000u,
            GP100_TUNER_REDUCTION_THREE_INTERVAL_TWO_GROUP) != 0 ||
        expect_reduction(
            three_at_float_threshold, 149250u,
            GP100_TUNER_REDUCTION_THREE_INTERVAL_TWO_GROUP) != 0 ||
        expect_reduction(
            three_normal, 300000u,
            GP100_TUNER_REDUCTION_THREE_INTERVAL) != 0 ||
        expect_reduction(
            four_special, 600000u,
            GP100_TUNER_REDUCTION_FOUR_INTERVAL_TWO_GROUP) != 0 ||
        expect_reduction(
            four_normal, 800000u,
            GP100_TUNER_REDUCTION_FOUR_INTERVAL) != 0) {
        return 1;
    }
    if (gp100_tuner_reduce_factory_intervals(rejected, &result) ||
        result.path != GP100_TUNER_REDUCTION_NONE ||
        result.normalized_period != 0u ||
        gp100_tuner_reduce_factory_intervals(NULL, &result) ||
        gp100_tuner_reduce_factory_intervals(single, NULL)) {
        return 2;
    }

    gp100_tuner_reset(&model);
    if (!gp100_tuner_update_factory_intervals(
            &model, single, &reading) ||
        reading.current_note_index != 53u || reading.note_letter != 'E' ||
        reading.sharp || reading.octave != 0u ||
        gp100_tuner_update_factory_intervals(
            &model, first_interval_out_of_range, &reading) ||
        model.history_count != 1u) {
        return 3;
    }
    return 0;
}

static int test_factory_midpoint_and_stabilizer(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;
    unsigned count;

    gp100_tuner_reset(&model);
    if (!gp100_tuner_update_factory_period(&model, 106028u, &reading) ||
        reading.current_note_index != 54u || reading.cents != -1.0f ||
        !gp100_tuner_update_factory_period(&model, 106029u, &reading) ||
        reading.current_note_index != 53u) {
        return 1;
    }
    gp100_tuner_reset(&model);
    if (!gp100_tuner_update_factory_period(&model, 109122u, &reading) ||
        reading.cents != -0.5f) {
        return 2;
    }
    gp100_tuner_reset(&model);
    for (count = 1u; count <= 64u; ++count) {
        uint8_t displayed_note = count <= 32u ? 0u : 53u;
        float cents = (float)((count * 40u) / 64u) * 0.5f;

        if (!gp100_tuner_update_factory_period(&model, 107838u, &reading) ||
            reading.current_note_index != 53u ||
            reading.stable_note_index != displayed_note || reading.cents != cents ||
            reading.octave != (count <= 32u ? 0u : 4u)) {
            return 3;
        }
    }
    if (reading.note_letter != 'A' || reading.needle_step != 2 ||
        !gp100_tuner_update_factory_period(&model, 11467u, &reading) ||
        reading.current_note_index != 92u) {
        return 4;
    }
    return 0;
}

static int test_cents_needle_and_hysteresis(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;
    float a4;

    gp100_tuner_reset(&model);
    a4 = gp100_tuner_note_frequency(440u, 53u);
    if (!gp100_tuner_update_frequency(
            &model, a4 * 1.01161944f, &reading) ||
        absolute_value(reading.cents - 20.0f) > 1.0f ||
        reading.needle_step != 1) {
        return 1;
    }

    /* The averaged value moves, but less than ten cents retains needle bin 1. */
    if (!gp100_tuner_update_frequency(
            &model, a4 * 1.01512f, &reading) ||
        reading.needle_step != 1) {
        return 2;
    }

    gp100_tuner_reset(&model);
    if (!gp100_tuner_update_frequency(
            &model, a4 * 0.9828206f, &reading) ||
        absolute_value(reading.cents + 30.0f) > 1.0f ||
        reading.needle_step != -2) {
        return 3;
    }
    return 0;
}

static int test_range_and_reference(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;

    gp100_tuner_reset(&model);
    if (gp100_tuner_update_frequency(&model, 0.0f, &reading) ||
        gp100_tuner_update_frequency(&model, 10.0f, &reading) ||
        gp100_tuner_update_frequency(&model, 5000.0f, &reading) ||
        !gp100_tuner_set_reference(&model, 442u) ||
        !gp100_tuner_update_frequency(&model, 442.0f, &reading) ||
        reading.note_letter != 'A' || reading.octave != 4u ||
        absolute_value(reading.cents) > 0.01f) {
        return 1;
    }
    return 0;
}

static int test_stable_note_vote_threshold(void)
{
    gp100_tuner_model_t model;
    gp100_tuner_reading_t reading;
    float a4;
    float b4;
    unsigned count;

    gp100_tuner_reset(&model);
    a4 = gp100_tuner_note_frequency(440u, 53u);
    b4 = gp100_tuner_note_frequency(440u, 55u);
    if (!gp100_tuner_update_frequency(&model, a4, &reading)) {
        return 1;
    }
    for (count = 0u; count < 11u; ++count) {
        if (!gp100_tuner_update_frequency(&model, b4, &reading)) {
            return 2;
        }
    }
    if (reading.current_note_index != 55u ||
        reading.stable_note_index != 53u || reading.note_letter != 'A') {
        return 3;
    }
    if (!gp100_tuner_update_frequency(&model, b4, &reading) ||
        reading.stable_note_index != 55u || reading.note_letter != 'B' ||
        reading.sharp) {
        return 4;
    }
    return 0;
}

int main(void)
{
    int result;

    result = test_defaults_and_controls();
    if (result != 0) {
        fprintf(stderr, "tuner controls test failed: %d\n", result);
        return 10 + result;
    }
    result = test_note_table_and_exact_notes();
    if (result != 0) {
        fprintf(stderr, "tuner note test failed: %d\n", result);
        return 20 + result;
    }
    result = test_cents_needle_and_hysteresis();
    if (result != 0) {
        fprintf(stderr, "tuner cents test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_period_conversion();
    if (result != 0) {
        fprintf(stderr, "tuner factory period test failed: %d\n", result);
        return 35 + result;
    }
    result = test_factory_interval_reducer();
    if (result != 0) {
        fprintf(stderr, "tuner factory interval test failed: %d\n", result);
        return 37 + result;
    }
    result = test_factory_midpoint_and_stabilizer();
    if (result != 0) {
        fprintf(stderr, "tuner factory display test failed: %d\n", result);
        return 60 + result;
    }
    result = test_range_and_reference();
    if (result != 0) {
        fprintf(stderr, "tuner range test failed: %d\n", result);
        return 40 + result;
    }
    result = test_stable_note_vote_threshold();
    if (result != 0) {
        fprintf(stderr, "tuner vote test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
