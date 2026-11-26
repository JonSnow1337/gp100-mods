#include "protocol/midi/midi_message.h"

#include <stdio.h>
#include <string.h>

typedef struct hook_log {
    char order[16];
    size_t order_length;
    bool blocked;
    unsigned control_calls;
    unsigned recall_calls;
    unsigned data_calls;
    unsigned command_calls;
    uint16_t preset_index;
} hook_log_t;

static void log_char(hook_log_t *log, char value)
{
    if (log->order_length < sizeof(log->order)) {
        log->order[log->order_length++] = value;
    }
}

static void set_change_notifications_suppressed(
    void *context,
    bool suppressed)
{
    log_char((hook_log_t *)context, suppressed ? 'S' : 's');
}

static bool input_blocked(void *context)
{
    hook_log_t *log = (hook_log_t *)context;

    log_char(log, '?');
    return log->blocked;
}

static void mark_midi_activity(void *context)
{
    log_char((hook_log_t *)context, 'A');
}

static void control_change(
    void *context,
    const uint8_t *message,
    size_t message_length)
{
    hook_log_t *log = (hook_log_t *)context;

    (void)message;
    (void)message_length;
    ++log->control_calls;
    log_char(log, 'C');
}

static void recall_preset(void *context, uint16_t preset_index)
{
    hook_log_t *log = (hook_log_t *)context;

    ++log->recall_calls;
    log->preset_index = preset_index;
    log_char(log, 'P');
}

static void sysex_data(
    void *context,
    const uint8_t *message,
    size_t message_length)
{
    hook_log_t *log = (hook_log_t *)context;

    (void)message;
    (void)message_length;
    ++log->data_calls;
    log_char(log, 'D');
}

static void sysex_command(
    void *context,
    const uint8_t *message,
    size_t message_length)
{
    hook_log_t *log = (hook_log_t *)context;

    (void)message;
    (void)message_length;
    ++log->command_calls;
    log_char(log, 'S');
}

static gp100_midi_message_hooks_t hooks_for(hook_log_t *log)
{
    gp100_midi_message_hooks_t hooks = {
        set_change_notifications_suppressed,
        input_blocked,
        mark_midi_activity,
        control_change,
        recall_preset,
        sysex_data,
        sysex_command,
        log
    };

    return hooks;
}

static int test_bank_and_program(void)
{
    gp100_midi_message_state_t state;
    hook_log_t log = {{0}, 0u, false, 0u, 0u, 0u, 0u, 0u};
    gp100_midi_message_hooks_t hooks = hooks_for(&log);
    const uint8_t bank[] = {0xb3u, 0u, 1u};
    const uint8_t program[] = {0xc3u, 120u};

    gp100_midi_message_reset(&state);
    if (gp100_midi_handle_message(
            &state, bank, sizeof(bank), &hooks) !=
            GP100_MIDI_ACTION_BANK_SELECT ||
        state.bank_select != 1u ||
        gp100_midi_handle_message(
            &state, program, sizeof(program), &hooks) !=
            GP100_MIDI_ACTION_PRESET_RECALL ||
        log.recall_calls != 1u || log.preset_index != 197u ||
        memcmp(log.order, "S?AsS?APs", 9u) != 0) {
        return 1;
    }
    if (gp100_midi_program_to_preset(9u, 0u) != 99u ||
        gp100_midi_program_to_preset(0u, 127u) != 98u) {
        return 2;
    }
    return 0;
}

static int test_control_and_block(void)
{
    gp100_midi_message_state_t state;
    hook_log_t log = {{0}, 0u, false, 0u, 0u, 0u, 0u, 0u};
    gp100_midi_message_hooks_t hooks = hooks_for(&log);
    const uint8_t control[] = {0xb0u, 7u, 100u};

    gp100_midi_message_reset(&state);
    if (gp100_midi_handle_message(
            &state, control, sizeof(control), &hooks) !=
            GP100_MIDI_ACTION_CONTROL_CHANGE ||
        log.control_calls != 1u ||
        memcmp(log.order, "S?ACs", 5u) != 0) {
        return 1;
    }
    log.blocked = true;
    log.order_length = 0u;
    if (gp100_midi_handle_message(
            &state, control, sizeof(control), &hooks) !=
            GP100_MIDI_ACTION_BLOCKED ||
        log.control_calls != 1u ||
        memcmp(log.order, "S?s", 3u) != 0) {
        return 2;
    }
    return 0;
}

static int test_sysex_dispatch(void)
{
    gp100_midi_message_state_t state;
    hook_log_t log = {{0}, 0u, false, 0u, 0u, 0u, 0u, 0u};
    gp100_midi_message_hooks_t hooks = hooks_for(&log);
    uint8_t message[] = {
        0xf0u, 0x21u, 0x25u, 0x01u,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        0x11u, 0xf7u
    };

    gp100_midi_message_reset(&state);
    if (gp100_midi_handle_message(
            &state, message, sizeof(message), &hooks) !=
            GP100_MIDI_ACTION_SYSEX_DATA ||
        log.data_calls != 1u || log.command_calls != 0u) {
        return 1;
    }
    message[8] = 0x22u;
    if (gp100_midi_handle_message(
            &state, message, sizeof(message), &hooks) !=
            GP100_MIDI_ACTION_SYSEX_COMMAND ||
        log.data_calls != 1u || log.command_calls != 1u) {
        return 2;
    }
    message[7] = (uint8_t)'x';
    if (gp100_midi_handle_message(
            &state, message, sizeof(message), &hooks) !=
            GP100_MIDI_ACTION_IGNORED) {
        return 3;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_midi_message_state_t state;
    hook_log_t log = {{0}, 0u, false, 0u, 0u, 0u, 0u, 0u};
    gp100_midi_message_hooks_t hooks = hooks_for(&log);
    const uint8_t short_control[] = {0xb0u, 7u};

    gp100_midi_message_reset(&state);
    if (gp100_midi_handle_message(
            &state, short_control, sizeof(short_control), &hooks) !=
            GP100_MIDI_ACTION_IGNORED ||
        gp100_midi_handle_message(
            NULL, short_control, sizeof(short_control), &hooks) !=
            GP100_MIDI_ACTION_IGNORED) {
        return 1;
    }
    return 0;
}

static int test_length_framed_meta_message_is_ignored(void)
{
    gp100_midi_message_state_t state;
    hook_log_t log = {{0}, 0u, false, 0u, 0u, 0u, 0u, 0u};
    gp100_midi_message_hooks_t hooks = hooks_for(&log);
    const uint8_t message[] = {0xffu, 0x2fu, 0u};

    gp100_midi_message_reset(&state);
    if (gp100_midi_handle_message(
            &state, message, sizeof(message), &hooks) !=
            GP100_MIDI_ACTION_IGNORED ||
        memcmp(log.order, "S?As", 4u) != 0 ||
        log.control_calls != 0u || log.recall_calls != 0u ||
        log.data_calls != 0u || log.command_calls != 0u) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_bank_and_program();
    if (result != 0) {
        fprintf(stderr, "bank/program test failed: %d\n", result);
        return 10 + result;
    }
    result = test_control_and_block();
    if (result != 0) {
        fprintf(stderr, "control/block test failed: %d\n", result);
        return 20 + result;
    }
    result = test_sysex_dispatch();
    if (result != 0) {
        fprintf(stderr, "SysEx dispatch test failed: %d\n", result);
        return 30 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 40 + result;
    }
    result = test_length_framed_meta_message_is_ignored();
    if (result != 0) {
        fprintf(stderr, "length-framed meta-message test failed: %d\n",
                result);
        return 50 + result;
    }
    return 0;
}
