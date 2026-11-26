#include "looper/looper_state.h"

#include <stdio.h>

int main(void)
{
    static const gp100_looper_state_t expected[5][4] = {
        {GP100_LOOPER_RECORDING, GP100_LOOPER_IDLE,
         GP100_LOOPER_IDLE, GP100_LOOPER_IDLE},
        {GP100_LOOPER_RECORDING, GP100_LOOPER_PLAYING,
         GP100_LOOPER_RECORDING, GP100_LOOPER_IDLE},
        {GP100_LOOPER_OVERDUBBING, GP100_LOOPER_PLAYING,
         GP100_LOOPER_STOPPED, GP100_LOOPER_IDLE},
        {GP100_LOOPER_OVERDUBBING, GP100_LOOPER_PLAYING,
         GP100_LOOPER_OVERDUBBING, GP100_LOOPER_IDLE},
        {GP100_LOOPER_STOPPED, GP100_LOOPER_PLAYING,
         GP100_LOOPER_STOPPED, GP100_LOOPER_IDLE}
    };
    unsigned state;
    unsigned event;

    for (state = 0u; state < 5u; ++state) {
        for (event = 0u; event < 4u; ++event) {
            if (gp100_looper_next_state(
                    (gp100_looper_state_t)state,
                    (gp100_looper_event_t)event) !=
                expected[state][event]) {
                fprintf(stderr, "state %u event %u mismatch\n", state, event);
                return 1;
            }
        }
    }
    if (gp100_looper_next_state(
            (gp100_looper_state_t)5,
            GP100_LOOPER_PLAY) != (gp100_looper_state_t)5 ||
        gp100_looper_next_state(
            GP100_LOOPER_PLAYING,
            (gp100_looper_event_t)4) != GP100_LOOPER_PLAYING ||
        gp100_looper_next_state(
            (gp100_looper_state_t)-1,
            GP100_LOOPER_CLEAR) != (gp100_looper_state_t)-1 ||
        gp100_looper_next_state(
            GP100_LOOPER_PLAYING,
            (gp100_looper_event_t)-1) != GP100_LOOPER_PLAYING) {
        fprintf(stderr, "invalid input was not preserved\n");
        return 2;
    }
    return 0;
}
