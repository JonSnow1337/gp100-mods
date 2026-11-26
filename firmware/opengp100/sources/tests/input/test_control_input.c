#include "input/control_input.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct event_log {
    gp100_event_t events[32];
    size_t count;
};

static bool append_event(void *opaque, gp100_event_t event)
{
    struct event_log *log = opaque;
    if (log->count >= sizeof(log->events) / sizeof(log->events[0])) {
        return false;
    }
    log->events[log->count++] = event;
    return true;
}

static int test_pin_map(void)
{
    if (gp100_button_pins[0].bank != GP100_GPIO1 ||
        gp100_button_pins[0].pin != 22u ||
        gp100_button_pins[6].bank != GP100_GPIO3 ||
        gp100_button_pins[6].pin != 16u) {
        return 1;
    }
    if (gp100_encoder_pins[0].phase_a.pin != 27u ||
        gp100_encoder_pins[0].phase_b.pin != 26u ||
        gp100_encoder_pins[3].phase_a.pin != 26u ||
        gp100_encoder_pins[3].phase_b.pin != 0u) {
        return 2;
    }
    return 0;
}

static int test_button_press_hold_repeat_release(void)
{
    gp100_button_scanner_t scanner;
    struct event_log log = {0};
    uint32_t gpio1 = UINT32_MAX;
    unsigned int index;

    gp100_button_scanner_reset(&scanner);
    gpio1 &= ~(UINT32_C(1) << 23); /* button 2, active low */
    for (index = 0u; index < 10u; ++index) {
        gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 100u + index,
                                append_event, &log);
    }
    if (log.count != 1u || log.events[0].value != 109u ||
        log.events[0].code != 0x32010000u) {
        return 1;
    }
    for (index = 0u; index < 500u; ++index) {
        gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 200u + index,
                                append_event, &log);
    }
    if (log.count != 2u || log.events[1].code != 0x32020000u) {
        return 2;
    }
    for (index = 0u; index < 101u; ++index) {
        gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 800u + index,
                                append_event, &log);
    }
    if (log.count != 3u || log.events[2].code != 0x32030000u) {
        return 3;
    }
    gpio1 |= UINT32_C(1) << 23;
    gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 1000u,
                            append_event, &log);
    if (log.count != 4u || log.events[3].code != 0x32000000u) {
        return 4;
    }
    return 0;
}

static int test_double_press(void)
{
    gp100_button_scanner_t scanner;
    struct event_log log = {0};
    uint32_t gpio1 = UINT32_MAX;
    unsigned int index;

    gp100_button_scanner_reset(&scanner);
    gpio1 &= ~(UINT32_C(1) << 15); /* button 3 */
    for (index = 0u; index < 10u; ++index) {
        gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, index,
                                append_event, &log);
    }
    gpio1 |= UINT32_C(1) << 15;
    gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 20u,
                            append_event, &log);
    gpio1 &= ~(UINT32_C(1) << 15);
    for (index = 0u; index < 10u; ++index) {
        gp100_scan_buttons(&scanner, gpio1, UINT32_MAX, 30u + index,
                                append_event, &log);
    }
    if (log.count != 3u || log.events[2].code != 0x33040000u) {
        return 1;
    }
    return 0;
}

static void scan_buttons(gp100_button_scanner_t *scanner,
                         struct event_log *log, uint32_t *tick,
                         uint32_t pressed, unsigned int count)
{
    while (count-- != 0u) {
        gp100_scan_buttons(scanner, ~pressed, UINT32_MAX, ++*tick,
                                append_event, log);
    }
}

static int test_double_press_expiry(void)
{
    const uint32_t exit_button = UINT32_C(1) << 16;

    for (unsigned int gap = 389u; gap <= 390u; ++gap) {
        gp100_button_scanner_t scanner;
        struct event_log log = {0};
        uint32_t tick = 0u;
        uint32_t expected = gap == 389u ? 0x34040000u : 0x34010000u;

        gp100_button_scanner_reset(&scanner);
        scan_buttons(&scanner, &log, &tick, exit_button, 10u);
        scan_buttons(&scanner, &log, &tick, 0u, gap);
        scan_buttons(&scanner, &log, &tick, exit_button, 10u);
        if (log.count != 3u || log.events[2].code != expected) {
            return 1;
        }
    }
    return 0;
}

