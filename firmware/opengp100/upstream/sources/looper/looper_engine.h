#ifndef GP100_LOOPER_ENGINE_H
#define GP100_LOOPER_ENGINE_H

#include "looper_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 91 seconds of mono audio or 45.5 seconds of interleaved stereo at 44.1 kHz. */
#define GP100_LOOPER_FACTORY_STORAGE_SAMPLES 4013100u
#define GP100_LOOPER_FACTORY_STORAGE_ADDRESS UINT32_C(0x80eab110)
#define GP100_LOOPER_MAX_LEVEL 100u
#define GP100_LOOPER_SAMPLE_LIMIT 8388500
#define GP100_LOOPER_FACTORY_BLOCK_FRAMES 64u
#define GP100_LOOPER_MONO_BACKUP_SAMPLES (340u * GP100_LOOPER_FACTORY_BLOCK_FRAMES)
#define GP100_LOOPER_TRANSITION_FRAMES 256u
#define GP100_LOOPER_SEAM_FRAMES 128u

typedef enum gp100_looper_position {
    GP100_LOOPER_POST_EFFECTS = 0,
    GP100_LOOPER_PRE_EFFECTS = 1
} gp100_looper_position_t;

typedef struct gp100_looper_engine {
    int32_t *storage;
    size_t storage_capacity_samples;
    size_t recorded_samples;
    size_t playback_position;
    gp100_looper_position_t position;
    gp100_looper_state_t state;
    uint8_t record_level;
    uint8_t record_level_target;
    uint8_t playback_level;
    uint8_t playback_level_target;
    uint16_t playback_gain_q8;
    uint16_t playback_gain_target_q8;
    bool fade_out_active;
    bool last_callback_was_factory_block;
    bool recording_from_factory_blocks;
    uint8_t recording_preroll_blocks;
    uint8_t recording_exit_blocks;
    uint8_t playback_entry_blocks;
    uint8_t wrap_transition_blocks;
    uint8_t playback_mode_switch_blocks;
    gp100_looper_state_t playback_mode_before_switch;
    size_t clear_tail_start;
    uint8_t clear_tail_frames;
    uint8_t clear_blocks;
    int32_t *mono_overdub_backup;
    size_t mono_overdub_backup_samples;
    size_t mono_overdub_backup_start;
    uint16_t seam_frames_remaining;
    int32_t seam_from_left;
    int32_t seam_from_right;
    uint32_t sample_rate_hz;
    uint16_t tempo_bpm;
    bool beat_extension_enabled;
    size_t beat_extension_samples;
} gp100_looper_engine_t;

typedef struct gp100_looper_process_result {
    size_t frames_processed;
    bool loop_wrapped;
    bool capacity_reached;
    bool state_changed;
    bool seam_smoothed;
} gp100_looper_process_result_t;

bool gp100_looper_init(
    gp100_looper_engine_t *engine,
    int32_t *storage,
    size_t storage_capacity_samples,
    gp100_looper_position_t position,
    uint8_t record_level,
    uint8_t playback_level);

bool gp100_looper_set_position(
    gp100_looper_engine_t *engine,
    gp100_looper_position_t position);

/* Supply disjoint caller-owned scratch for the fixed-block PRE overdub.
 * Pass NULL and zero to detach it. Replacing scratch discards any pending backup. */
bool gp100_looper_set_mono_overdub_backup(
    gp100_looper_engine_t *engine,
    int32_t *scratch,
    size_t scratch_samples);

bool gp100_looper_set_levels(
    gp100_looper_engine_t *engine,
    uint8_t record_level,
    uint8_t playback_level);

/* Beat extension uses this tempo when enabled by the caller's policy. A zero
 * BPM always disables it. The portable engine starts with the policy enabled. */
bool gp100_looper_set_tempo(
    gp100_looper_engine_t *engine,
    uint32_t sample_rate_hz,
    uint16_t bpm);

bool gp100_looper_handle_event(
    gp100_looper_engine_t *engine,
    gp100_looper_event_t event,
    bool *changed);

bool gp100_looper_process(
    gp100_looper_engine_t *engine,
    int32_t *interleaved_stereo,
    size_t frame_count,
    gp100_looper_process_result_t *result);

/* Process one factory-sized callback with the recovered recording fades and
 * block-aligned forward playback restart. The input retains its dry signal. */
bool gp100_looper_process_factory_block(
    gp100_looper_engine_t *engine,
    int32_t interleaved_stereo[GP100_LOOPER_FACTORY_BLOCK_FRAMES * 2u],
    gp100_looper_process_result_t *result);

double gp100_looper_capacity_seconds(
    const gp100_looper_engine_t *engine,
    uint32_t sample_rate_hz);

#ifdef __cplusplus
}
#endif

#endif
