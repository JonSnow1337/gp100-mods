#include "control_input.h"

#include <string.h>

#define BUTTON_EVENT_CLASS UINT32_C(0x30000000)
#define BUTTON_PAIR_EVENT_CLASS UINT32_C(0x40000000)
#define ENCODER_EVENT_CLASS UINT32_C(0x20000000)
#define CONTROL_INDEX_SHIFT 24u
#define CONTROL_ACTION_SHIFT 16u

#define BUTTON_DEBOUNCE_TICKS 10u
#define BUTTON_HOLD_TICKS 500u
#define BUTTON_REPEAT_TICKS 101u
#define BUTTON_DOUBLE_WINDOW_TICKS 400u

const gp100_gpio_pin_t
    gp100_button_pins[GP100_BUTTON_COUNT] = {
        {GP100_GPIO1, 22u},
        {GP100_GPIO1, 10u},
        {GP100_GPIO1, 23u},
        {GP100_GPIO1, 15u},
        {GP100_GPIO1, 16u},
        {GP100_GPIO1, 19u},
        {GP100_GPIO3, 16u},
        {GP100_GPIO1, 18u}
    };

const gp100_encoder_pins_t
    gp100_encoder_pins[GP100_ENCODER_COUNT] = {
        {{GP100_GPIO3, 27u}, {GP100_GPIO3, 26u}},
        {{GP100_GPIO1, 24u}, {GP100_GPIO1, 27u}},
        {{GP100_GPIO1, 11u}, {GP100_GPIO1, 9u}},
        {{GP100_GPIO1, 26u}, {GP100_GPIO1, 0u}}
    };

static bool read_pin(gp100_gpio_pin_t pin,
                     uint32_t gpio1,
                     uint32_t gpio3)
{
    uint32_t value = pin.bank == GP100_GPIO3 ? gpio3 : gpio1;
    return ((value >> pin.pin) & 1u) != 0u;
}

static bool emit_event(gp100_control_emit_t emit,
                       void *context,
                       uint32_t value,
                       uint32_t code)
{
    gp100_event_t event = {.value = value, .code = code};
    return emit != NULL && emit(context, event);
}

uint32_t gp100_button_event_code(uint8_t button,
                                      gp100_button_action_t action)
{
    return BUTTON_EVENT_CLASS |
           ((uint32_t)button << CONTROL_INDEX_SHIFT) |
           ((uint32_t)action << CONTROL_ACTION_SHIFT);
}

uint32_t gp100_encoder_event_code(uint8_t encoder,
                                       bool positive,
                                       uint8_t acceleration)
{
    return ENCODER_EVENT_CLASS |
           ((uint32_t)encoder << CONTROL_INDEX_SHIFT) |
           (positive ? UINT32_C(0x00010000) : 0u) |
           acceleration;
}

void gp100_button_scanner_reset(gp100_button_scanner_t *scanner)
{
    if (scanner != NULL) {
        memset(scanner, 0, sizeof(*scanner));
    }
}

void gp100_encoder_scanner_reset(gp100_encoder_scanner_t *scanner)
{
    if (scanner != NULL) {
        memset(scanner, 0, sizeof(*scanner));
    }
}

static bool emit_button_action(gp100_button_scanner_t *scanner,
                               uint8_t button,
                               gp100_button_action_t action,
                               uint32_t tick,
                               gp100_control_emit_t emit,
                               void *context)
{
    if (button >= 2u) {
        return emit_event(emit, context, tick,
                          gp100_button_event_code(button, action));
    }

    if (action == GP100_BUTTON_PRESS) {
        uint8_t other = (uint8_t)(button ^ 1u);
        if (scanner->pair_press_pending[other]) {
            scanner->suppress_pair_release[0] = true;
            scanner->suppress_pair_release[1] = true;
            scanner->pair_release_pending = true;
            return emit_event(emit, context, tick,
                              BUTTON_PAIR_EVENT_CLASS | UINT32_C(0x00010000));
        }
        scanner->pair_press_pending[button] = true;
    } else if (action == GP100_BUTTON_RELEASE) {
        const bool paired = scanner->suppress_pair_release[button];
        const bool emit_pair = scanner->pair_release_pending;

        scanner->pair_press_pending[0] = false;
        scanner->pair_press_pending[1] = false;
        scanner->pair_release_pending = false;
        scanner->suppress_pair_release[button] = false;
        if (paired) {
            /* Keep the other key's release suppressed after the chord exits. */
            return emit_pair &&
                emit_event(emit, context, tick, BUTTON_PAIR_EVENT_CLASS);
        }
    } else if (action == GP100_BUTTON_HOLD ||
               action == GP100_BUTTON_REPEAT) {
        if (scanner->suppress_pair_release[0] &&
            scanner->suppress_pair_release[1]) {
            const bool first_hold = scanner->pair_press_pending[0] ||
                scanner->pair_press_pending[1];
            scanner->pair_press_pending[0] = false;
            scanner->pair_press_pending[1] = false;
            return first_hold && emit_event(emit, context, tick,
                BUTTON_PAIR_EVENT_CLASS | UINT32_C(0x00030000));
        }
        scanner->pair_press_pending[button] = false;
        scanner->pair_release_pending = false;
    }

    return emit_event(emit, context, tick,
                      gp100_button_event_code(button, action));
}

