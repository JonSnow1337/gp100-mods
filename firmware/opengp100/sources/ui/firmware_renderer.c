#include "firmware_renderer.h"

#include "ui/render/mode_chip.h"
#include "ui/screens/guide_screen.h"

#include "ui_palette.h"

#include <string.h>

static const char *text_or(const char *text, const char *fallback)
{
    return text != NULL ? text : fallback;
}

bool gp100_firmware_renderer_init(
    gp100_firmware_renderer_t *renderer,
    const gp100_bitmap_font_t *font)
{
    return gp100_firmware_renderer_init_with_fonts(
        renderer, font, font, font);
}

/* GP100_UI_ASSET_REPLACEMENT: bind the heading, text and compact font roles. */
bool gp100_firmware_renderer_init_with_fonts(
    gp100_firmware_renderer_t *renderer,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (renderer == NULL || large_font == NULL || regular_font == NULL ||
        small_font == NULL) {
        return false;
    }
    memset(renderer, 0, sizeof(*renderer));
    if (!gp100_play_screen_default_theme(
            &renderer->play, large_font, regular_font, small_font) ||
        !gp100_tuner_screen_default_theme(
            &renderer->tuner, regular_font, large_font) ||
        !gp100_drum_screen_default_theme(
            &renderer->drum, regular_font, small_font) ||
        !gp100_looper_screen_default_theme(
            &renderer->looper, regular_font, small_font) ||
        !gp100_edit_screen_default_theme(
            &renderer->edit, large_font, regular_font, small_font) ||
        !gp100_global_screen_default_theme(
            &renderer->global, large_font, regular_font, small_font) ||
        !gp100_expression_settings_default_theme(
            &renderer->expression, regular_font, small_font) ||
        !gp100_language_screen_default_theme(
            &renderer->language, regular_font, small_font) ||
        !gp100_factory_reset_default_theme(
            &renderer->factory_reset, regular_font, small_font) ||
        !gp100_save_screen_default_theme(
            &renderer->save, large_font, regular_font, small_font)) {
        return false;
    }
    renderer->hardware_test.regular_font = regular_font;
    renderer->hardware_test.small_font = small_font;
    renderer->hardware_test.foreground_rgb565 = GP100_UI_FOREGROUND;
    renderer->hardware_test.background_rgb565 = GP100_UI_BACKGROUND;
    renderer->hardware_test.accent_rgb565 = GP100_UI_SUCCESS;
    return true;
}

static bool render_play(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime)
{
    gp100_play_screen_snapshot_t snapshot;

    const bool built = gp100_play_screen_snapshot_from_records(
               &snapshot, runtime->ui.logical_preset,
               &runtime->preset, &runtime->settings,
               runtime->preset_modified, runtime->ui.drum_enabled);
    snapshot.unsaved_warning = runtime->ui.unsaved_warning && runtime->preset_modified;
    return built &&
           gp100_play_screen_render(
               canvas, &snapshot, &renderer->play);
}

static bool render_drum(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime)
{
    gp100_drum_screen_snapshot_t snapshot;

    return gp100_drum_screen_snapshot_from_settings(
               &snapshot, &runtime->settings,
               runtime->ui.drum_enabled) &&
           gp100_drum_screen_render(
               canvas, &snapshot, &renderer->drum);
}

static bool render_looper(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime)
{
    char preset_name[GP100_PRESET_NAME_SIZE + 1u];
    gp100_looper_screen_snapshot_t snapshot = {
        &runtime->looper,
        preset_name,
        runtime->ui.logical_preset,
        runtime->ui.looper_clear_feedback,
    };

    gp100_preset_name(&runtime->preset, preset_name);
    return gp100_looper_screen_render(
        canvas, &snapshot, &renderer->looper);
}

static bool render_edit(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime)
{
    gp100_edit_screen_snapshot_t snapshot;
    int16_t mode;

    return gp100_settings_get_i16(
               &runtime->settings,
               GP100_SETTINGS_FOOTSWITCH_MODE, &mode) &&
           (mode == GP100_FOOTSWITCH_MODE_PATCH ||
            mode == GP100_FOOTSWITCH_MODE_STOMP) &&
           gp100_edit_screen_snapshot_from_preset(
               &snapshot, runtime->ui.logical_preset,
               &runtime->preset, (gp100_footswitch_mode_t)mode,
               runtime->ui.edit.selected_position,
               runtime->ui.edit.page_index,
               runtime->ui.edit.manage_signal_chain,
               runtime->preset_modified) &&
           gp100_edit_screen_render(
               canvas, &snapshot, &renderer->edit);
}

