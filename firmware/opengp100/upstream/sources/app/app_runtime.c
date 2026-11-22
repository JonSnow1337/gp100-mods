#include "app_runtime.h"

#include <stddef.h>

static const gp100_init_stage_t k_initial_stages[] = {
    GP100_INIT_CONFIGURE_MPU,
    GP100_INIT_BOARD_IOMUX,
    GP100_INIT_LPI2C1_PINS,
    GP100_INIT_BOOT_CLOCKS,
    GP100_INIT_SYSTEM_STATE_AND_SYSTICK,
    GP100_INIT_SAI1_TX_SYNC_PIN,
    GP100_INIT_BUTTON_GPIO_INPUTS,
    GP100_INIT_ENCODER_GPIO_INPUTS,
    GP100_INIT_ANALOG_CONTROL_ADC_DMA,
    GP100_INIT_TUNER_EDGE_INPUT,
    GP100_INIT_DISPLAY_CONTROLLER_AND_FRAMEBUFFER,
    GP100_INIT_MEMORY_BUFFER_LAYOUT
};

static const gp100_init_stage_t k_final_stages[] = {
    GP100_INIT_CLEAR_WORK_AREAS,
    GP100_INIT_USB_AUDIO_MIDI,
    GP100_INIT_SELECT_STARTUP_UI_MODE,
    GP100_INIT_VERIFY_DEVICE_RECORD
};

bool gp100_app_hooks_are_valid(const gp100_app_hooks_t *hooks)
{
    return hooks != NULL &&
           hooks->run_initialization_stage != NULL &&
           hooks->initialize_tuner_counter != NULL &&
           hooks->configure_periodic_timer != NULL &&
           hooks->poll_control_events != NULL &&
           hooks->parse_midi_input != NULL &&
           hooks->parse_midi_transport != NULL &&
           hooks->dispatch_ui_event != NULL &&
           hooks->flush_display != NULL &&
           hooks->flush_settings != NULL &&
           hooks->set_tmr2_irq_enabled != NULL &&
           hooks->data_synchronization_barrier != NULL &&
           hooks->instruction_synchronization_barrier != NULL &&
           hooks->tick_maintenance != NULL;
}

bool gp100_app_initialize(gp100_app_state_t *state,
                               const gp100_app_hooks_t *hooks)
{
    size_t index;

    if (state == NULL || !gp100_app_hooks_are_valid(hooks)) {
        return false;
    }

    state->system_ready = false;

    for (index = 0u;
         index < sizeof(k_initial_stages) / sizeof(k_initial_stages[0]);
         ++index) {
        hooks->run_initialization_stage(hooks->context,
                                        k_initial_stages[index]);
    }

    state->tuner_counter_clock_hz =
        hooks->initialize_tuner_counter(hooks->context);
    hooks->configure_periodic_timer(hooks->context,
                                    GP100_PERIODIC_TIMER_CONTROL_SCAN,
                                    200u);
    hooks->configure_periodic_timer(hooks->context,
                                    GP100_PERIODIC_TIMER_RHYTHM_SEQUENCER,
                                    500u);

    for (index = 0u;
         index < sizeof(k_final_stages) / sizeof(k_final_stages[0]);
         ++index) {
        hooks->run_initialization_stage(hooks->context,
                                        k_final_stages[index]);
    }

    state->system_ready = true;
    return true;
}

gp100_step_result_t gp100_app_service_once(
    gp100_app_state_t *state,
    const gp100_app_hooks_t *hooks)
{
    if (state == NULL || !gp100_app_hooks_are_valid(hooks)) {
        return GP100_STEP_INVALID;
    }

    hooks->poll_control_events(hooks->context);
    hooks->parse_midi_input(hooks->context);
    hooks->parse_midi_transport(hooks->context);
    hooks->dispatch_ui_event(hooks->context);

    if (state->foreground_ticks_pending == 0u) {
        return GP100_STEP_IDLE;
    }

    hooks->flush_display(hooks->context);
    hooks->flush_settings(hooks->context);

    /* V2.1.0 masks external IRQ 134, which is TMR2 on MIMXRT1052. */
    hooks->set_tmr2_irq_enabled(hooks->context, false);
    hooks->data_synchronization_barrier(hooks->context);
    hooks->instruction_synchronization_barrier(hooks->context);
    state->foreground_ticks_pending = 0u;
    hooks->set_tmr2_irq_enabled(hooks->context, true);

    return GP100_STEP_TICK_HANDLED;
}

void gp100_app_on_systick(gp100_app_state_t *state,
                               const gp100_app_hooks_t *hooks)
{
    if (state == NULL || !gp100_app_hooks_are_valid(hooks)) {
        return;
    }

    ++state->ticks_since_poll;
    hooks->tick_maintenance(hooks->context);
    ++state->event_tick;
    ++state->foreground_ticks_pending;
}
