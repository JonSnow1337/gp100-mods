#include "midi_message.h"

#include <stddef.h>

void gp100_midi_message_reset(
    gp100_midi_message_state_t *state)
{
    if (state != NULL) {
        state->bank_select = 0u;
    }
}

uint16_t gp100_midi_program_to_preset(
    uint8_t bank_select,
    uint8_t program_number)
{
    uint16_t bank = bank_select > 1u ? 1u : bank_select;
    uint16_t program = program_number >= GP100_MIDI_PROGRAMS_PER_BANK
        ? GP100_MIDI_PROGRAMS_PER_BANK - 1u
        : program_number;

    return bank * GP100_MIDI_PROGRAMS_PER_BANK + program;
}

static bool is_gp100_sysex(
    const uint8_t *message,
    size_t message_length)
{
    return message_length >= 9u &&
           message[0] == 0xf0u &&
           message[1] == 0x21u &&
           message[2] == 0x25u &&
           message[4] == (uint8_t)'G' &&
           message[5] == (uint8_t)'P' &&
           message[6] == (uint8_t)'-' &&
           message[7] == (uint8_t)'d';
}

static void set_notifications_suppressed(
    const gp100_midi_message_hooks_t *hooks,
    bool suppressed)
{
    if (hooks != NULL &&
        hooks->set_change_notifications_suppressed != NULL) {
        hooks->set_change_notifications_suppressed(
            hooks->context, suppressed);
    }
}

gp100_midi_action_t gp100_midi_handle_message(
    gp100_midi_message_state_t *state,
    const uint8_t *message,
    size_t message_length,
    const gp100_midi_message_hooks_t *hooks)
{
    gp100_midi_action_t action = GP100_MIDI_ACTION_IGNORED;
    uint8_t kind;

    if (state == NULL || message == NULL || message_length == 0u) {
        return action;
    }

    set_notifications_suppressed(hooks, true);
    if (hooks != NULL && hooks->input_blocked != NULL &&
        hooks->input_blocked(hooks->context)) {
        action = GP100_MIDI_ACTION_BLOCKED;
        set_notifications_suppressed(hooks, false);
        return action;
    }
    if (hooks != NULL && hooks->mark_midi_activity != NULL) {
        hooks->mark_midi_activity(hooks->context);
    }

    kind = message[0] & 0xf0u;
    if (kind == 0xb0u && message_length >= 3u) {
        if (message[1] == 0u) {
            state->bank_select = message[2];
            action = GP100_MIDI_ACTION_BANK_SELECT;
        } else {
            if (hooks != NULL && hooks->control_change != NULL) {
                hooks->control_change(
                    hooks->context, message, message_length);
            }
            action = GP100_MIDI_ACTION_CONTROL_CHANGE;
        }
    } else if (kind == 0xc0u && message_length >= 2u) {
        uint16_t preset_index = gp100_midi_program_to_preset(
            state->bank_select, message[1]);

        if (hooks != NULL && hooks->recall_preset != NULL) {
            hooks->recall_preset(hooks->context, preset_index);
        }
        action = GP100_MIDI_ACTION_PRESET_RECALL;
    } else if (kind == 0xf0u && is_gp100_sysex(message, message_length)) {
        if (message[8] == 0x11u) {
            if (hooks != NULL && hooks->sysex_data != NULL) {
                hooks->sysex_data(
                    hooks->context, message, message_length);
            }
            action = GP100_MIDI_ACTION_SYSEX_DATA;
        } else {
            if (hooks != NULL && hooks->sysex_command != NULL) {
                hooks->sysex_command(
                    hooks->context, message, message_length);
            }
            action = GP100_MIDI_ACTION_SYSEX_COMMAND;
        }
    }

    set_notifications_suppressed(hooks, false);
    return action;
}
