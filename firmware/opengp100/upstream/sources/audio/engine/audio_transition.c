#include "audio_transition.h"

#define GP100_AUDIO_GAIN_FULL_SCALE 255u

static bool valid_target_mode(gp100_audio_mode_t mode)
{
    return mode == GP100_AUDIO_MODE_EFFECTS ||
           mode == GP100_AUDIO_MODE_BYPASS;
}

void gp100_audio_transition_reset(
    gp100_audio_transition_t *transition)
{
    if (transition != NULL) {
        transition->mode = GP100_AUDIO_MODE_BYPASS;
        transition->target_mode = GP100_AUDIO_MODE_BYPASS;
        transition->phase = GP100_AUDIO_TRANSITION_IDLE;
        transition->gain = GP100_AUDIO_GAIN_FULL_SCALE;
    }
}

void gp100_audio_transition_begin_fade_out(
    gp100_audio_transition_t *transition)
{
    if (transition != NULL) {
        transition->phase = GP100_AUDIO_TRANSITION_FADE_OUT;
    }
}

bool gp100_audio_transition_begin_fade_in(
    gp100_audio_transition_t *transition,
    gp100_audio_mode_t target_mode)
{
    if (transition == NULL || !valid_target_mode(target_mode)) {
        return false;
    }
    transition->target_mode = target_mode;
    transition->phase = GP100_AUDIO_TRANSITION_FADE_IN;
    return true;
}

bool gp100_audio_mode_processes_effects(
    gp100_audio_mode_t mode)
{
    return mode == GP100_AUDIO_MODE_EFFECTS;
}

bool gp100_audio_route_state_valid(
    gp100_audio_route_state_t route_state)
{
    return (unsigned int)route_state <=
           (unsigned int)GP100_AUDIO_ROUTE_RESERVED_5;
}

bool gp100_audio_route_core_mode(
    gp100_audio_route_state_t route_state,
    gp100_audio_mode_t *core_mode)
{
    if (core_mode == NULL) {
        return false;
    }
    switch (route_state) {
    case GP100_AUDIO_ROUTE_TUNER_MUTE:
        *core_mode = GP100_AUDIO_MODE_STOPPED;
        return true;
    case GP100_AUDIO_ROUTE_NORMAL:
        *core_mode = GP100_AUDIO_MODE_EFFECTS;
        return true;
    case GP100_AUDIO_ROUTE_TUNER_BYPASS:
        *core_mode = GP100_AUDIO_MODE_BYPASS;
        return true;
    default:
        return false;
    }
}

size_t gp100_audio_transition_apply_gain(
    gp100_audio_transition_t *transition,
    float *interleaved_stereo,
    size_t frame_count)
{
    size_t frame;

    if (transition == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        transition->gain > GP100_AUDIO_GAIN_FULL_SCALE) {
        return 0u;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        float scale = (float)transition->gain /
                      (float)GP100_AUDIO_GAIN_FULL_SCALE;
        interleaved_stereo[frame * 2u] *= scale;
        interleaved_stereo[frame * 2u + 1u] *= scale;

        if (transition->phase == GP100_AUDIO_TRANSITION_FADE_OUT) {
            if (transition->gain == 0u) {
                transition->mode = GP100_AUDIO_MODE_STOPPED;
                transition->phase = GP100_AUDIO_TRANSITION_IDLE;
                return frame + 1u;
            }
            --transition->gain;
        }
        if (transition->phase == GP100_AUDIO_TRANSITION_FADE_IN) {
            transition->mode = transition->target_mode;
            if (transition->gain >= GP100_AUDIO_GAIN_FULL_SCALE) {
                transition->phase = GP100_AUDIO_TRANSITION_IDLE;
                return frame + 1u;
            }
            ++transition->gain;
        }
    }
    return frame_count;
}