static bool render_global(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime,
    const gp100_firmware_render_context_t *context)
{
    const gp100_global_controller_t *global = &runtime->ui.global;

    switch (global->view) {
    case GP100_GLOBAL_VIEW_MENU: {
        gp100_global_screen_snapshot_t snapshot;
        const char *version = context == NULL
                                  ? "development"
                                  : text_or(context->firmware_version,
                                            "development");

        return gp100_global_controller_snapshot(
                   global, &snapshot, &runtime->settings, version) &&
               gp100_global_screen_render(
                   canvas, &snapshot, &renderer->global);
    }
    case GP100_GLOBAL_VIEW_GUIDE:
        return gp100_guide_screen_render(
            canvas, renderer->global.large_font, renderer->global.regular_font,
            renderer->global.small_font, global->guide_page);
    case GP100_GLOBAL_VIEW_EXPRESSION: {
        gp100_expression_settings_snapshot_t snapshot;

        return gp100_expression_settings_snapshot_from_preset(
                   &snapshot, &runtime->preset,
                   global->expression_item) &&
               gp100_expression_settings_render(
                   canvas, &snapshot, &renderer->expression);
    }
    case GP100_GLOBAL_VIEW_LANGUAGE: {
        gp100_language_screen_state_t language = global->language;

        if (language.catalog.count == 0u) {
            gp100_language_catalog_t catalog;
            int16_t current;

            if (!gp100_settings_get_i16(
                    &runtime->settings,
                    GP100_SETTINGS_LANGUAGE_INDEX, &current) ||
                current < 0 || current > UINT8_MAX) {
                current = 0;
            }
            gp100_language_catalog_from_mask(0u, &catalog);
            if (!gp100_language_screen_init(
                    &language, &catalog, (uint8_t)current)) {
                return false;
            }
        }
        return gp100_language_screen_render(
            canvas, &language, &renderer->language);
    }
    case GP100_GLOBAL_VIEW_FACTORY_RESET:
        return gp100_factory_reset_render(
            canvas, &global->factory_reset, &renderer->factory_reset);
    case GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION:
        return gp100_expression_calibration_render(
            canvas, &global->expression_calibration,
            &renderer->expression);
    default:
        return false;
    }
}

static bool render_hardware_test(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime,
    const gp100_firmware_render_context_t *context)
{
    gp100_hardware_test_snapshot_t snapshot = {
        context == NULL ? "GP-100" :
            text_or(context->hardware_info, "GP-100"),
        context == NULL ? "development" :
            text_or(context->hardware_variant, "development"),
        context == NULL ? "development" :
            text_or(context->firmware_version, "development"),
        context == NULL ? 0 : context->tuner_value,
        context == NULL ? 0 : context->pedal_value,
        context == NULL ? 0u : context->encoder_id,
        &runtime->ui.hardware_test,
    };

    return gp100_hardware_test_render(
        canvas, &snapshot, &renderer->hardware_test);
}

static bool render_screen(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime,
    const gp100_firmware_render_context_t *context)
{
    if (renderer == NULL || canvas == NULL || runtime == NULL) {
        return false;
    }
    switch (runtime->ui.mode) {
    case GP100_UI_MODE_PLAY:
        return render_play(renderer, canvas, runtime);
    case GP100_UI_MODE_TUNER:
        return gp100_tuner_screen_render(
            canvas, &runtime->ui.tuner, &renderer->tuner);
    case GP100_UI_MODE_DRUM:
        return render_drum(renderer, canvas, runtime);
    case GP100_UI_MODE_LOOPER:
        return render_looper(renderer, canvas, runtime);
    case GP100_UI_MODE_CONSTRUCT:
    case GP100_UI_MODE_EDIT:
        return gp100_patch_is_native(&runtime->preset)
            ? gp100_patch_editor_render(&runtime->ui.patch_editor, runtime->ui.mode,
                                        &runtime->preset, runtime->ui.logical_preset,
                                        runtime->preset_modified, canvas, &renderer->edit)
            : render_edit(renderer, canvas, runtime);
    case GP100_UI_MODE_GLOBAL:
        return render_global(renderer, canvas, runtime, context);
    case GP100_UI_MODE_SAVE:
        return gp100_save_screen_render(
            canvas, &runtime->ui.save,
            context == NULL ? "" :
                text_or(context->save_destination_name, ""),
            &renderer->save);
    case GP100_UI_MODE_HARDWARE_TEST:
        return render_hardware_test(renderer, canvas, runtime, context);
    default:
        return false;
    }
}

bool gp100_firmware_renderer_render(
    const gp100_firmware_renderer_t *renderer,
    gp100_display_canvas_t *canvas,
    const gp100_firmware_runtime_t *runtime,
    const gp100_firmware_render_context_t *context)
{
    if (!render_screen(renderer, canvas, runtime, context)) {
        return false;
    }
    /* The editor draws its own chip (it also names the picker and menu states). */
    if (runtime->ui.mode == GP100_UI_MODE_EDIT || runtime->ui.mode == GP100_UI_MODE_CONSTRUCT) {
        return true;
    }
    const char *label = gp100_mode_chip_label(runtime->ui.mode);
    return label == NULL ||
           gp100_mode_chip_draw(canvas, renderer->global.small_font, label,
                                gp100_mode_chip_color(runtime->ui.mode),
                                renderer->global.background_rgb565, 158, 1);
}
