#include "tuner_model.h"

#include <string.h>

#define A4_NOTE_INDEX 53u
#define SEMITONE_RATIO 1.0594630943592952646
#define HALF_SEMITONE_RATIO 1.029302236643492f
#define CENTS_PER_OCTAVE 1200.0f
#define INVERSE_LN_2 1.4426950408889634f
#define FACTORY_SINGLE_RATIO_MINIMUM 0.994
#define FACTORY_GROUP_RATIO_MINIMUM 0.99f
#define FACTORY_MULTI_GROUP_RATIO_MINIMUM 0.99
#define FACTORY_THREE_GROUP_BAND_LOW 0x214acu
#define FACTORY_THREE_GROUP_BAND_HIGH 0x2a472u
#define FACTORY_FOUR_GROUP_BAND_LOW 0x852b0u
#define FACTORY_FOUR_GROUP_BAND_HIGH 0xa91c8u

/* Descending half-cent ratio boundaries from the factory DTCM initializer. */
static const float factory_detune_thresholds[] = {
    0.999711096f, 0.999422312f, 0.999133587f, 0.998844922f,
    0.998556376f, 0.998267949f, 0.997979522f, 0.997691214f,
    0.997403026f, 0.997114897f, 0.996826828f, 0.996538877f,
    0.996250987f, 0.995963156f, 0.995675445f, 0.995387793f,
    0.99510026f, 0.994812787f, 0.994525373f, 0.994238079f,
    0.993950844f, 0.993663728f, 0.993376672f, 0.993089676f,
    0.992802799f, 0.992515981f, 0.992229283f, 0.991942644f,
    0.991656065f, 0.991369605f, 0.991083205f, 0.990796864f,
    0.990510643f, 0.990224481f, 0.989938438f, 0.989652455f,
    0.989366531f, 0.989080727f, 0.988794982f, 0.988509357f,
    0.988223791f, 0.987938285f, 0.987652898f, 0.98736757f,
    0.987082362f, 0.986797214f, 0.986512125f, 0.986227095f,
    0.985942185f, 0.985657394f, 0.985372663f, 0.985087991f,
    0.984803379f, 0.984518886f, 0.984234512f, 0.983950138f,
    0.983665884f, 0.983381748f, 0.983097672f, 0.982813656f,
    0.9825297f, 0.982245862f, 0.981962144f, 0.981678426f,
    0.981394827f, 0.981111348f, 0.980827928f, 0.980544567f,
    0.980261266f, 0.979978085f, 0.979695022f, 0.97941196f,
    0.979129016f, 0.978846192f, 0.978563428f, 0.978280723f,
    0.977998078f, 0.977715552f, 0.977433085f, 0.977150738f,
    0.976868451f, 0.976586282f, 0.976304114f, 0.976022065f,
    0.975740135f, 0.975458264f, 0.975176454f, 0.974894762f,
    0.97461307f, 0.974331558f, 0.974050045f, 0.973768711f,
    0.973487377f, 0.973206162f, 0.972925007f, 0.972643912f,
    0.972362936f, 0.972082019f, 0.971801221f, 0.971520483f,
};

static float absolute_value(float value)
{
    return value < 0.0f ? -value : value;
}

static float unsigned_similarity(uint32_t first, uint32_t second)
{
    uint32_t smaller;
    uint32_t larger;

    if (first < second) {
        smaller = first;
        larger = second;
    } else {
        smaller = second;
        larger = first;
    }
    if (larger == 0u) {
        return 0.0f;
    }
    return (float)smaller / (float)larger;
}

static uint32_t sum_intervals(
    const uint32_t *intervals,
    size_t first,
    size_t count)
{
    uint32_t sum = 0u;
    size_t index;

    for (index = 0u; index < count; ++index) {
        sum += intervals[first + index];
    }
    return sum;
}

static bool set_period_result(
    gp100_tuner_period_result_t *result,
    uint32_t normalized_period,
    gp100_tuner_reduction_path_t path)
{
    result->normalized_period = normalized_period;
    result->path = path;
    return true;
}

static bool reference_is_valid(uint16_t reference_hz)
{
    return reference_hz >= GP100_TUNER_REFERENCE_MIN_HZ &&
           reference_hz <= GP100_TUNER_REFERENCE_MAX_HZ;
}

static bool output_mode_is_valid(gp100_tuner_output_mode_t mode)
{
    return (unsigned int)mode <=
           (unsigned int)GP100_TUNER_OUTPUT_BYPASS;
}

