#ifndef GP100_BOARD_CONTROL_INPUT_H
#define GP100_BOARD_CONTROL_INPUT_H

#include "app/event_dispatch.h"

#include <stdbool.h>
#include <stdint.h>

#define GP100_CONTROL_SCAN_PERIOD_US 200u
#define GP100_CONTROL_ENCODER_SCAN_HZ 5000u
#define GP100_CONTROL_BUTTON_SCAN_HZ 1000u

/* Configure the recovered QTMR2/GPIO control scanner. The timer and interrupt
 * remain stopped until GP100_ControlInputStart() is called explicitly. */
void GP100_ControlInputInit(void);

bool GP100_ControlInputStart(void);
void GP100_ControlInputStop(void);
bool GP100_ControlInputIsRunning(void);

/* Pop one event emitted by the reference button/encoder state machines.
 * The ISR uses the safe 499-entry queue policy, avoiding the factory queue's
 * full-versus-empty ambiguity. */
bool GP100_ControlInputPop(gp100_event_t *event);

/* Number of events rejected because the foreground queue was full. */
uint32_t GP100_ControlInputDroppedEvents(void);

#endif
