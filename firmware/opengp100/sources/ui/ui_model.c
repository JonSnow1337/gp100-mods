#include "ui_model.h"

#include <stddef.h>
#include <string.h>

#define STARTUP_DRUM_GPIO1_PIN 15u
#define STARTUP_FOOTSWITCH_MINUS_GPIO1_PIN 22u

gp100_ui_mode_t gp100_ui_startup_mode(uint32_t gpio1_snapshot)
{
    const uint32_t chord_mask =
        (UINT32_C(1) << STARTUP_DRUM_GPIO1_PIN) |
        (UINT32_C(1) << STARTUP_FOOTSWITCH_MINUS_GPIO1_PIN);

    return (gpio1_snapshot & chord_mask) == 0u
               ? GP100_UI_MODE_HARDWARE_TEST
               : GP100_UI_MODE_PLAY;
}

const char *gp100_ui_mode_name(gp100_ui_mode_t mode)
{
    switch (mode) {
    case GP100_UI_MODE_PLAY:
        return "Play";
    case GP100_UI_MODE_TUNER:
        return "Tuner";
    case GP100_UI_MODE_DRUM:
        return "Drum";
    case GP100_UI_MODE_LOOPER:
        return "Looper";
    case GP100_UI_MODE_EDIT:
        return "Edit";
    case GP100_UI_MODE_GLOBAL:
        return "Global";
    case GP100_UI_MODE_SAVE:
        return "Save";
    case GP100_UI_MODE_CONSTRUCT:
        return "Construct";
    case GP100_UI_MODE_HARDWARE_TEST:
        return "Hardware test";
    default:
        return "Unknown";
    }
}

const char *gp100_button_name(gp100_button_id_t button)
{
    switch (button) {
    case GP100_BUTTON_FOOTSWITCH_MINUS:
        return "Footswitch -";
    case GP100_BUTTON_FOOTSWITCH_PLUS:
        return "Footswitch +";
    case GP100_BUTTON_GLOBAL:
        return "GLOBAL";
    case GP100_BUTTON_DRUM:
        return "DRUM";
    case GP100_BUTTON_EXIT:
        return "EXIT";
    case GP100_BUTTON_EDIT:
        return "EDIT";
    case GP100_BUTTON_SAVE:
        return "SAVE";
    case GP100_BUTTON_PARA:
        return "PARA press";
    default:
        return "Unknown button";
    }
}

const char *gp100_encoder_name(gp100_encoder_id_t encoder)
{
    switch (encoder) {
    case GP100_ENCODER_QUICK_1:
        return "Quick knob 1";
    case GP100_ENCODER_QUICK_2:
        return "Quick knob 2";
    case GP100_ENCODER_QUICK_3:
        return "Quick knob 3";
    case GP100_ENCODER_PARA:
        return "PARA knob";
    default:
        return "Unknown encoder";
    }
}

bool gp100_decode_ui_control_event(
    const gp100_event_t *event,
    gp100_ui_control_event_t *decoded)
{
    uint8_t event_class;
    uint8_t control;

    if (event == NULL || decoded == NULL) {
        return false;
    }

    event_class = (uint8_t)(event->code >> 28);
    control = (uint8_t)((event->code >> 24) & 0x0fu);
    decoded->kind = GP100_UI_CONTROL_NONE;
    decoded->control = control;
    decoded->action = (uint8_t)(event->code >> 16);
    decoded->amount = (uint8_t)event->code;
    decoded->sample = (uint16_t)event->code;
    decoded->tick = event->value;

    if (event_class == 1u &&
        control == GP100_EXPRESSION_PEDAL_CHANNEL) {
        decoded->kind = GP100_UI_CONTROL_EXPRESSION_POSITION;
        return true;
    }
    if (event_class == 6u &&
        control == GP100_EXPRESSION_PEDAL_CHANNEL) {
        decoded->kind = GP100_UI_CONTROL_EXPRESSION_ACTIVATE;
        return true;
    }
    if (event_class == 2u && control < GP100_ENCODER_COUNT) {
        decoded->kind = GP100_UI_CONTROL_ENCODER;
        return true;
    }
    if (event_class == 3u && control < GP100_BUTTON_COUNT) {
        decoded->kind = GP100_UI_CONTROL_BUTTON;
        return true;
    }
    if (event_class == 4u && control == 0u) {
        decoded->kind = GP100_UI_CONTROL_FOOTSWITCH_PAIR;
        return true;
    }
    if (event_class == 7u && control == 0u) {
        decoded->kind = GP100_UI_CONTROL_TICK;
        return true;
    }
    return false;
}

void gp100_hardware_test_reset(gp100_hardware_test_t *test,
                                    bool effect_enabled)
{
    if (test == NULL) {
        return;
    }

    memset(test, 0, sizeof(*test));
    test->effect_enabled = effect_enabled;
}

