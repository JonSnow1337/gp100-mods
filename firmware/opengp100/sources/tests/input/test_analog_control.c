#include "input/analog_control.h"

#include <stdint.h>
#include <stdio.h>

static void make_filters_passthrough(gp100_analog_control_t *control)
{
    uint32_t channel;
    for (channel = 0u; channel < GP100_ANALOG_CHANNEL_COUNT; ++channel) {
        control->filter[channel].b0 = 1.0f;
        control->filter[channel].b1 = 0.0f;
        control->filter[channel].b2 = 0.0f;
        control->filter[channel].a0 = 1.0f;
        control->filter[channel].a1 = 0.0f;
        control->filter[channel].a2 = 0.0f;
    }
}

static int test_expression_curve(void)
{
    uint16_t middle = gp100_expression_curve(2048u);

    if (gp100_expression_curve(0u) != 0u ||
        gp100_expression_curve(4096u) != 4096u) {
        return 1;
    }
    if (middle < 2160u || middle > 2180u) {
        return 2;
    }
    return 0;
}

static int test_window_and_deadband(void)
{
    gp100_analog_control_t control;
    const uint16_t full_scale[2] = {1234u, 4096u};
    const uint16_t zero[2] = {0u, 0u};
    uint32_t index;

    gp100_analog_control_init(&control);
    make_filters_passthrough(&control);
    if (gp100_analog_control_process(&control, full_scale) != 8u ||
        control.filtered[0] != 1234u || control.filtered[1] != 4096u) {
        return 1;
    }
    for (index = 1u; index < GP100_ANALOG_HISTORY_LENGTH; ++index) {
        (void)gp100_analog_control_process(&control, full_scale);
    }
    if (control.expression_value != 256u ||
        control.sum[0] != 1234u * GP100_ANALOG_HISTORY_LENGTH) {
        return 2;
    }
    (void)gp100_analog_control_process(&control, zero);
    if (control.expression_value != 248u) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_expression_curve();
    if (result != 0) {
        fprintf(stderr, "expression curve test failed: %d\n", result);
        return 1;
    }
    result = test_window_and_deadband();
    if (result != 0) {
        fprintf(stderr, "window/deadband test failed: %d\n", result);
        return 1;
    }
    puts("analog control tests passed");
    return 0;
}
