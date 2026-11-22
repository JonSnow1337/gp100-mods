#include "audio_engine.h"

#include <string.h>

static bool valid_cabinet_bypass(gp100_cabinet_bypass_t bypass)
{
    return (uint32_t)bypass <=
           (uint32_t)GP100_CABINET_BYPASS_BOTH;
}

static bool valid_run_mode(gp100_audio_mode_t mode)
{
    return mode == GP100_AUDIO_MODE_EFFECTS ||
           mode == GP100_AUDIO_MODE_BYPASS;
}

static void continue_requested_transition(
    gp100_audio_engine_t *engine)
{
    if (engine->transition.mode == GP100_AUDIO_MODE_STOPPED &&
        engine->transition.phase == GP100_AUDIO_TRANSITION_IDLE &&
        valid_run_mode(engine->transition.target_mode)) {
        (void)gp100_audio_transition_begin_fade_in(
            &engine->transition, engine->transition.target_mode);
    }
}

static void apply_transition(gp100_audio_engine_t *engine, float *samples,
                             size_t frame_count)
{
    const size_t adjusted = gp100_audio_transition_apply_gain(
        &engine->transition, samples, frame_count);

    /* A fade that reaches zero partway through a block leaves the tail
     * untouched in the ramp helper. Keep the rest of that block silent. */
    if (engine->transition.mode == GP100_AUDIO_MODE_STOPPED &&
        adjusted < frame_count) {
        memset(samples + adjusted * 2u, 0,
               (frame_count - adjusted) * 2u * sizeof(*samples));
    }
    continue_requested_transition(engine);
}

static bool valid_output_level(
    const gp100_audio_output_level_t *level)
{
    return level->target_percent <= 100u &&
           level->current_percent >= 0.0f &&
           level->current_percent <= 100.0f &&
           level->current_percent == level->current_percent;
}

void gp100_audio_engine_reset(gp100_audio_engine_t *engine)
{
    if (engine != NULL) {
        gp100_audio_transition_reset(&engine->transition);
        gp100_audio_output_level_reset(&engine->output_level);
        engine->cabinet_bypass = GP100_CABINET_BYPASS_NONE;
    }
}

bool gp100_audio_engine_request_mode(
    gp100_audio_engine_t *engine,
    gp100_audio_mode_t target_mode)
{
    if (engine == NULL ||
        (target_mode != GP100_AUDIO_MODE_STOPPED &&
         !valid_run_mode(target_mode))) {
        return false;
    }
    if (engine->transition.mode == target_mode &&
        engine->transition.phase == GP100_AUDIO_TRANSITION_IDLE) {
        engine->transition.target_mode = target_mode;
        return true;
    }
    if (engine->transition.target_mode == target_mode &&
        engine->transition.phase != GP100_AUDIO_TRANSITION_IDLE) {
        return true;
    }
    engine->transition.target_mode = target_mode;
    if (engine->transition.mode == GP100_AUDIO_MODE_STOPPED &&
        engine->transition.phase == GP100_AUDIO_TRANSITION_IDLE) {
        if (target_mode == GP100_AUDIO_MODE_STOPPED) {
            return true;
        }
        return gp100_audio_transition_begin_fade_in(
            &engine->transition, target_mode);
    }
    if (engine->transition.phase != GP100_AUDIO_TRANSITION_FADE_OUT) {
        gp100_audio_transition_begin_fade_out(&engine->transition);
    }
    return true;
}

bool gp100_audio_engine_set_cabinet_bypass(
    gp100_audio_engine_t *engine,
    gp100_cabinet_bypass_t cabinet_bypass)
{
    if (engine == NULL || !valid_cabinet_bypass(cabinet_bypass)) {
        return false;
    }
    engine->cabinet_bypass = cabinet_bypass;
    return true;
}

gp100_audio_block_result_t gp100_audio_engine_process_block(
    gp100_audio_engine_t *engine,
    const gp100_effect_chain_t *chain,
    const gp100_effect_runtime_t *runtime,
    float *interleaved_stereo,
    size_t frame_count,
    float *channel_scratch,
    size_t scratch_count,
    const gp100_effect_chain_ops_t *effect_ops)
{
    if (engine == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        engine->transition.gain > 255u ||
        !valid_cabinet_bypass(engine->cabinet_bypass) ||
        !valid_output_level(&engine->output_level)) {
        return GP100_AUDIO_BLOCK_INVALID;
    }

    if (engine->transition.mode == GP100_AUDIO_MODE_EFFECTS) {
        if (!gp100_effect_chain_process_routed_block(
                chain, runtime, engine->cabinet_bypass,
                interleaved_stereo, frame_count,
                channel_scratch, scratch_count, effect_ops)) {
            return GP100_AUDIO_BLOCK_INVALID;
        }
        apply_transition(engine, interleaved_stereo, frame_count);
        if (!gp100_audio_output_level_apply(
                &engine->output_level, interleaved_stereo, frame_count)) {
            return GP100_AUDIO_BLOCK_INVALID;
        }
        return GP100_AUDIO_BLOCK_EFFECTS;
    }

    if (engine->transition.mode == GP100_AUDIO_MODE_STOPPED ||
        engine->transition.mode == GP100_AUDIO_MODE_BYPASS) {
        apply_transition(engine, interleaved_stereo, frame_count);
        return GP100_AUDIO_BLOCK_TRANSITION_ONLY;
    }

    engine->transition.mode = GP100_AUDIO_MODE_STOPPED;
    return GP100_AUDIO_BLOCK_INVALID;
}
