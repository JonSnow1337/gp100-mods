#ifndef GP100_APP_RUNTIME_H
#define GP100_APP_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Values are the V2.1.0 ITCM addresses of the corresponding routines.  They
 * make traces and documentation directly cross-referenceable without using
 * address-derived names in application code.
 */
typedef enum gp100_init_stage {
    GP100_INIT_CONFIGURE_MPU          = 0x00004030u,
    GP100_INIT_BOARD_IOMUX            = 0x00004268u,
    GP100_INIT_LPI2C1_PINS            = 0x00004220u,
    GP100_INIT_BOOT_CLOCKS            = 0x00003f18u,
    GP100_INIT_SYSTEM_STATE_AND_SYSTICK = 0x00015178u,
    GP100_INIT_SAI1_TX_SYNC_PIN       = 0x00003ef0u,
    GP100_INIT_BUTTON_GPIO_INPUTS     = 0x00015a0cu,
    GP100_INIT_ENCODER_GPIO_INPUTS    = 0x00007960u,
    GP100_INIT_ANALOG_CONTROL_ADC_DMA = 0x0001529cu,
    GP100_INIT_TUNER_EDGE_INPUT       = 0x0001552cu,
    GP100_INIT_DISPLAY_CONTROLLER_AND_FRAMEBUFFER = 0x00016d88u,
    GP100_INIT_MEMORY_BUFFER_LAYOUT   = 0x0001907cu,
    GP100_INIT_CLEAR_WORK_AREAS       = 0x00020d00u,
    GP100_INIT_USB_AUDIO_MIDI         = 0x00020d5cu,
    GP100_INIT_SELECT_STARTUP_UI_MODE = 0x0001986cu,
    GP100_INIT_VERIFY_DEVICE_RECORD   = 0x000064b8u
} gp100_init_stage_t;

/* Compatibility names retained for clients written before these roles were
 * resolved from their peripheral accesses and callers. */
#define GP100_INIT_BOARD_PINS \
    GP100_INIT_BOARD_IOMUX
#define GP100_INIT_AUDIO_AND_DMA \
    GP100_INIT_ANALOG_CONTROL_ADC_DMA
#define GP100_INIT_PRESET_AND_UI_STATE \
    GP100_INIT_SELECT_STARTUP_UI_MODE
#define GP100_INIT_SYSTICK \
    GP100_INIT_SYSTEM_STATE_AND_SYSTICK
#define GP100_INIT_GPIO_GROUP_A \
    GP100_INIT_BUTTON_GPIO_INPUTS
#define GP100_INIT_GPIO_GROUP_B \
    GP100_INIT_ENCODER_GPIO_INPUTS
#define GP100_INIT_AUDIO_CODECS \
    GP100_INIT_DISPLAY_CONTROLLER_AND_FRAMEBUFFER
#define GP100_INIT_GPIO1_IO21_INTERRUPT \
    GP100_INIT_TUNER_EDGE_INPUT

typedef enum gp100_periodic_timer {
    GP100_PERIODIC_TIMER_CONTROL_SCAN = 0x00020844u,
    GP100_PERIODIC_TIMER_RHYTHM_SEQUENCER = 0x000208d8u
} gp100_periodic_timer_t;

#define GP100_PERIODIC_TIMER_200 \
    GP100_PERIODIC_TIMER_CONTROL_SCAN
#define GP100_PERIODIC_TIMER_500 \
    GP100_PERIODIC_TIMER_RHYTHM_SEQUENCER

typedef enum gp100_step_result {
    GP100_STEP_INVALID = -1,
    GP100_STEP_IDLE = 0,
    GP100_STEP_TICK_HANDLED = 1
} gp100_step_result_t;

/*
 * Semantic state used by the reference model.  This is intentionally not a
 * memory-mapped reproduction of the original 0x200028cc object.
 */
typedef struct gp100_app_state {
    volatile bool system_ready;
    volatile uint32_t tuner_counter_clock_hz;
    volatile uint32_t foreground_ticks_pending;
    volatile uint32_t ticks_since_poll;
    volatile uint32_t event_tick;
} gp100_app_state_t;

/*
 * The callbacks separate observed V2.1.0 orchestration from one particular
 * board implementation.  An MCUXpresso firmware can bind these to SDK-based
 * drivers; host tests can bind them to deterministic fakes.
 */
typedef struct gp100_app_hooks {
    void *context;

    void (*run_initialization_stage)(void *context,
                                     gp100_init_stage_t stage);
    uint32_t (*initialize_tuner_counter)(void *context);
    void (*configure_periodic_timer)(void *context,
                                     gp100_periodic_timer_t timer,
                                     uint32_t period_us);

    void (*poll_control_events)(void *context);
    void (*parse_midi_input)(void *context);
    void (*parse_midi_transport)(void *context);
    void (*dispatch_ui_event)(void *context);
    void (*flush_display)(void *context);
    void (*flush_settings)(void *context);

    void (*set_tmr2_irq_enabled)(void *context, bool enabled);
    void (*data_synchronization_barrier)(void *context);
    void (*instruction_synchronization_barrier)(void *context);
    void (*tick_maintenance)(void *context);
} gp100_app_hooks_t;

bool gp100_app_hooks_are_valid(const gp100_app_hooks_t *hooks);

bool gp100_app_initialize(gp100_app_state_t *state,
                               const gp100_app_hooks_t *hooks);

gp100_step_result_t gp100_app_service_once(
    gp100_app_state_t *state,
    const gp100_app_hooks_t *hooks);

void gp100_app_on_systick(gp100_app_state_t *state,
                               const gp100_app_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