static int test_footswitch_chord(bool hold)
{
    const uint32_t minus = UINT32_C(1) << 22;
    const uint32_t plus = UINT32_C(1) << 10;
    gp100_button_scanner_t scanner;
    struct event_log log = {0};
    uint32_t tick = 0u;
    size_t release_index = hold ? 3u : 2u;
    unsigned int both_scans = hold ? 715u : 75u;

    gp100_button_scanner_reset(&scanner);
    scan_buttons(&scanner, &log, &tick, minus, 40u);
    scan_buttons(&scanner, &log, &tick, minus | plus, both_scans);
    scan_buttons(&scanner, &log, &tick, plus, 15u);
    scan_buttons(&scanner, &log, &tick, 0u, 1u);
    if (log.count != release_index + 1u ||
        log.events[0].code != 0x30010000u || log.events[0].value != 10u ||
        log.events[1].code != 0x40010000u || log.events[1].value != 50u ||
        log.events[release_index].code != 0x40000000u ||
        log.events[release_index].value != 41u + both_scans) {
        return 1;
    }
    if (hold && (log.events[2].code != 0x40030000u ||
                 log.events[2].value != 510u)) {
        return 2;
    }
    return 0;
}

static uint32_t encoder0_gpio3(uint8_t sample)
{
    uint32_t gpio3 = 0u;
    if ((sample & 1u) != 0u) {
        gpio3 |= UINT32_C(1) << 27;
    }
    if ((sample & 2u) != 0u) {
        gpio3 |= UINT32_C(1) << 26;
    }
    return gpio3;
}

static void confirm_encoder_sample(gp100_encoder_scanner_t *scanner,
                                   struct event_log *log,
                                   uint8_t sample,
                                   uint32_t tick)
{
    unsigned int index;
    for (index = 0u; index < 3u; ++index) {
        gp100_scan_encoders(scanner, 0u, encoder0_gpio3(sample),
                                 tick, append_event, log);
    }
}

static int test_encoder_direction_and_acceleration(void)
{
    gp100_encoder_scanner_t scanner;
    struct event_log log = {0};

    gp100_encoder_scanner_reset(&scanner);
    confirm_encoder_sample(&scanner, &log, 2u, 100u);
    confirm_encoder_sample(&scanner, &log, 3u, 100u);
    if (log.count != 1u || log.events[0].value != 0u ||
        log.events[0].code != 0x2001000cu) {
        return 1;
    }

    confirm_encoder_sample(&scanner, &log, 2u, 300u);
    confirm_encoder_sample(&scanner, &log, 0u, 300u);
    if (log.count != 2u || log.events[1].code != 0x20000008u) {
        return 2;
    }
    return 0;
}

static void confirm_encoders(gp100_encoder_scanner_t *scanner,
                             struct event_log *log, uint32_t tick,
                             uint8_t quick_1, uint8_t para)
{
    const uint32_t gpio1 = ((uint32_t)(para & 1u) << 26) | (para >> 1);

    for (unsigned int scan = 0u; scan < 3u; ++scan) {
        gp100_scan_encoders(scanner, gpio1, encoder0_gpio3(quick_1),
                                 tick, append_event, log);
    }
}

static int test_shared_acceleration(void)
{
    gp100_encoder_scanner_t scanner;
    struct event_log log = {0};

    gp100_encoder_scanner_reset(&scanner);
    confirm_encoders(&scanner, &log, 1100u, 2u, 0u);
    confirm_encoders(&scanner, &log, 1105u, 3u, 0u);
    confirm_encoders(&scanner, &log, 1140u, 3u, 2u);
    confirm_encoders(&scanner, &log, 1145u, 3u, 3u);
    confirm_encoders(&scanner, &log, 2300u, 2u, 3u);
    confirm_encoders(&scanner, &log, 2305u, 0u, 3u);
    confirm_encoders(&scanner, &log, 2340u, 2u, 3u);
    confirm_encoders(&scanner, &log, 2345u, 3u, 3u);
    if (log.count != 4u || log.events[0].code != 0x20010001u ||
        log.events[1].code != 0x2301000cu ||
        log.events[2].code != 0x20000001u ||
        log.events[3].code != 0x2001000cu) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_pin_map();
    if (result != 0) {
        fprintf(stderr, "pin-map test failed: %d\n", result);
        return 10 + result;
    }
    result = test_button_press_hold_repeat_release();
    if (result != 0) {
        fprintf(stderr, "button-state test failed: %d\n", result);
        return 20 + result;
    }
    result = test_double_press();
    if (result != 0) {
        fprintf(stderr, "double-press test failed: %d\n", result);
        return 30 + result;
    }
    result = test_encoder_direction_and_acceleration();
    if (result != 0) {
        fprintf(stderr, "encoder test failed: %d\n", result);
        return 40 + result;
    }
    result = test_footswitch_chord(false);
    if (result != 0) {
        fprintf(stderr, "short footswitch chord test failed: %d\n", result);
        return 50 + result;
    }
    result = test_footswitch_chord(true);
    if (result != 0) {
        fprintf(stderr, "held footswitch chord test failed: %d\n", result);
        return 60 + result;
    }
    result = test_shared_acceleration();
    if (result != 0) {
        fprintf(stderr, "shared encoder acceleration test failed: %d\n", result);
        return 70 + result;
    }
    result = test_double_press_expiry();
    if (result != 0) {
        fprintf(stderr, "double-press expiry test failed: %d\n", result);
        return 80 + result;
    }
    return 0;
}
