#include "ui/ui_model.h"

#include <stdio.h>
#include <string.h>

static int test_names(void)
{
    if (strcmp(gp100_ui_mode_name(GP100_UI_MODE_LOOPER),
               "Looper") != 0 ||
        strcmp(gp100_ui_mode_name(GP100_UI_MODE_HARDWARE_TEST),
               "Hardware test") != 0 ||
        GP100_UI_MODE_INTERNAL_8 != GP100_UI_MODE_HARDWARE_TEST ||
        strcmp(gp100_button_name(GP100_BUTTON_DRUM), "DRUM") != 0 ||
        strcmp(gp100_encoder_name(GP100_ENCODER_PARA),
               "PARA knob") != 0) {
        return 1;
    }
    if (strcmp(gp100_ui_mode_name((gp100_ui_mode_t)99),
               "Unknown") != 0) {
        return 2;
    }
    if (gp100_ui_startup_mode(UINT32_MAX) !=
            GP100_UI_MODE_PLAY ||
        gp100_ui_startup_mode(
            UINT32_MAX & ~(UINT32_C(1) << 15u)) !=
            GP100_UI_MODE_PLAY ||
        gp100_ui_startup_mode(
            UINT32_MAX & ~(UINT32_C(1) << 22u)) !=
            GP100_UI_MODE_PLAY ||
        gp100_ui_startup_mode(
            UINT32_MAX & ~((UINT32_C(1) << 15u) |
                           (UINT32_C(1) << 22u))) !=
            GP100_UI_MODE_HARDWARE_TEST) {
        return 3;
    }
    return 0;
}

static int test_hardware_test(void)
{
    gp100_hardware_test_t test;
    gp100_event_t event = {0u, 0u};

    gp100_hardware_test_reset(&test, true);
    if (test.exercised_mask != 0u || test.encoder_value != 0u ||
        test.pressed_buttons != 0u || !test.effect_enabled ||
        test.pedal_reference_valid) {
        return 1;
    }

    test.encoder_value = GP100_HARDWARE_TEST_ENCODER_MAX;
    event.code = 0x22010001u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        test.encoder_value != 0u ||
        test.exercised_mask !=
            GP100_HARDWARE_TEST_ENCODER_BIT(GP100_ENCODER_QUICK_3)) {
        return 2;
    }
    event.code = 0x23000001u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        test.encoder_value != GP100_HARDWARE_TEST_ENCODER_MAX ||
        (test.exercised_mask &
         GP100_HARDWARE_TEST_ENCODER_BIT(GP100_ENCODER_PARA)) == 0u) {
        return 3;
    }

    event.code = 0x36010000u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        (test.pressed_buttons & (UINT8_C(1) << GP100_BUTTON_SAVE)) == 0u ||
        strcmp(gp100_hardware_test_key_label(&test), "SAVE") != 0 ||
        !test.key_pressed ||
        (test.exercised_mask &
         GP100_HARDWARE_TEST_BUTTON_BIT(GP100_BUTTON_SAVE)) == 0u) {
        return 4;
    }
    event.code = 0x36000000u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        (test.pressed_buttons & (UINT8_C(1) << GP100_BUTTON_SAVE)) != 0u ||
        test.key_pressed) {
        return 5;
    }

    event.code = 0x40010000u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        test.effect_enabled ||
        strcmp(gp100_hardware_test_key_label(&test), "L&R:TAP") != 0 ||
        !test.key_pressed) {
        return 6;
    }
    event.code = 0x40000000u;
    if (!gp100_hardware_test_handle_event(&test, &event) ||
        !test.effect_enabled ||
        test.key_pressed ||
        strcmp(gp100_hardware_test_key_label(&test), "L&R:TAP") != 0 ||
        (test.exercised_mask & GP100_HARDWARE_TEST_PAIR_BIT) == 0u) {
        return 7;
    }

    if (gp100_hardware_test_observe_pedal(&test, 10u) ||
        gp100_hardware_test_observe_pedal(&test, 49u) ||
        !gp100_hardware_test_observe_pedal(&test, 50u) ||
        (test.exercised_mask &
         GP100_HARDWARE_TEST_PEDAL_TRAVEL_BIT) == 0u ||
        gp100_hardware_test_observe_pedal(&test, 100u)) {
        return 8;
    }

    test.exercised_mask = GP100_HARDWARE_TEST_COMPLETE_MASK;
    if (!gp100_hardware_test_tick(&test) ||
        !test.completion_visible || test.exit_ready ||
        test.exit_delay_ticks != GP100_HARDWARE_TEST_EXIT_DELAY_TICKS) {
        return 9;
    }
    for (unsigned tick = 1u;
         tick < GP100_HARDWARE_TEST_EXIT_DELAY_TICKS;
         ++tick) {
        if (gp100_hardware_test_tick(&test)) {
            return 10;
        }
    }
    if (!gp100_hardware_test_tick(&test) || !test.exit_ready ||
        test.exit_delay_ticks != 0u ||
        gp100_hardware_test_tick(&test)) {
        return 11;
    }

    event.code = 0x62000001u;
    if (gp100_hardware_test_handle_event(&test, &event) ||
        gp100_hardware_test_handle_event(NULL, &event) ||
        gp100_hardware_test_observe_pedal(NULL, 0u)) {
        return 12;
    }
    gp100_hardware_test_reset(NULL, false);
    if (strcmp(gp100_hardware_test_key_label(NULL), "NULL") != 0) {
        return 13;
    }
    return 0;
}

static int test_decode(void)
{
    gp100_ui_control_event_t decoded;
    gp100_event_t event = {0u, 0x2201000cu};

    if (!gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_ENCODER ||
        decoded.control != GP100_ENCODER_QUICK_3 ||
        decoded.action != 1u || decoded.amount != 12u || decoded.tick != 0u) {
        return 1;
    }

    event.value = 1234u;
    event.code = 0x36020000u;
    if (!gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_BUTTON ||
        decoded.control != GP100_BUTTON_SAVE ||
        decoded.action != GP100_BUTTON_HOLD || decoded.tick != 1234u) {
        return 2;
    }

    event.code = 0x11001234u;
    if (!gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_EXPRESSION_POSITION ||
        decoded.control != GP100_EXPRESSION_PEDAL_CHANNEL ||
        decoded.sample != 0x1234u) {
        return 3;
    }

    event.code = 0x61000123u;
    if (!gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_EXPRESSION_ACTIVATE ||
        decoded.sample != 0x0123u || decoded.tick != 1234u) {
        return 4;
    }

    event.code = 0x40030000u;
    if (!gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_FOOTSWITCH_PAIR ||
        decoded.action != 3u) {
        return 5;
    }

    event.code = 0x62000001u;
    if (gp100_decode_ui_control_event(&event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_NONE) {
        return 6;
    }
    return 0;
}

int main(void)
{
    int result = test_names();
    if (result != 0) {
        fprintf(stderr, "UI name test failed: %d\n", result);
        return 10 + result;
    }
    result = test_decode();
    if (result != 0) {
        fprintf(stderr, "UI decode test failed: %d\n", result);
        return 20 + result;
    }
    result = test_hardware_test();
    if (result != 0) {
        fprintf(stderr, "Hardware-test model failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
