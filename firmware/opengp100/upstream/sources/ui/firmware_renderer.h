#ifndef GP100_FIRMWARE_RENDERER_H
#define GP100_FIRMWARE_RENDERER_H

#include "app/firmware_runtime.h"
#include "ui/screens/hardware_test_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Board-supplied values which are not owned by the portable UI runtime.
 * Every string may be NULL; readable development defaults are then used.
 */
typedef struct gp100_firmware_render_context {
    const char *firmware_version;
    const char *save_destination_name;
    const char *hardware_info;
    const char *hardware_variant;
    int32_t tuner_value;
    int32_t pedal_value;
    uint8_t encoder_id;
} gp100_firmware_render_context_t;

/* One initialized palette bundle for every recovered top-level screen. */
typedef struct gp100_firmware_renderer {
    gp100_play_screen_theme_t play;
    gp100_tuner_screen_theme_t tuner;
    gp100_drum_screen_theme_t drum;
    gp100_looper_screen_theme_t looper;
    gp100_edit_screen_theme_t edit;
    gp100_global_screen_theme_t global;
    gp100_expression_settings_theme_t expression;
    gp100_language_screen_theme_t language;
    gp100_factory_reset_theme_t factory_reset;
    gp100_save_screen_theme_t save;
    gp100_hardware_test_theme_t hardware_test;
} gp100_firmware_renderer_t;

/* Use one caller-owned font for all three font roles. */
bool gp100_firmware_renderer_init(
    gp100_firmware_renderer_t *renderer,
    const gp100_bitmap_font_t *font);

/* Fonts remain caller-owned for the lifetime of the renderer. */
bool gp100_firmware_renderer_init_with_fonts(
    gp100_firmware_renderer_t *renderer,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

/*
 * Render the active screen from the complete portable runtime. This is the
 * single display entry point intended for MCUXpresso board applications.
 */
bool gp100_firmware_renderer_render(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime,
    const gp100_firmware_render_context_t *context);

#ifdef __cplusplus
}
#endif

#endif
