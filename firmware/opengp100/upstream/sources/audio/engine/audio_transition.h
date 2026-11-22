#ifndef GP100_AUDIO_TRANSITION_H
#define GP100_AUDIO_TRANSITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_audio_mode {
    GP100_AUDIO_MODE_STOPPED = 1,
    GP100_AUDIO_MODE_EFFECTS = 3,
    GP100_AUDIO_MODE_BYPASS = 4
} gp100_audio_mode_t;

/*
 * Public route state stored separately from the core run/transition mode.
 * States 0..2 have observed tuner callers; 3..5 are accepted by the factory
 * setter but are unused by every direct caller in the supplied application.
 */
typedef enum gp100_audio_route_state {
    GP100_AUDIO_ROUTE_TUNER_MUTE = 0,
    GP100_AUDIO_ROUTE_NORMAL = 1,
    GP100_AUDIO_ROUTE_TUNER_BYPASS = 2,
    GP100_AUDIO_ROUTE_RESERVED_3 = 3,
    GP100_AUDIO_ROUTE_RESERVED_4 = 4,
    GP100_AUDIO_ROUTE_RESERVED_5 = 5
} gp100_audio_route_state_t;

/* Compatibility names from the first route draft. */
#define GP100_AUDIO_ROUTE_DIRECT \
    GP100_AUDIO_ROUTE_TUNER_BYPASS
#define GP100_AUDIO_ROUTE_PROCESSED \
    GP100_AUDIO_ROUTE_NORMAL
#define GP100_AUDIO_ROUTE_AUX_ONLY \
    GP100_AUDIO_ROUTE_TUNER_MUTE

typedef enum gp100_audio_transition_phase {
    GP100_AUDIO_TRANSITION_FADE_OUT = 0,
    GP100_AUDIO_TRANSITION_FADE_IN = 2,
    GP100_AUDIO_TRANSITION_IDLE = 5
} gp100_audio_transition_phase_t;

typedef struct gp100_audio_transition {
    gp100_audio_mode_t mode;
    gp100_audio_mode_t target_mode;
    gp100_audio_transition_phase_t phase;
    uint16_t gain;
} gp100_audio_transition_t;

void gp100_audio_transition_reset(
    gp100_audio_transition_t *transition);

void gp100_audio_transition_begin_fade_out(
    gp100_audio_transition_t *transition);

bool gp100_audio_transition_begin_fade_in(
    gp100_audio_transition_t *transition,
    gp100_audio_mode_t target_mode);

bool gp100_audio_mode_processes_effects(
    gp100_audio_mode_t mode);

bool gp100_audio_route_state_valid(
    gp100_audio_route_state_t route_state);

/*
 * Returns true when the public route has an observed core-mode request and
 * writes it to core_mode. Reserved states are valid route values but return
 * false because the factory leaves the core mode unchanged for them.
 */
bool gp100_audio_route_core_mode(
    gp100_audio_route_state_t route_state,
    gp100_audio_mode_t *core_mode);

/*
 * Applies the factory 0..255 stereo ramp in place. The return value is the
 * number of frames touched; factory behavior returns early on the terminal
 * zero/full-scale frame, so this can be less than frame_count.
 */
size_t gp100_audio_transition_apply_gain(
    gp100_audio_transition_t *transition,
    float *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
