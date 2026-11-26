#ifndef GP100_MIDI_MESSAGE_H
#define GP100_MIDI_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_MIDI_PROGRAMS_PER_BANK 99u
#define GP100_MIDI_PRESET_COUNT 198u

typedef struct gp100_midi_message_state {
    uint8_t bank_select;
} gp100_midi_message_state_t;

typedef enum gp100_midi_action {
    GP100_MIDI_ACTION_IGNORED = 0,
    GP100_MIDI_ACTION_BLOCKED,
    GP100_MIDI_ACTION_BANK_SELECT,
    GP100_MIDI_ACTION_CONTROL_CHANGE,
    GP100_MIDI_ACTION_PRESET_RECALL,
    GP100_MIDI_ACTION_SYSEX_DATA,
    GP100_MIDI_ACTION_SYSEX_COMMAND
} gp100_midi_action_t;

typedef struct gp100_midi_message_hooks {
    void (*set_change_notifications_suppressed)(
        void *context,
        bool suppressed);
    bool (*input_blocked)(void *context);
    void (*mark_midi_activity)(void *context);
    void (*control_change)(
        void *context,
        const uint8_t *message,
        size_t message_length);
    void (*recall_preset)(void *context, uint16_t preset_index);
    void (*sysex_data)(
        void *context,
        const uint8_t *message,
        size_t message_length);
    void (*sysex_command)(
        void *context,
        const uint8_t *message,
        size_t message_length);
    void *context;
} gp100_midi_message_hooks_t;

void gp100_midi_message_reset(
    gp100_midi_message_state_t *state);

/* Safe mapping for new firmware; valid factory bank values 0 and 1 are exact. */
uint16_t gp100_midi_program_to_preset(
    uint8_t bank_select,
    uint8_t program_number);

gp100_midi_action_t gp100_midi_handle_message(
    gp100_midi_message_state_t *state,
    const uint8_t *message,
    size_t message_length,
    const gp100_midi_message_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
