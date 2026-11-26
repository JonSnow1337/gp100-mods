#include "looper_state.h"

gp100_looper_state_t gp100_looper_next_state(
    gp100_looper_state_t state,
    gp100_looper_event_t event)
{
    if ((unsigned)state > (unsigned)GP100_LOOPER_STOPPED ||
        (unsigned)event > (unsigned)GP100_LOOPER_CLEAR) {
        return state;
    }
    if (event == GP100_LOOPER_CLEAR) {
        return state == GP100_LOOPER_IDLE
            ? state
            : GP100_LOOPER_IDLE;
    }
    if (event == GP100_LOOPER_RECORD_OR_OVERDUB) {
        if (state == GP100_LOOPER_IDLE) {
            return GP100_LOOPER_RECORDING;
        }
        if (state == GP100_LOOPER_PLAYING) {
            return GP100_LOOPER_OVERDUBBING;
        }
        return state;
    }
    if (event == GP100_LOOPER_PLAY) {
        if (state == GP100_LOOPER_RECORDING ||
            state == GP100_LOOPER_OVERDUBBING ||
            state == GP100_LOOPER_STOPPED) {
            return GP100_LOOPER_PLAYING;
        }
        return state;
    }
    if (event == GP100_LOOPER_STOP &&
        state == GP100_LOOPER_PLAYING) {
        return GP100_LOOPER_STOPPED;
    }
    return state;
}