bool gp100_hardware_test_handle_event(
    gp100_hardware_test_t *test,
    const gp100_event_t *event)
{
    gp100_ui_control_event_t decoded;
    uint8_t button_bit;

    if (test == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return false;
    }

    switch (decoded.kind) {
    case GP100_UI_CONTROL_ENCODER:
        if (decoded.action == GP100_BUTTON_PRESS) {
            if (test->encoder_value >= GP100_HARDWARE_TEST_ENCODER_MAX) {
                test->encoder_value = 0u;
            } else {
                ++test->encoder_value;
            }
        } else if (decoded.action == GP100_BUTTON_RELEASE) {
            if (test->encoder_value == 0u) {
                test->encoder_value = GP100_HARDWARE_TEST_ENCODER_MAX;
            } else {
                --test->encoder_value;
            }
        }
        test->exercised_mask |=
            GP100_HARDWARE_TEST_ENCODER_BIT(decoded.control);
        return true;

    case GP100_UI_CONTROL_BUTTON:
        button_bit = (uint8_t)(UINT8_C(1) << decoded.control);
        test->exercised_mask |=
            GP100_HARDWARE_TEST_BUTTON_BIT(decoded.control);
        test->last_button = decoded.control;
        test->last_button_valid = true;
        test->key_pressed = decoded.action != GP100_BUTTON_RELEASE;
        if (decoded.action == GP100_BUTTON_RELEASE) {
            test->pressed_buttons &= (uint8_t)~button_bit;
        } else {
            test->pressed_buttons |= button_bit;
        }
        return true;

    case GP100_UI_CONTROL_FOOTSWITCH_PAIR:
        test->effect_enabled = !test->effect_enabled;
        test->key_pressed = decoded.action != GP100_BUTTON_RELEASE;
        if (decoded.action == GP100_BUTTON_PRESS ||
            decoded.action == GP100_BUTTON_HOLD) {
            test->pair_action = decoded.action;
            test->last_button_valid = false;
        }
        if (decoded.action == GP100_BUTTON_RELEASE) {
            test->exercised_mask |= GP100_HARDWARE_TEST_PAIR_BIT;
        }
        return true;

    default:
        return false;
    }
}

bool gp100_hardware_test_observe_pedal(
    gp100_hardware_test_t *test,
    uint16_t sample)
{
    uint16_t distance;

    if (test == NULL) {
        return false;
    }
    if (!test->pedal_reference_valid) {
        test->pedal_reference = sample;
        test->pedal_reference_valid = true;
        return false;
    }
    if ((test->exercised_mask & GP100_HARDWARE_TEST_PEDAL_TRAVEL_BIT) !=
        0u) {
        return false;
    }

    if (sample >= test->pedal_reference) {
        distance = (uint16_t)(sample - test->pedal_reference);
    } else {
        distance = (uint16_t)(test->pedal_reference - sample);
    }
    if (distance < GP100_HARDWARE_TEST_PEDAL_TRAVEL_MIN) {
        return false;
    }

    test->exercised_mask |= GP100_HARDWARE_TEST_PEDAL_TRAVEL_BIT;
    return true;
}

bool gp100_hardware_test_tick(gp100_hardware_test_t *test)
{
    if (test == NULL || test->exit_ready) {
        return false;
    }
    if (test->exit_delay_ticks != 0u) {
        --test->exit_delay_ticks;
        if (test->exit_delay_ticks == 0u) {
            test->exit_ready = true;
            return true;
        }
        return false;
    }
    if (test->exercised_mask != GP100_HARDWARE_TEST_COMPLETE_MASK) {
        return false;
    }
    test->completion_visible = true;
    test->exit_delay_ticks = GP100_HARDWARE_TEST_EXIT_DELAY_TICKS;
    return true;
}

const char *gp100_hardware_test_key_label(
    const gp100_hardware_test_t *test)
{
    if (test == NULL) {
        return "NULL";
    }
    if (test->last_button_valid) {
        switch ((gp100_button_id_t)test->last_button) {
        case GP100_BUTTON_FOOTSWITCH_MINUS:
            return "FS_L";
        case GP100_BUTTON_FOOTSWITCH_PLUS:
            return "FS_R";
        case GP100_BUTTON_GLOBAL:
            return "GLOBAL";
        case GP100_BUTTON_DRUM:
            return "DRUM";
        case GP100_BUTTON_EXIT:
            return "EXIT";
        case GP100_BUTTON_EDIT:
            return "EDIT";
        case GP100_BUTTON_SAVE:
            return "SAVE";
        case GP100_BUTTON_PARA:
            return "PARA";
        default:
            break;
        }
    }
    if (test->pair_action == GP100_BUTTON_PRESS) {
        return "L&R:TAP";
    }
    if (test->pair_action == GP100_BUTTON_HOLD) {
        return "L&R:HOLD";
    }
    return "NULL";
}
