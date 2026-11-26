#ifndef GP100_UI_MODEL_H
#define GP100_UI_MODEL_H

#include "input/control_input.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_ui_mode {
    GP100_UI_MODE_PLAY = 1,
    GP100_UI_MODE_TUNER = 2,
    GP100_UI_MODE_DRUM = 3,
    GP100_UI_MODE_LOOPER = 4,
    GP100_UI_MODE_EDIT = 5,
    GP100_UI_MODE_GLOBAL = 6,
    GP100_UI_MODE_SAVE = 7,
    GP100_UI_MODE_HARDWARE_TEST = 8,
    GP100_UI_MODE_CONSTRUCT = 9
} gp100_ui_mode_t;

/* Compatibility name used before the mode-8 screen contents were recovered. */
#define GP100_UI_MODE_INTERNAL_8 GP100_UI_MODE_HARDWARE_TEST

#define GP100_HARDWARE_TEST_BUTTON_BIT(button) \
    ((uint16_t)(UINT16_C(1) << (button)))
#define GP100_HARDWARE_TEST_PAIR_BIT UINT16_C(0x0100)
#define GP100_HARDWARE_TEST_ENCODER_BIT(encoder) \
    ((uint16_t)(UINT16_C(0x0200) << (encoder)))
#define GP100_HARDWARE_TEST_PEDAL_TRAVEL_BIT UINT16_C(0x2000)
#define GP100_HARDWARE_TEST_COMPLETE_MASK UINT16_C(0x3fff)
#define GP100_HARDWARE_TEST_EXIT_DELAY_TICKS 20u
#define GP100_HARDWARE_TEST_PEDAL_TRAVEL_MIN 40u
#define GP100_HARDWARE_TEST_ENCODER_MAX 100u

/* Factory startup samples active-low GPIO1_IO15 (DRUM) and GPIO1_IO22
 * (footswitch minus). Holding both enters the hardware-test screen. */
gp100_ui_mode_t gp100_ui_startup_mode(uint32_t gpio1_snapshot);

typedef enum gp100_ui_control_kind {
    GP100_UI_CONTROL_NONE = 0,
    GP100_UI_CONTROL_EXPRESSION_POSITION,
    GP100_UI_CONTROL_EXPRESSION_ACTIVATE,
    GP100_UI_CONTROL_ENCODER,
    GP100_UI_CONTROL_BUTTON,
    GP100_UI_CONTROL_FOOTSWITCH_PAIR,
    GP100_UI_CONTROL_TICK
} gp100_ui_control_kind_t;

typedef struct gp100_ui_control_event {
    gp100_ui_control_kind_t kind;
    uint8_t control;
    uint8_t action;
    uint8_t amount;
    uint16_t sample;
    uint32_t tick;
} gp100_ui_control_event_t;

/*
 * Readable state model for the factory hardware-test screen at mode 8. The
 * shipped handler uses one displayed encoder value for all four knobs and a
 * 16-bit mask to remember which controls have been exercised.
 */
typedef struct gp100_hardware_test {
    uint16_t exercised_mask;
    uint16_t pedal_reference;
    uint8_t encoder_value;
    uint8_t pressed_buttons;
    uint8_t last_button;
    uint8_t pair_action;
    bool pedal_reference_valid;
    bool effect_enabled;
    bool last_button_valid;
    bool key_pressed;
    bool completion_visible;
    bool exit_ready;
    uint8_t exit_delay_ticks;
} gp100_hardware_test_t;

const char *gp100_ui_mode_name(gp100_ui_mode_t mode);
const char *gp100_button_name(gp100_button_id_t button);
const char *gp100_encoder_name(gp100_encoder_id_t encoder);

/*
 * Synthetic event (class 7) injected by the application when a debounce timer
 * expires. The editor uses it to start the effect preview after the encoder
 * has stopped turning. It never comes from the hardware.
 */
#define GP100_UI_TICK_EVENT_CODE (UINT32_C(7) << 28)

/* Decode expression, encoder, button, footswitch-pair and tick event classes. */
bool gp100_decode_ui_control_event(
    const gp100_event_t *event,
    gp100_ui_control_event_t *decoded);

void gp100_hardware_test_reset(gp100_hardware_test_t *test,
                                    bool effect_enabled);

/* Apply one packed front-panel event. Returns false for unsupported events. */
bool gp100_hardware_test_handle_event(
    gp100_hardware_test_t *test,
    const gp100_event_t *event);

/* Record the initial pedal sample and qualify travel of at least 40 units. */
bool gp100_hardware_test_observe_pedal(
    gp100_hardware_test_t *test,
    uint16_t sample);

/* Advance the factory completion banner and delayed exit-enable state once. */
bool gp100_hardware_test_tick(gp100_hardware_test_t *test);

/* Text shown by the factory mode-8 key widget after the latest key event. */
const char *gp100_hardware_test_key_label(
    const gp100_hardware_test_t *test);

#ifdef __cplusplus
}
#endif

#endif
