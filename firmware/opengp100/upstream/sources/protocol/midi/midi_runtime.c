#include "midi_runtime.h"

#include <string.h>

typedef struct midi_hook_context {
    gp100_midi_runtime_t *runtime;
    gp100_midi_runtime_result_t *result;
    bool success;
} midi_hook_context_t;

static bool runtime_is_valid(const gp100_midi_runtime_t *runtime)
{
    return runtime != NULL && runtime->controls.ui != NULL &&
           runtime->controls.preset != NULL &&
           runtime->controls.settings != NULL &&
           runtime->controls.looper != NULL &&
           runtime->controls.effects != NULL && runtime->catalog != NULL &&
           runtime->chain != NULL && runtime->output != NULL &&
           runtime->load_preset != NULL;
}

void gp100_midi_runtime_reset(gp100_midi_runtime_t *runtime)
{
    if (runtime != NULL) {
        gp100_midi_message_reset(&runtime->message);
        runtime->input_blocked = false;
        runtime->change_notifications_suppressed = false;
    }
}

static void set_change_notifications_suppressed(
    void *opaque,
    bool suppressed)
{
    midi_hook_context_t *context = (midi_hook_context_t *)opaque;
    context->runtime->change_notifications_suppressed = suppressed;
}

static bool input_blocked(void *opaque)
{
    midi_hook_context_t *context = (midi_hook_context_t *)opaque;
    return context->runtime->input_blocked;
}

static void mark_midi_activity(void *opaque)
{
    midi_hook_context_t *context = (midi_hook_context_t *)opaque;
    context->result->midi_activity_observed = true;
}

static bool remember_current_preset(midi_hook_context_t *context)
{
    gp100_midi_runtime_t *runtime = context->runtime;

    if (!gp100_settings_set_i16(
            runtime->controls.settings,
            GP100_SETTINGS_CURRENT_PRESET,
            (int16_t)runtime->controls.ui->logical_preset)) {
        return false;
    }
    context->result->settings_changed = true;
    return true;
}

static void apply_control_change(
    void *opaque,
    const uint8_t *message,
    size_t message_length)
{
    midi_hook_context_t *context = (midi_hook_context_t *)opaque;
    uint16_t previous_preset = context->runtime->controls.ui->logical_preset;

    if (message_length < 3u ||
        !gp100_midi_cc_decode_and_apply(
            message[1], message[2], &context->runtime->controls,
            &context->result->control_change)) {
        context->success = false;
        return;
    }
    if ((context->result->control_change.ui_updates &
         GP100_UI_UPDATE_SETTINGS) != 0u) {
        context->result->settings_changed = true;
    }
    if ((context->result->control_change.ui_updates &
         GP100_UI_UPDATE_PRESET_LOAD) != 0u) {
        if (!gp100_preset_runtime_load_and_recall(
                context->runtime->controls.ui->logical_preset,
                context->runtime->load_preset,
                context->runtime->context,
                context->runtime->catalog,
                context->runtime->controls.settings,
                context->runtime->controls.preset,
                context->runtime->controls.ui,
                context->runtime->chain,
                context->runtime->controls.effects,
                context->runtime->output,
                &context->result->preset_recall)) {
            context->runtime->controls.ui->logical_preset = previous_preset;
            context->success = false;
        } else if (!remember_current_preset(context)) {
            context->success = false;
        }
    }
}

static void recall_preset(void *opaque, uint16_t logical_preset)
{
    midi_hook_context_t *context = (midi_hook_context_t *)opaque;
    gp100_midi_runtime_t *runtime = context->runtime;
    if (!gp100_preset_runtime_load_and_recall(
            logical_preset, runtime->load_preset, runtime->context,
            runtime->catalog, runtime->controls.settings, runtime->controls.preset,
            runtime->controls.ui, runtime->chain,
            runtime->controls.effects, runtime->output,
            &context->result->preset_recall) ||
        !remember_current_preset(context)) {
        context->success = false;
        return;
    }
}

static void apply_sysex(
    midi_hook_context_t *context,
    bool data_path,
    const uint8_t *message,
    size_t message_length)
{
    if (context->runtime->handle_sysex == NULL ||
        !context->runtime->handle_sysex(
            context->runtime->context, data_path,
            message, message_length)) {
        context->success = false;
    }
}

static void sysex_data(
    void *opaque,
    const uint8_t *message,
    size_t message_length)
{
    apply_sysex(
        (midi_hook_context_t *)opaque, true, message, message_length);
}

static void sysex_command(
    void *opaque,
    const uint8_t *message,
    size_t message_length)
{
    apply_sysex(
        (midi_hook_context_t *)opaque, false, message, message_length);
}

bool gp100_midi_runtime_handle_message(
    gp100_midi_runtime_t *runtime,
    const uint8_t *message,
    size_t message_length,
    gp100_midi_runtime_result_t *result)
{
    gp100_midi_runtime_result_t next;
    midi_hook_context_t context;
    gp100_midi_message_hooks_t hooks;

    if (!runtime_is_valid(runtime) || message == NULL ||
        message_length == 0u || result == NULL) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    context.runtime = runtime;
    context.result = &next;
    context.success = true;
    hooks.set_change_notifications_suppressed =
        set_change_notifications_suppressed;
    hooks.input_blocked = input_blocked;
    hooks.mark_midi_activity = mark_midi_activity;
    hooks.control_change = apply_control_change;
    hooks.recall_preset = recall_preset;
    hooks.sysex_data = sysex_data;
    hooks.sysex_command = sysex_command;
    hooks.context = &context;

    next.action = gp100_midi_handle_message(
        &runtime->message, message, message_length, &hooks);
    *result = next;
    return context.success;
}

bool gp100_midi_runtime_poll_stream(
    gp100_midi_runtime_t *runtime,
    gp100_midi_stream_t *stream,
    uint8_t *message_workspace,
    size_t message_capacity,
    gp100_midi_poll_result_t *poll_result,
    gp100_midi_runtime_result_t *result)
{
    size_t message_length = 0u;

    if (!runtime_is_valid(runtime) || stream == NULL ||
        message_workspace == NULL ||
        poll_result == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    *poll_result = gp100_midi_stream_poll(
        stream, message_workspace, message_capacity, &message_length);
    if (*poll_result != GP100_MIDI_POLL_MESSAGE) {
        return true;
    }
    return gp100_midi_runtime_handle_message(
        runtime, message_workspace, message_length, result);
}