static bool model_is_valid(const gp100_tuner_model_t *model)
{
    return model != NULL && reference_is_valid(model->reference_hz) &&
           output_mode_is_valid(model->output_mode);
}

static float natural_log_near_one(float value)
{
    float y = (value - 1.0f) / (value + 1.0f);
    float y_squared = y * y;
    float term = y;
    float sum = term;

    term *= y_squared;
    sum += term / 3.0f;
    term *= y_squared;
    sum += term / 5.0f;
    term *= y_squared;
    sum += term / 7.0f;
    term *= y_squared;
    sum += term / 9.0f;
    return 2.0f * sum;
}

static int16_t rounded_cents(float cents)
{
    int value = cents < 0.0f
        ? (int)(cents - 0.5f)
        : (int)(cents + 0.5f);

    if (value < -50) {
        value = -50;
    } else if (value > 50) {
        value = 50;
    }
    return (int16_t)value;
}

static char note_letter(uint8_t note_index)
{
    static const char letters[12] = {
        'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B', 'C', 'C', 'D', 'D'
    };

    return letters[note_index % 12u];
}

static bool note_is_sharp(uint8_t note_index)
{
    uint8_t pitch_class = note_index % 12u;

    return pitch_class == 2u || pitch_class == 4u ||
           pitch_class == 6u || pitch_class == 9u ||
           pitch_class == 11u;
}

static uint8_t note_octave(uint8_t note_index)
{
    return (uint8_t)((note_index + 4u) / 12u);
}

static int8_t needle_step_from_cents(float cents)
{
    float magnitude = absolute_value(cents);
    int8_t step;

    if (magnitude < 8.0f) {
        step = 0;
    } else if (magnitude < 25.0f) {
        step = 1;
    } else if (magnitude < 45.0f) {
        step = 2;
    } else if (magnitude < 65.0f) {
        step = 3;
    } else if (magnitude < 85.0f) {
        step = 4;
    } else {
        step = 5;
    }
    return cents < 0.0f ? (int8_t)-step : step;
}

static uint8_t closest_note(
    uint16_t reference_hz,
    float frequency_hz,
    float *note_frequency)
{
    uint8_t index;
    float frequency = gp100_tuner_note_frequency(reference_hz, 0u);

    for (index = 0u; index + 1u < GP100_TUNER_NOTE_COUNT;
         ++index) {
        if (frequency_hz < frequency * HALF_SEMITONE_RATIO) {
            *note_frequency = frequency;
            return index;
        }
        frequency *= (float)SEMITONE_RATIO;
    }
    *note_frequency = frequency;
    return GP100_TUNER_NOTE_COUNT - 1u;
}

static uint8_t update_stable_note(
    gp100_tuner_model_t *model,
    uint8_t current_note,
    bool factory_window)
{
    uint8_t votes[GP100_TUNER_NOTE_COUNT] = {0};
    size_t index;
    uint8_t best_note = current_note;
    uint8_t best_votes = 0u;
    size_t window = factory_window ? GP100_TUNER_HISTORY_LENGTH
                                   : model->history_count;

    for (index = 0u; index < window; ++index) {
        ++votes[model->note_history[index]];
    }
    for (index = 0u; index < GP100_TUNER_NOTE_COUNT; ++index) {
        if (votes[index] > best_votes) {
            best_votes = votes[index];
            best_note = (uint8_t)index;
        }
    }
    if ((!factory_window && model->history_count == 1u) ||
        best_votes >= GP100_TUNER_STABLE_VOTES) {
        model->stable_note_index = best_note;
    }
    return factory_window && best_votes >= GP100_TUNER_STABLE_VOTES
        ? best_note : current_note;
}

static float average_cents(const gp100_tuner_model_t *model, bool factory_window)
{
    size_t index;
    int32_t sum = 0;

    for (index = 0u; index < model->history_count; ++index) {
        sum += model->detune_half_cents_history[index];
    }
    if (factory_window) {
        /* The factory biases each signed half-cent value before dividing. */
        int32_t half_cents = (sum + 100 * (int32_t)GP100_TUNER_HISTORY_LENGTH) /
                             (int32_t)GP100_TUNER_HISTORY_LENGTH - 100;

        return 0.5f * (float)half_cents;
    }
    return 0.5f * (float)sum / (float)model->history_count;
}

