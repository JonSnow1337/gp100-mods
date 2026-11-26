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

/* Optional finer MASTER curve (feature flag). The stock curve is linear in amplitude (gain = 255 * percent / 100), so the
 * knob is very coarse at the bottom: percent 1/2/3 are already -48/-42/-38 dB. Here gain = p*p*(p+60) / 32, 0..50000, i.e.
 * percent 20 is about as loud as stock 2 (-34 dB), 40 as stock 10 (-20 dB); percent 100 is unchanged (full scale). */
#define GP100_AUDIO_MASTER_FINE_MAX 50000u

typedef struct gp100_audio_master_fine {
    uint16_t current_gain;      /* 0..GP100_AUDIO_MASTER_FINE_MAX, moves by one ramp step per block */
    uint8_t target_percent;     /* MASTER 0..100 */
} gp100_audio_master_fine_t;

/* Capability marker: the gp100-mods app looks for this text in a bundled firmware to know it has the fine-MASTER switch. */
extern const char gp100_fine_master_marker[];

uint32_t gp100_audio_master_fine_gain(uint8_t percent);
void gp100_audio_master_fine_reset(gp100_audio_master_fine_t *level);
bool gp100_audio_master_fine_apply(
    gp100_audio_master_fine_t *level,
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
