#ifndef GP100_TUNER_MODEL_H
#define GP100_TUNER_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_TUNER_REFERENCE_MIN_HZ 435u
#define GP100_TUNER_REFERENCE_MAX_HZ 445u
#define GP100_TUNER_REFERENCE_DEFAULT_HZ 440u
#define GP100_TUNER_NOTE_COUNT 93u
#define GP100_TUNER_HISTORY_LENGTH 64u
#define GP100_TUNER_STABLE_VOTES 12u
#define GP100_TUNER_FACTORY_PERIOD_CLOCK_HZ 48000000u
#define GP100_TUNER_FACTORY_INTERVAL_COUNT 12u

typedef enum gp100_tuner_output_mode {
    GP100_TUNER_OUTPUT_MUTE = 0,
    GP100_TUNER_OUTPUT_THRU = 1,
    GP100_TUNER_OUTPUT_BYPASS = 2
} gp100_tuner_output_mode_t;

typedef enum gp100_tuner_audio_source {
    GP100_TUNER_AUDIO_DRY,
    GP100_TUNER_AUDIO_EFFECTS,
    GP100_TUNER_AUDIO_SILENCE
} gp100_tuner_audio_source_t;

typedef struct gp100_tuner_reading {
    bool valid;
    uint8_t current_note_index;
    uint8_t stable_note_index;
    char note_letter;
    bool sharp;
    uint8_t octave;
    float cents; /* Signed musical cents. */
    int8_t needle_step;
} gp100_tuner_reading_t;

typedef enum gp100_tuner_reduction_path {
    GP100_TUNER_REDUCTION_NONE = 0,
    GP100_TUNER_REDUCTION_SINGLE_INTERVAL,
    GP100_TUNER_REDUCTION_TWO_INTERVAL,
    GP100_TUNER_REDUCTION_DISPLACED_TWO_INTERVAL,
    GP100_TUNER_REDUCTION_THREE_INTERVAL_TWO_GROUP,
    GP100_TUNER_REDUCTION_THREE_INTERVAL,
    GP100_TUNER_REDUCTION_FOUR_INTERVAL_TWO_GROUP,
    GP100_TUNER_REDUCTION_FOUR_INTERVAL
} gp100_tuner_reduction_path_t;

typedef struct gp100_tuner_period_result {
    uint32_t normalized_period;
    gp100_tuner_reduction_path_t path;
} gp100_tuner_period_result_t;

typedef struct gp100_tuner_model {
    uint16_t reference_hz;
    gp100_tuner_output_mode_t output_mode;
    uint8_t note_history[GP100_TUNER_HISTORY_LENGTH];
    int16_t detune_half_cents_history[GP100_TUNER_HISTORY_LENGTH];
    size_t history_position;
    size_t history_count;
    uint8_t stable_note_index;
    float needle_cents;
    bool needle_initialized;
    gp100_tuner_reading_t reading;
} gp100_tuner_model_t;

void gp100_tuner_reset(gp100_tuner_model_t *model);

bool gp100_tuner_set_reference(
    gp100_tuner_model_t *model,
    uint16_t reference_hz);

bool gp100_tuner_set_output_mode(
    gp100_tuner_model_t *model,
    gp100_tuner_output_mode_t mode);

gp100_tuner_audio_source_t gp100_tuner_audio_source(
    gp100_tuner_output_mode_t mode);

/*
 * Convert one detector result into the product's note/cents display model.
 * Returns false for a null model, NaN, or a frequency outside E0..C8.
 */
bool gp100_tuner_update_frequency(
    gp100_tuner_model_t *model,
    float frequency_hz,
    gp100_tuner_reading_t *reading);

/* Convert the factory reducer's normalized 48 MHz period to hertz. */
float gp100_tuner_frequency_from_factory_period(
    uint32_t normalized_period);

/* Select a factory note and update its 64-sample display history. */
bool gp100_tuner_update_factory_period(
    gp100_tuner_model_t *model,
    uint32_t normalized_period,
    gp100_tuner_reading_t *reading);

/* Reproduce the factory's twelve-interval grouping and rejection cascade. */
bool gp100_tuner_reduce_factory_intervals(
    const uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    gp100_tuner_period_result_t *result);

/* Reduce a factory capture window and feed it into the portable display model. */
bool gp100_tuner_update_factory_intervals(
    gp100_tuner_model_t *model,
    const uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    gp100_tuner_reading_t *reading);

/* Reference-frequency table entry: index 0 is E0, 53 is A4, 92 is C8. */
float gp100_tuner_note_frequency(
    uint16_t reference_hz,
    uint8_t note_index);

#ifdef __cplusplus
}
#endif

#endif
