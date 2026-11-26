#include "ui/screens/tuner_screen.h"

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
    gp100_tuner_model_t model;
    gp100_tuner_screen_theme_t theme;
    gp100_settings_record_t settings;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_TUNER;
    gp100_event_t event = {0u, 0u};

    memset(pixels, 0xa5, sizeof(pixels));
    memset(&settings, 0, sizeof(settings));
    gp100_tuner_reset(&model);
    model.reading.valid = true;
    model.reading.note_letter = 'A';
    model.reading.needle_step = 0;
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_tuner_screen_default_theme(
            &theme, &empty_font, NULL) ||
        !gp100_tuner_screen_render(&canvas, &model, &theme)) {
        return 1;
    }
    if (pixels[0] != theme.background_rgb565 ||
        pixels[92u * GP100_UI_LOGICAL_WIDTH] !=
            theme.status_background_rgb565 ||
        pixels[60u * GP100_UI_LOGICAL_WIDTH + 55u] !=
            theme.in_tune_rgb565 ||
        pixels[60u * GP100_UI_LOGICAL_WIDTH + 105u] !=
            theme.in_tune_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 4u] !=
            theme.control_background_rgb565) {
        return 2;
    }

    model.reading.needle_step = -3;
    if (!gp100_tuner_screen_render(&canvas, &model, &theme) ||
        pixels[60u * GP100_UI_LOGICAL_WIDTH + 45u] !=
            theme.near_marker_rgb565 ||
        pixels[60u * GP100_UI_LOGICAL_WIDTH + 115u] !=
            theme.background_rgb565) {
        return 3;
    }
    model.reference_hz = 434u;
    if (gp100_tuner_screen_render(&canvas, &model, &theme) ||
        gp100_tuner_screen_default_theme(NULL, &empty_font, NULL)) {
        return 4;
    }
    if (gp100_display_bgr888_to_rgb565(0xffffffu) != 0xffffu ||
        gp100_display_bgr888_to_rgb565(0x00ff00u) != 0x07e0u ||
        gp100_display_bgr888_to_rgb565(0x0000ffu) != 0xf800u ||
        gp100_display_bgr888_to_rgb565(0xff0000u) != 0x001fu) {
        return 5;
    }
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TUNER_MODE,
            GP100_TUNER_OUTPUT_THRU) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TUNER_REFERENCE_HZ, 442) ||
        !gp100_tuner_screen_load_settings(&model, &settings) ||
        model.output_mode != GP100_TUNER_OUTPUT_THRU ||
        model.reference_hz != 442u ||
        !gp100_tuner_screen_adjust(
            &model, &settings, GP100_TUNER_SCREEN_OUTPUT_MODE, true) ||
        model.output_mode != GP100_TUNER_OUTPUT_BYPASS ||
        !gp100_tuner_screen_adjust(
            &model, &settings, GP100_TUNER_SCREEN_OUTPUT_MODE, true) ||
        model.output_mode != GP100_TUNER_OUTPUT_BYPASS ||
        !gp100_tuner_screen_adjust(
            &model, &settings, GP100_TUNER_SCREEN_REFERENCE_HZ,
            false) ||
        model.reference_hz != 441u ||
        !gp100_settings_validate(&settings)) {
        return 6;
    }
    if (gp100_tuner_screen_load_settings(NULL, &settings) ||
        gp100_tuner_screen_adjust(
            NULL, &settings, GP100_TUNER_SCREEN_OUTPUT_MODE, true) ||
        gp100_tuner_screen_adjust(
            &model, &settings, (gp100_tuner_screen_control_t)2,
            true)) {
        return 7;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_tuner_screen_handle_mode_event(&event, &next_mode) !=
            GP100_TUNER_SCREEN_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 8;
    }
    next_mode = GP100_UI_MODE_TUNER;
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_HOLD);
    if (gp100_tuner_screen_handle_mode_event(&event, &next_mode) !=
            GP100_TUNER_SCREEN_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 9;
    }
    next_mode = GP100_UI_MODE_TUNER;
    event.code = UINT32_C(0x40000000);
    if (gp100_tuner_screen_handle_mode_event(&event, &next_mode) !=
            GP100_TUNER_SCREEN_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 10;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, true, 1u);
    if (gp100_tuner_screen_handle_mode_event(&event, &next_mode) !=
            GP100_TUNER_SCREEN_MODE_UNHANDLED ||
        gp100_tuner_screen_handle_mode_event(NULL, &next_mode) !=
            GP100_TUNER_SCREEN_MODE_INVALID) {
        return 11;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 1u);
    if (gp100_tuner_screen_handle_event(
            &model, &settings, &event, &next_mode) !=
            GP100_TUNER_SCREEN_EVENT_SETTINGS_CHANGED ||
        model.reference_hz != 442u) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_tuner_screen_handle_event(
            &model, &settings, &event, &next_mode) !=
            GP100_TUNER_SCREEN_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 13;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, true, 1u);
    if (gp100_tuner_screen_handle_event(
            &model, &settings, &event, &next_mode) !=
            GP100_TUNER_SCREEN_EVENT_UNHANDLED ||
        gp100_tuner_screen_handle_event(
            NULL, &settings, &event, &next_mode) !=
            GP100_TUNER_SCREEN_EVENT_INVALID) {
        return 14;
    }
    return 0;
}
