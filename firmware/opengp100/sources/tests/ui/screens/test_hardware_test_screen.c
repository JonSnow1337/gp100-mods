#include "ui/screens/hardware_test_screen.h"

#include <stdio.h>
#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL,
    0u,
    8u,
    5u,
};

int main(void)
{
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    gp100_display_canvas_t canvas;
    gp100_hardware_test_t test;
    gp100_hardware_test_snapshot_t snapshot = {
        "GP100",
        "A",
        "2.1.0",
        440,
        127,
        2u,
        &test,
    };
    gp100_hardware_test_theme_t theme = {
        &empty_font,
        NULL,
        0xffffu,
        0x0000u,
        0x07e0u,
    };

    memset(pixels, 0xa5, sizeof(pixels));
    gp100_hardware_test_reset(&test, true);
    test.encoder_value = 50u;
    test.last_button = GP100_BUTTON_SAVE;
    test.last_button_valid = true;
    test.key_pressed = true;
    test.completion_visible = true;
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_hardware_test_render(&canvas, &snapshot, &theme)) {
        return 1;
    }
    if (pixels[0] != 0x0000u ||
        pixels[34u * GP100_UI_LOGICAL_WIDTH + 50u] != 0xffffu ||
        pixels[36u * GP100_UI_LOGICAL_WIDTH + 52u] != 0x07e0u ||
        pixels[51u * GP100_UI_LOGICAL_WIDTH + 51u] != 0x07e0u ||
        pixels[68u * GP100_UI_LOGICAL_WIDTH + 3u] != 0xffffu) {
        return 2;
    }
    if (gp100_hardware_test_render(NULL, &snapshot, &theme)) {
        return 3;
    }
    canvas.width = 128u;
    if (gp100_hardware_test_render(&canvas, &snapshot, &theme)) {
        return 4;
    }
    memset(pixels, 0, sizeof(pixels));
    if (!gp100_display_canvas_init_rotated(
            &canvas, pixels, 128u, 160u, 128u,
            GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE) ||
        !gp100_hardware_test_render(&canvas, &snapshot, &theme) ||
        pixels[50u * 128u + 93u] != 0xffffu ||
        pixels[109u * 128u + 34u] != 0x0000u) {
        return 5;
    }
    return 0;
}