size_t gp100_scan_buttons(gp100_button_scanner_t *scanner,
                               uint32_t gpio1,
                               uint32_t gpio3,
                               uint32_t event_tick,
                               gp100_control_emit_t emit,
                               void *context)
{
    size_t emitted = 0u;
    uint8_t button;

    if (scanner == NULL || emit == NULL) {
        return 0u;
    }

    for (button = 0u; button < GP100_BUTTON_COUNT; ++button) {
        gp100_button_state_t *state = &scanner->buttons[button];
        bool active = !read_pin(gp100_button_pins[button], gpio1, gpio3);
        gp100_button_action_t action;
        bool have_action = false;

        if (state->double_press_armed) {
            ++state->double_press_age;
            if (state->double_press_age >= BUTTON_DOUBLE_WINDOW_TICKS) {
                state->double_press_armed = false;
                state->double_press_age = 0u;
            }
        }

        if (!state->pressed) {
            if (!active) {
                state->debounce_ticks = 0u;
                continue;
            }
            if (++state->debounce_ticks < BUTTON_DEBOUNCE_TICKS) {
                continue;
            }

            state->debounce_ticks = 0u;
            state->pressed = true;
            state->held_ticks = 0u;
            state->hold_reported = false;
            if (state->double_press_armed) {
                state->double_press_armed = false;
                state->double_press_age = 0u;
                action = GP100_BUTTON_DOUBLE_PRESS;
            } else {
                state->double_press_armed = true;
                state->double_press_age = 0u;
                action = GP100_BUTTON_PRESS;
            }
            have_action = true;
        } else if (!active) {
            state->pressed = false;
            state->debounce_ticks = 0u;
            state->held_ticks = 0u;
            state->hold_reported = false;
            action = GP100_BUTTON_RELEASE;
            have_action = true;
        } else {
            ++state->held_ticks;
            if (!state->hold_reported &&
                state->held_ticks >= BUTTON_HOLD_TICKS) {
                state->hold_reported = true;
                action = GP100_BUTTON_HOLD;
                have_action = true;
            } else if (state->hold_reported &&
                       state->held_ticks >=
                           BUTTON_HOLD_TICKS + BUTTON_REPEAT_TICKS) {
                state->held_ticks = BUTTON_HOLD_TICKS;
                action = GP100_BUTTON_REPEAT;
                have_action = true;
            }
        }

        if (have_action &&
            emit_button_action(scanner, button, action, event_tick,
                               emit, context)) {
            ++emitted;
        }
    }
    return emitted;
}

static uint8_t encoder_sample(uint8_t encoder,
                              uint32_t gpio1,
                              uint32_t gpio3)
{
    const gp100_encoder_pins_t *pins = &gp100_encoder_pins[encoder];
    uint8_t phase_a = read_pin(pins->phase_a, gpio1, gpio3) ? 1u : 0u;
    uint8_t phase_b = read_pin(pins->phase_b, gpio1, gpio3) ? 2u : 0u;
    return (uint8_t)(phase_a | phase_b);
}

static uint8_t encoder_acceleration(uint32_t interval)
{
    if (interval <= 150u) {
        return 12u;
    }
    if (interval <= 250u) {
        return 8u;
    }
    if (interval <= 500u) {
        return 4u;
    }
    if (interval <= 1000u) {
        return 2u;
    }
    return 1u;
}

size_t gp100_scan_encoders(gp100_encoder_scanner_t *scanner,
                                uint32_t gpio1,
                                uint32_t gpio3,
                                uint32_t encoder_tick,
                                gp100_control_emit_t emit,
                                void *context)
{
    size_t emitted = 0u;
    uint8_t encoder;

    if (scanner == NULL || emit == NULL) {
        return 0u;
    }

    for (encoder = 0u; encoder < GP100_ENCODER_COUNT; ++encoder) {
        gp100_encoder_state_t *state = &scanner->encoders[encoder];
        uint8_t sample = encoder_sample(encoder, gpio1, gpio3);
        uint8_t stable = state->history & 3u;

        if (sample == stable) {
            if (state->idle_ticks != 0u) {
                state->confirming_from = 0u;
                state->confirmation_ticks = 0u;
                if (++state->idle_ticks > 100u) {
                    state->idle_ticks = 0u;
                    state->direction_run = 0u;
                }
            }
            continue;
        }
        if (state->confirming_from != stable) {
            state->confirming_from = stable;
            state->confirmation_ticks = 0u;
            continue;
        }
        if (++state->confirmation_ticks < 2u) {
            continue;
        }

        state->confirmation_ticks = 0u;
        state->history = (uint8_t)(sample | (uint8_t)(state->history << 2));
        {
            uint8_t pattern = state->history & 0x3fu;
            bool positive;
            uint32_t interval;
            uint8_t acceleration;

            if (pattern != 0x0bu && pattern != 0x38u) {
                continue;
            }
            positive = pattern == 0x0bu;
            if (state->direction_run == 0u ||
                state->direction_positive != positive) {
                state->direction_positive = positive;
                state->direction_run = 0u;
            }
            if (state->direction_run < 8u) {
                ++state->direction_run;
            }
            state->idle_ticks = 1u;
            interval = encoder_tick - scanner->last_step_tick;
            scanner->last_step_tick = encoder_tick;
            acceleration = encoder_acceleration(interval);
            if (emit_event(emit, context, 0u,
                           gp100_encoder_event_code(
                               encoder, positive, acceleration))) {
                ++emitted;
            }
        }
    }
    return emitted;
}
