#ifndef GP100_LOOPER_STATE_H
#define GP100_LOOPER_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_looper_event {
    GP100_LOOPER_RECORD_OR_OVERDUB = 0,
    GP100_LOOPER_PLAY,
    GP100_LOOPER_STOP,
    GP100_LOOPER_CLEAR
} gp100_looper_event_t;

typedef enum gp100_looper_state {
    GP100_LOOPER_IDLE = 0,
    GP100_LOOPER_RECORDING,
    GP100_LOOPER_PLAYING,
    GP100_LOOPER_OVERDUBBING,
    GP100_LOOPER_STOPPED
} gp100_looper_state_t;

/* Shared factory transition graph used by physical and MIDI controls. */
gp100_looper_state_t gp100_looper_next_state(
    gp100_looper_state_t state,
    gp100_looper_event_t event);

#ifdef __cplusplus
}
#endif

#endif
