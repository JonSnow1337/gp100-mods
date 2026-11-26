#ifndef GP100_RHYTHM_CLOCK_H
#define GP100_RHYTHM_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "rhythm_player.h"

#define GP100_RHYTHM_INITIAL_PERIOD_US 500u

/* Configure the recovered QTMR4 channel-0 cadence source. Initialization is
 * deliberately inert; call GP100_RhythmClockStart() when the sequencer is
 * ready to consume pending ticks. */
void GP100_RhythmClockInit(void);

bool GP100_RhythmClockStart(void);
void GP100_RhythmClockStop(void);
bool GP100_RhythmClockIsRunning(void);

/* Retune to 120 sequencer ticks per quarter note within the drum tempo range. */
bool GP100_RhythmClockSetTempoBpm(uint16_t tempoBpm);
uint16_t GP100_RhythmClockTempoBpm(void);

/* Atomically return and clear the interrupt backlog. Foreground sequencer code
 * should advance once per returned tick so late service catches up exactly as
 * the factory implementation does. */
uint32_t GP100_RhythmClockDrainPending(void);
uint32_t GP100_RhythmClockPending(void);
uint32_t GP100_RhythmClockTimerHz(void);

/* Fill the callback table used by gp100_rhythm_player_init(). */
void GP100_RhythmClockGetPlayerClock(gp100_rhythm_clock_t *clock);

#endif