static void update_needle(gp100_tuner_model_t *model, float cents, bool factory_window)
{
    float units_per_cent = factory_window ? 2.0f : 1.0f;
    float change_threshold = absolute_value(cents * units_per_cent) < 8.0f ? 5.0f : 10.0f;

    if ((!factory_window && !model->needle_initialized) ||
        absolute_value(model->needle_cents - cents) * units_per_cent >= change_threshold) {
        model->needle_cents = cents;
    }
    model->needle_initialized = true;
    model->reading.needle_step = needle_step_from_cents(model->needle_cents * units_per_cent);
}

static void update_reading(
    gp100_tuner_model_t *model,
    uint8_t current_note,
    int16_t half_cents,
    bool factory_window,
    gp100_tuner_reading_t *reading)
{
    size_t slot = model->history_position;
    uint8_t octave_note;

    model->note_history[slot] = current_note;
    model->detune_half_cents_history[slot] = half_cents;
    model->history_position = (slot + 1u) % GP100_TUNER_HISTORY_LENGTH;
    if (model->history_count < GP100_TUNER_HISTORY_LENGTH) {
        ++model->history_count;
    }
    octave_note = update_stable_note(model, current_note, factory_window);
    model->reading.valid = true;
    model->reading.current_note_index = current_note;
    model->reading.stable_note_index = model->stable_note_index;
    model->reading.note_letter = note_letter(model->stable_note_index);
    model->reading.sharp = note_is_sharp(model->stable_note_index);
    model->reading.octave = note_octave(octave_note);
    model->reading.cents = average_cents(model, factory_window);
    update_needle(model, model->reading.cents, factory_window);
    if (reading != NULL) {
        *reading = model->reading;
    }
}

