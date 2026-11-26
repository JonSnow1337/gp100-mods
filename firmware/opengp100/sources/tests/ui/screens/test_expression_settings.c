#include "ui/screens/expression_settings.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

int main(void)
{
    gp100_preset_record_t preset = {0};
    gp100_expression_settings_snapshot_t snapshot;
    gp100_expression_settings_theme_t theme;
    gp100_expression_calibration_t calibration;
    gp100_display_canvas_t canvas;
    gp100_volume_pedal_position_t position;
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    float minimum;
    float maximum;
    bool committed;

    if (!gp100_preset_set_control_range(&preset, GP100_CONTROL_RANGE_VOLUME, 0.0f, 99.0f) ||
        !gp100_preset_set_volume_pedal_position(&preset, GP100_VOLUME_PEDAL_POST) ||
        !gp100_expression_adjust_volume_range(&preset, false, 7) ||
        !gp100_expression_adjust_volume_range(&preset, true, -4) ||
        !gp100_preset_control_range(&preset, GP100_CONTROL_RANGE_VOLUME, &minimum, &maximum) ||
        minimum != 7.0f || maximum != 95.0f) {
        return 1;
    }
    if (!gp100_expression_settings_snapshot_from_preset(
            &snapshot, &preset, GP100_EXPRESSION_VOLUME_RANGE) ||
        strcmp(snapshot.controls[0].label, "Min") != 0 ||
        strcmp(snapshot.controls[0].value, "7") != 0 ||
        strcmp(snapshot.controls[2].value, "95") != 0 ||
        strcmp(snapshot.controls[1].label, "Position") != 0 ||
        strcmp(snapshot.controls[1].value, "Post") != 0 ||
        !gp100_preset_set_volume_pedal_position(
            &preset, GP100_VOLUME_PEDAL_PRE) ||
        !gp100_preset_volume_pedal_position(&preset, &position) ||
        position != GP100_VOLUME_PEDAL_PRE) {
        return 4;
    }
    gp100_expression_calibration_init(&calibration);
    if (!gp100_expression_calibration_advance(
            &calibration, 10u, &committed) || committed ||
        !gp100_expression_calibration_advance(
            &calibration, 200u, &committed) || committed ||
        !gp100_expression_calibration_advance(
            &calibration, 240u, &committed) || !committed ||
        calibration.step != GP100_EXPRESSION_CALIBRATE_SWITCH ||
        calibration.heel != 10u || calibration.toe != 200u ||
        calibration.trigger != 240u) {
        return 5;
    }
    calibration.step = GP100_EXPRESSION_CALIBRATE_SAVE_FAILED;
    if (!gp100_expression_calibration_advance(
            &calibration, 0u, &committed) || !committed ||
        calibration.trigger != 240u) {
        return 9;
    }
    calibration.step = GP100_EXPRESSION_CALIBRATE_COMPLETE;
    if (!gp100_expression_calibration_advance(
            &calibration, 0u, &committed) || committed) {
        return 10;
    }
    gp100_expression_calibration_init(&calibration);
    (void)gp100_expression_calibration_advance(
        &calibration, 200u, &committed);
    (void)gp100_expression_calibration_advance(
        &calibration, 100u, &committed);
    (void)gp100_expression_calibration_advance(
        &calibration, 240u, &committed);
    if (calibration.step != GP100_EXPRESSION_CALIBRATE_FAILED ||
        !gp100_expression_calibration_advance(
            &calibration, 0u, &committed) || committed ||
        calibration.step != GP100_EXPRESSION_CALIBRATE_HEEL) {
        return 6;
    }
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_expression_settings_default_theme(
            &theme, &empty_font, NULL) ||
        !gp100_expression_settings_snapshot_from_preset(
            &snapshot, &preset, GP100_EXPRESSION_CALIBRATE) ||
        !gp100_expression_settings_render(&canvas, &snapshot, &theme) ||
        pixels[81u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.panel_rgb565) {
        return 7;
    }
    gp100_expression_calibration_init(&calibration);
    if (!gp100_expression_calibration_render(
            &canvas, &calibration, &theme) ||
        pixels[83u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.panel_rgb565 ||
        gp100_expression_menu_item_name(
            (gp100_expression_menu_item_t)
                GP100_EXPRESSION_MENU_ITEMS) != NULL) {
        return 8;
    }
    return 0;
}
