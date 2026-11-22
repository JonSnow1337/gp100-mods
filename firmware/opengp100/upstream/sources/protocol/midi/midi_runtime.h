#ifndef GP100_MIDI_RUNTIME_H
#define GP100_MIDI_RUNTIME_H

#include "midi_cc_runtime.h"
#include "midi_message.h"
#include "midi_stream.h"
#include "app/preset_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gp100_preset_loader_t gp100_midi_preset_loader_t;

typedef bool (*gp100_midi_sysex_handler_t)(
    void *context,
    bool data_path,
    const uint8_t *message,
    size_t message_length);

typedef struct gp100_midi_runtime {
    gp100_midi_message_state_t message;
    gp100_midi_cc_runtime_t controls;
    const gp100_effect_catalog_t *catalog;
    gp100_effect_chain_t *chain;
    gp100_audio_output_level_t *output;
    gp100_midi_preset_loader_t load_preset;
    gp100_midi_sysex_handler_t handle_sysex;
    void *context;
    bool input_blocked;
    bool change_notifications_suppressed;
} gp100_midi_runtime_t;

typedef struct gp100_midi_runtime_result {
    gp100_midi_action_t action;
    bool midi_activity_observed;
    bool settings_changed;
    gp100_midi_cc_apply_result_t control_change;
    gp100_preset_recall_result_t preset_recall;
} gp100_midi_runtime_result_t;

/* Reset Bank Select and the two factory-style message flags; keep wiring. */
void gp100_midi_runtime_reset(gp100_midi_runtime_t *runtime);

/*
 * Apply one complete product message. Program Change and CC24/CC25 preset
 * steps load bytes into a temporary record and publish them only after the
 * recovered recall pipeline succeeds. Successful recall stores the selected
 * logical preset in settings and reports settings_changed.
 */
bool gp100_midi_runtime_handle_message(
    gp100_midi_runtime_t *runtime,
    const uint8_t *message,
    size_t message_length,
    gp100_midi_runtime_result_t *result);

/* Poll and dispatch at most one framed message. Non-message poll results are
 * successful no-ops and are returned explicitly to the caller. */
bool gp100_midi_runtime_poll_stream(
    gp100_midi_runtime_t *runtime,
    gp100_midi_stream_t *stream,
    uint8_t *message_workspace,
    size_t message_capacity,
    gp100_midi_poll_result_t *poll_result,
    gp100_midi_runtime_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