void gp100_tuner_reset(gp100_tuner_model_t *model)
{
    if (model == NULL) {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->reference_hz = GP100_TUNER_REFERENCE_DEFAULT_HZ;
    model->output_mode = GP100_TUNER_OUTPUT_MUTE;
}

bool gp100_tuner_set_reference(
    gp100_tuner_model_t *model,
    uint16_t reference_hz)
{
    if (model == NULL || !reference_is_valid(reference_hz)) {
        return false;
    }
    model->reference_hz = reference_hz;
    return true;
}

bool gp100_tuner_set_output_mode(
    gp100_tuner_model_t *model,
    gp100_tuner_output_mode_t mode)
{
    if (model == NULL || !output_mode_is_valid(mode)) {
        return false;
    }
    model->output_mode = mode;
    return true;
}

gp100_tuner_audio_source_t gp100_tuner_audio_source(
    gp100_tuner_output_mode_t mode)
{
    switch (mode) {
    case GP100_TUNER_OUTPUT_BYPASS:
        return GP100_TUNER_AUDIO_DRY;
    case GP100_TUNER_OUTPUT_THRU:
        return GP100_TUNER_AUDIO_EFFECTS;
    case GP100_TUNER_OUTPUT_MUTE:
    default:
        return GP100_TUNER_AUDIO_SILENCE;
    }
}

static double note_frequency(uint16_t reference_hz, uint8_t note_index)
{
    double frequency = (double)reference_hz;
    uint8_t index;

    if (note_index < A4_NOTE_INDEX) {
        for (index = note_index; index < A4_NOTE_INDEX; ++index) {
            frequency /= SEMITONE_RATIO;
        }
    } else {
        for (index = A4_NOTE_INDEX; index < note_index; ++index) {
            frequency *= SEMITONE_RATIO;
        }
    }
    return frequency;
}

float gp100_tuner_note_frequency(
    uint16_t reference_hz,
    uint8_t note_index)
{
    if (!reference_is_valid(reference_hz) ||
        note_index >= GP100_TUNER_NOTE_COUNT) {
        return 0.0f;
    }
    return (float)note_frequency(reference_hz, note_index);
}

static uint32_t note_period(double frequency)
{
    return (uint32_t)((double)GP100_TUNER_FACTORY_PERIOD_CLOCK_HZ /
                      frequency + 0.5);
}

static bool factory_period_in_range(uint16_t reference_hz, uint32_t period)
{
    return period <= note_period(note_frequency(reference_hz, 0u)) &&
           period >= note_period(note_frequency(reference_hz, GP100_TUNER_NOTE_COUNT - 1u));
}

static uint8_t closest_factory_note(
    uint16_t reference_hz, uint32_t period, uint32_t *selected_period)
{
    double frequency = note_frequency(reference_hz, 0u);
    uint32_t higher_period = note_period(frequency);
    uint8_t index;

    for (index = 1u; index < GP100_TUNER_NOTE_COUNT; ++index) {
        uint32_t lower_period;

        frequency *= SEMITONE_RATIO;
        lower_period = note_period(frequency);
        if (period > lower_period || index == GP100_TUNER_NOTE_COUNT - 1u) {
            uint32_t midpoint = (higher_period + lower_period) / 2u;

            if (period < midpoint) {
                *selected_period = lower_period;
                return index;
            }
            break;
        }
        higher_period = lower_period;
    }
    *selected_period = higher_period;
    return index - 1u;
}

static int16_t factory_half_cents(uint32_t period, uint32_t target)
{
    const size_t count = sizeof(factory_detune_thresholds) / sizeof(factory_detune_thresholds[0]);
    float ratio = unsigned_similarity(period, target);
    size_t index;

    for (index = 0u; index + 1u < count; ++index) {
        if (ratio >= factory_detune_thresholds[index]) {
            break;
        }
    }
    return period > target ? -(int16_t)(index + 1u) : (int16_t)(index + 1u);
}

float gp100_tuner_frequency_from_factory_period(
    uint32_t normalized_period)
{
    if (normalized_period == 0u) {
        return 0.0f;
    }
    return (float)GP100_TUNER_FACTORY_PERIOD_CLOCK_HZ /
           (float)normalized_period;
}

bool gp100_tuner_update_factory_period(
    gp100_tuner_model_t *model,
    uint32_t normalized_period,
    gp100_tuner_reading_t *reading)
{
    uint32_t target;
    uint8_t note;

    if (!model_is_valid(model) ||
        !factory_period_in_range(model->reference_hz, normalized_period)) {
        return false;
    }
    note = closest_factory_note(model->reference_hz, normalized_period, &target);
    update_reading(model, note, factory_half_cents(normalized_period, target), true, reading);
    return true;
}

bool gp100_tuner_reduce_factory_intervals(
    const uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    gp100_tuner_period_result_t *result)
{
    uint32_t groups[4];
    uint32_t total;
    float first_ratio;
    float second_ratio;
    float third_ratio;
    size_t index;

    if (result == NULL) {
        return false;
    }
    result->normalized_period = 0u;
    result->path = GP100_TUNER_REDUCTION_NONE;
    if (intervals == NULL) {
        return false;
    }

    total = 0u;
    for (index = 0u; index < 8u; ++index) {
        if ((double)unsigned_similarity(intervals[index], intervals[index + 1u]) <
            FACTORY_SINGLE_RATIO_MINIMUM) {
            break;
        }
        total += intervals[index];
    }
    if (index == 8u) {
        return set_period_result(
            result, total >> 3, GP100_TUNER_REDUCTION_SINGLE_INTERVAL);
    }

    for (index = 0u; index < 4u; ++index) {
        groups[index] = sum_intervals(intervals, index * 2u, 2u);
    }
    first_ratio = unsigned_similarity(groups[0], groups[1]);
    second_ratio = unsigned_similarity(groups[1], groups[2]);
    third_ratio = unsigned_similarity(groups[2], groups[3]);
    if (first_ratio > FACTORY_GROUP_RATIO_MINIMUM &&
        second_ratio > FACTORY_GROUP_RATIO_MINIMUM &&
        third_ratio > FACTORY_GROUP_RATIO_MINIMUM) {
        total = groups[0] + groups[1] + groups[2] + groups[3];
        return set_period_result(
            result, total >> 2, GP100_TUNER_REDUCTION_TWO_INTERVAL);
    }

    groups[0] = intervals[0];
    groups[1] = intervals[1] + intervals[2];
    groups[2] = intervals[3];
    groups[3] = intervals[4] + intervals[5];
    if (groups[0] >= FACTORY_THREE_GROUP_BAND_LOW) {
        first_ratio = unsigned_similarity(groups[0], groups[1]);
        second_ratio = unsigned_similarity(groups[1], groups[2]);
        third_ratio = unsigned_similarity(groups[2], groups[3]);

        /*
         * The final comparison really is against 1.0 in the shipped control
         * flow, rather than the 0.99 group threshold used by its neighbors.
         */
        if (first_ratio > FACTORY_GROUP_RATIO_MINIMUM &&
            second_ratio > FACTORY_GROUP_RATIO_MINIMUM &&
            third_ratio < 1.0f) {
            total = groups[0] + groups[1] + groups[2] + groups[3];
            return set_period_result(
                result,
                total >> 2,
                GP100_TUNER_REDUCTION_DISPLACED_TWO_INTERVAL);
        }
    }

    for (index = 0u; index < 3u; ++index) {
        groups[index] = sum_intervals(intervals, index * 3u, 3u);
    }
    if (groups[0] < FACTORY_THREE_GROUP_BAND_LOW &&
        (intervals[0] <= intervals[1] ||
         intervals[0] <= intervals[2] ||
         intervals[3] <= intervals[4] ||
         intervals[3] <= intervals[5] ||
         intervals[6] <= intervals[7] ||
         intervals[6] <= intervals[8])) {
        return false;
    }
    first_ratio = unsigned_similarity(groups[0], groups[1]);
    second_ratio = unsigned_similarity(groups[1], groups[2]);
    if (groups[0] > FACTORY_THREE_GROUP_BAND_LOW &&
        groups[0] < FACTORY_THREE_GROUP_BAND_HIGH) {
        if ((double)first_ratio <= FACTORY_MULTI_GROUP_RATIO_MINIMUM) {
            return false;
        }
        return set_period_result(
            result,
            (groups[0] + groups[1]) >> 1,
            GP100_TUNER_REDUCTION_THREE_INTERVAL_TWO_GROUP);
    }
    if ((double)first_ratio > FACTORY_MULTI_GROUP_RATIO_MINIMUM &&
        (double)second_ratio > FACTORY_MULTI_GROUP_RATIO_MINIMUM) {
        total = groups[0] + groups[1] + groups[2];
        return set_period_result(
            result, total / 3u, GP100_TUNER_REDUCTION_THREE_INTERVAL);
    }

    for (index = 0u; index < 3u; ++index) {
        groups[index] = sum_intervals(intervals, index * 4u, 4u);
    }
    first_ratio = unsigned_similarity(groups[0], groups[1]);
    second_ratio = unsigned_similarity(groups[1], groups[2]);
    if (groups[0] > FACTORY_FOUR_GROUP_BAND_LOW &&
        groups[0] < FACTORY_FOUR_GROUP_BAND_HIGH) {
        if ((double)first_ratio <= FACTORY_MULTI_GROUP_RATIO_MINIMUM) {
            return false;
        }
        return set_period_result(
            result,
            (groups[0] + groups[1]) >> 1,
            GP100_TUNER_REDUCTION_FOUR_INTERVAL_TWO_GROUP);
    }
    if ((double)first_ratio > FACTORY_MULTI_GROUP_RATIO_MINIMUM &&
        (double)second_ratio > FACTORY_MULTI_GROUP_RATIO_MINIMUM) {
        total = groups[0] + groups[1] + groups[2];
        return set_period_result(
            result, total / 3u, GP100_TUNER_REDUCTION_FOUR_INTERVAL);
    }
    return false;
}

bool gp100_tuner_update_factory_intervals(
    gp100_tuner_model_t *model,
    const uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    gp100_tuner_reading_t *reading)
{
    gp100_tuner_period_result_t result;

    if (!model_is_valid(model) || intervals == NULL ||
        !factory_period_in_range(model->reference_hz, intervals[0]) ||
        !gp100_tuner_reduce_factory_intervals(intervals, &result)) {
        return false;
    }
    return gp100_tuner_update_factory_period(
        model, result.normalized_period, reading);
}

bool gp100_tuner_update_frequency(
    gp100_tuner_model_t *model,
    float frequency_hz,
    gp100_tuner_reading_t *reading)
{
    float lowest;
    float highest;
    float target;
    float ratio;
    float cents;
    uint8_t current_note;

    if (!model_is_valid(model) || !(frequency_hz > 0.0f)) {
        return false;
    }
    lowest = gp100_tuner_note_frequency(model->reference_hz, 0u);
    highest = gp100_tuner_note_frequency(
        model->reference_hz, GP100_TUNER_NOTE_COUNT - 1u);
    if (frequency_hz < lowest || frequency_hz > highest) {
        return false;
    }

    current_note = closest_note(model->reference_hz, frequency_hz, &target);
    ratio = frequency_hz / target;
    cents = CENTS_PER_OCTAVE * INVERSE_LN_2 *
            natural_log_near_one(ratio);

    update_reading(model, current_note, (int16_t)(2 * rounded_cents(cents)), false, reading);
    return true;
}
