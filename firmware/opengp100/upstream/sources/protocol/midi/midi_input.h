#ifndef GP100_MIDI_INPUT_H
#define GP100_MIDI_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define GP100_MIDI_INPUT_CAPACITY 512u

/* Start/stop interrupt-driven receive on the already configured LPUART1. */
bool GP100_MidiInputStart(void);
void GP100_MidiInputStop(void);
bool GP100_MidiInputIsRunning(void);

/* Pop one byte in foreground context. */
bool GP100_MidiInputPop(uint8_t *byte);

/* Push one byte from another interrupt-driven transport such as USB-MIDI. */
bool GP100_MidiInputPush(uint8_t byte);

uint32_t GP100_MidiInputReceived(void);
uint32_t GP100_MidiInputDropped(void);
uint32_t GP100_MidiInputErrors(void);

#endif
