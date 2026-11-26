#ifndef GP100_AUDIO_ENGINE_H
#define GP100_AUDIO_ENGINE_H

#include "audio_output.h"
#include "audio_transition.h"
#include "audio/effects/effect_chain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_audio_engine {
    gp100_audio_transition_t transition;
    gp100_audio_output_level_t output_level;
    gp100_cabinet_bypass_t cabinet_bypass;
} gp100_audio_engine_t;

typedef enum gp100_audio_block_result {
    GP100_AUDIO_BLOCK_INVALID = -1,
    GP100_AUDIO_BLOCK_TRANSITION_ONLY = 0,
    GP100_AUDIO_BLOCK_EFFECTS = 1
} gp100_audio_block_result_t;

void gp100_audio_engine_reset(gp100_audio_engine_t *engine);

/* Request an EFFECTS/BYPASS run-state change or a stable STOPPED state without
 * blocking foreground code. Run-state changes fade through STOPPED and start
 * the requested path on the following block. Repeating the pending request
 * leaves its fade in progress. */
bool gp100_audio_engine_request_mode(
    gp100_audio_engine_t *engine,
    gp100_audio_mode_t target_mode);

bool gp100_audio_engine_set_cabinet_bypass(
    gp100_audio_engine_t *engine,
    gp100_cabinet_bypass_t cabinet_bypass);

gp100_audio_block_result_t gp100_audio_engine_process_block(
    gp100_audio_engine_t *engine,
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    float *interleaved_stereo,
    size_t frame_count,
    float *channel_scratch,
    size_t scratch_count,
    const gp100_effect_chain_ops_t *effect_ops);

#ifdef __cplusplus
}
#endif

#endif
