#ifndef GP100_HARDWARE_TEST_SCREEN_H
#define GP100_HARDWARE_TEST_SCREEN_H

#include "ui/render/display_text.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_hardware_test_snapshot {
    const char *hardware_info;
    const char *hardware_variant;
    const char *version;
    int32_t tuner_value;
    int32_t pedal_value;
    uint8_t encoder_id;
    const gp100_hardware_test_t *test;
} gp100_hardware_test_snapshot_t;

typedef struct gp100_hardware_test_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t foreground_rgb565;
    uint16_t background_rgb565;
    uint16_t accent_rgb565;
} gp100_hardware_test_theme_t;

/*
 * Render the recovered 160x128 factory diagnostics layout. The caller owns
 * the font assets and canvas storage, making the screen usable in new
 * firmware without depending on the factory GUI library.
 */
bool gp100_hardware_test_render(
    gp100_display_canvas_t *canvas,
    const gp100_hardware_test_snapshot_t *snapshot,
    const gp100_hardware_test_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
