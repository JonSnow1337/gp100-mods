#ifndef GP100_CONTROL_INPUT_H
#define GP100_CONTROL_INPUT_H

#include "app/event_dispatch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_BUTTON_COUNT 8u
#define GP100_ENCODER_COUNT 4u

typedef enum gp100_button_id {
    GP100_BUTTON_FOOTSWITCH_MINUS = 0,
    GP100_BUTTON_FOOTSWITCH_PLUS = 1,
    GP100_BUTTON_GLOBAL = 2,
    GP100_BUTTON_DRUM = 3,
    GP100_BUTTON_EXIT = 4,
    GP100_BUTTON_EDIT = 5,
    GP100_BUTTON_SAVE = 6,
    GP100_BUTTON_PARA = 7
} gp100_button_id_t;

typedef enum gp100_encoder_id {
    GP100_ENCODER_QUICK_1 = 0,
    GP100_ENCODER_QUICK_2 = 1,
    GP100_ENCODER_QUICK_3 = 2,
    GP100_ENCODER_PARA = 3
} gp100_encoder_id_t;

typedef enum gp100_gpio_bank {
    GP100_GPIO1 = 1,
    GP100_GPIO3 = 3
} gp100_gpio_bank_t;

typedef struct gp100_gpio_pin {
    gp100_gpio_bank_t bank;
    uint8_t pin;
} gp100_gpio_pin_t;

typedef struct gp100_encoder_pins {
    gp100_gpio_pin_t phase_a;
    gp100_gpio_pin_t phase_b;
} gp100_encoder_pins_t;

extern const gp100_gpio_pin_t
    gp100_button_pins[GP100_BUTTON_COUNT];
extern const gp100_encoder_pins_t
    gp100_encoder_pins[GP100_ENCODER_COUNT];

typedef enum gp100_button_action {
    GP100_BUTTON_RELEASE = 0,
    GP100_BUTTON_PRESS = 1,
    GP100_BUTTON_HOLD = 2,
    GP100_BUTTON_REPEAT = 3,
    GP100_BUTTON_DOUBLE_PRESS = 4
} gp100_button_action_t;

typedef bool (*gp100_control_emit_t)(void *context,
                                          gp100_event_t event);

typedef struct gp100_button_state {
    uint16_t debounce_ticks;
    uint16_t held_ticks;
    uint16_t double_press_age;
    bool pressed;
    bool double_press_armed;
    bool hold_reported;
} gp100_button_state_t;

typedef struct gp100_button_scanner {
    gp100_button_state_t buttons[GP100_BUTTON_COUNT];
    bool pair_press_pending[2];
    bool suppress_pair_release[2];
    bool pair_release_pending;
} gp100_button_scanner_t;

typedef struct gp100_encoder_state {
    uint8_t history;
    uint8_t confirming_from;
    uint8_t confirmation_ticks;
    uint8_t idle_ticks;
    uint8_t direction_run;
    bool direction_positive;
} gp100_encoder_state_t;

typedef struct gp100_encoder_scanner {
    gp100_encoder_state_t encoders[GP100_ENCODER_COUNT];
    uint32_t last_step_tick;
} gp100_encoder_scanner_t;

void gp100_button_scanner_reset(gp100_button_scanner_t *scanner);
void gp100_encoder_scanner_reset(gp100_encoder_scanner_t *scanner);

/*
 * Poll the eight active-low keys recovered at ITCM 0x15b3c. gpio1/gpio3 are
 * snapshots of the corresponding GPIO data registers. The returned count is
 * the number of events accepted by emit.
 */
size_t gp100_scan_buttons(gp100_button_scanner_t *scanner,
                               uint32_t gpio1,
                               uint32_t gpio3,
                               uint32_t event_tick,
                               gp100_control_emit_t emit,
                               void *context);

/* Poll the four quadrature inputs recovered at ITCM 0x7a54. */
size_t gp100_scan_encoders(gp100_encoder_scanner_t *scanner,
                                uint32_t gpio1,
                                uint32_t gpio3,
                                uint32_t encoder_tick,
                                gp100_control_emit_t emit,
                                void *context);

uint32_t gp100_button_event_code(uint8_t button,
                                      gp100_button_action_t action);
uint32_t gp100_encoder_event_code(uint8_t encoder,
                                       bool positive,
                                       uint8_t acceleration);

#ifdef __cplusplus
}
#endif

#endif
