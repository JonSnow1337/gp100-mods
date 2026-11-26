#include "assets/ui/development_font.h"
#include "ui/firmware_renderer.h"

#include <string.h>

static bool render_mode(gp100_firmware_renderer_t *renderer,
                        gp100_display_canvas_t *canvas,
                        gp100_firmware_runtime_t *runtime,
                        gp100_ui_mode_t mode,
                        const gp100_firmware_render_context_t *context)
{
    memset(canvas->pixels, 0xa5,
           canvas->stride_pixels * canvas->storage_height *
               sizeof(uint16_t));
    return gp100_ui_runtime_set_mode(
               &runtime->ui, mode, &runtime->preset,
               &runtime->settings) &&
           gp100_firmware_renderer_render(
               renderer, canvas, runtime, context);
}

int main(void)
{
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    int32_t looper_storage[512];
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_firmware_runtime_t runtime;
    gp100_firmware_renderer_t renderer;
    gp100_display_canvas_t canvas;
    gp100_firmware_render_context_t context = {
        "development",
        "Destination",
        "GP-100",
        "reference board",
        440,
        2048,
        0u,
    };
    const gp100_bitmap_font_t *font =
        gp100_development_font_5x7();
    gp100_ui_mode_t mode;

    if (!gp100_make_development_preset(&preset, "Development") ||
        !gp100_make_development_settings(&settings) ||
        !gp100_firmware_runtime_init(
            &runtime, 0u, &preset, &settings,
            looper_storage,
            sizeof(looper_storage) / sizeof(looper_storage[0]),
            0u, 4095u) ||
        !gp100_firmware_renderer_init_with_fonts(
            &renderer, gp100_development_font_10x14(), font, font) ||
        !gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT,
            GP100_UI_LOGICAL_WIDTH)) {
        return 1;
    }
    for (mode = GP100_UI_MODE_PLAY;
         mode <= GP100_UI_MODE_HARDWARE_TEST;
         mode = (gp100_ui_mode_t)(mode + 1)) {
        if (!render_mode(
                &renderer, &canvas, &runtime, mode, &context)) {
            return 2 + (int)mode;
        }
    }
    if (!gp100_ui_runtime_set_mode(
            &runtime.ui, GP100_UI_MODE_GLOBAL,
            &runtime.preset, &runtime.settings)) {
        return 11;
    }
    runtime.ui.global.view = GP100_GLOBAL_VIEW_EXPRESSION;
    if (!gp100_firmware_renderer_render(
            &renderer, &canvas, &runtime, &context)) {
        return 12;
    }
    runtime.ui.global.view = GP100_GLOBAL_VIEW_LANGUAGE;
    memset(&runtime.ui.global.language, 0,
           sizeof(runtime.ui.global.language));
    if (!gp100_firmware_renderer_render(
            &renderer, &canvas, &runtime, &context)) {
        return 13;
    }
    runtime.ui.global.view = GP100_GLOBAL_VIEW_FACTORY_RESET;
    gp100_factory_reset_init(&runtime.ui.global.factory_reset);
    if (!gp100_firmware_renderer_render(
            &renderer, &canvas, &runtime, &context)) {
        return 14;
    }
    runtime.ui.global.view =
        GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION;
    gp100_expression_calibration_init(
        &runtime.ui.global.expression_calibration);
    if (!gp100_firmware_renderer_render(
            &renderer, &canvas, &runtime, &context)) {
        return 15;
    }
    if (gp100_firmware_renderer_init(NULL, font) ||
        gp100_firmware_renderer_init(&renderer, NULL) ||
        gp100_firmware_renderer_render(
            NULL, &canvas, &runtime, &context) ||
        gp100_firmware_renderer_render(
            &renderer, NULL, &runtime, &context) ||
        gp100_firmware_renderer_render(
            &renderer, &canvas, NULL, &context)) {
        return 20;
    }
    return 0;
}
