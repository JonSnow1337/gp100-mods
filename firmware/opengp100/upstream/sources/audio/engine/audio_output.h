#ifndef GP100_AUDIO_OUTPUT_H
#define GP100_AUDIO_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_audio_output_level {
    float current_percent;
    uint8_t target_percent;
} gp100_audio_output_level_t;

typedef struct gp100_audio_input_level {
    float current_gain;
    int16_t target_db;
} gp100_audio_input_level_t;

typedef struct gp100_audio_volume_pedal {
    uint8_t current_gain;
    uint8_t target_percent;
} gp100_audio_volume_pedal_t;

typedef gp100_audio_volume_pedal_t gp100_audio_master_level_t;

void gp100_audio_output_level_reset(
    gp100_audio_output_level_t *level);

bool gp100_audio_output_level_set_target(
    gp100_audio_output_level_t *level,
    uint8_t target_percent);

/* Advances by one percentage point per block, then scales stereo frames. */
bool gp100_audio_output_level_apply(
    gp100_audio_output_level_t *level,
    float *interleaved_stereo,
    size_t frame_count);

void gp100_audio_volume_pedal_reset(gp100_audio_volume_pedal_t *pedal);

/* Advances by one step toward floor(255 * target_percent / 100) per block. */
bool gp100_audio_volume_pedal_apply(
    gp100_audio_volume_pedal_t *pedal,
    float *interleaved_stereo,
    size_t frame_count);

/* Applies the pedal to normalized samples through the factory's signed
 * PCM work domain, including the 32-bit multiply and truncating division. */
bool gp100_audio_volume_pedal_apply_pcm(
    gp100_audio_volume_pedal_t *pedal,
    float *interleaved_stereo,
    size_t frame_count);

void gp100_audio_master_level_reset(gp100_audio_master_level_t *level);

/* Uses the same gain step as the pedal, with a divisor of 256. */
bool gp100_audio_master_level_apply(
    gp100_audio_master_level_t *level,
    float *interleaved_stereo,
    size_t frame_count);

void gp100_audio_input_level_reset(
    gp100_audio_input_level_t *level);

bool gp100_audio_input_level_set_target(
    gp100_audio_input_level_t *level,
    int16_t target_db);

/* Recovered -20..+20 dB input control. Gain moves by 0.01 per stereo frame,
 * then the normalized signal is clamped before the effect chain. */
bool gp100_audio_input_level_apply(
    gp100_audio_input_level_t *level,
    float *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
