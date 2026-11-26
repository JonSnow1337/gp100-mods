#include "ui/screens/global_screen.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

static void write_float(uint8_t *bytes, size_t offset, float value)
{
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    bytes[offset] = (uint8_t)word;
    bytes[offset + 1u] = (uint8_t)(word >> 8);
    bytes[offset + 2u] = (uint8_t)(word >> 16);
    bytes[offset + 3u] = (uint8_t)(word >> 24);
}

int main(void)
{
    gp100_settings_record_t settings;
    gp100_preset_record_t preset;
    gp100_global_controller_t controller;
    gp100_global_screen_snapshot_t snapshot;
    gp100_global_screen_theme_t theme;
    gp100_display_canvas_t canvas;
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    int16_t adjusted_value;
    size_t index;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_GLOBAL;
    gp100_event_t event = {0u, 0u};
    gp100_volume_pedal_position_t pedal_position;
    float range_minimum;
    float range_maximum;

    memset(&settings, 0, sizeof(settings));
    memset(&preset, 0, sizeof(preset));
    if (!gp100_preset_set_control_range(
            &preset, GP100_CONTROL_RANGE_VOLUME, 10.0f, 90.0f) ||
        !gp100_preset_set_volume_pedal_position(
            &preset, GP100_VOLUME_PEDAL_POST)) {
        return 1;
    }
    gp100_global_controller_init(&controller);
    if (gp100_global_controller_factory_page(&controller) != 1u ||
        gp100_global_controller_move(&controller, false) !=
            GP100_GLOBAL_NAVIGATION_UNCHANGED ||
        gp100_global_controller_move(&controller, true) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 2u) {
        return 1;
    }
    controller.menu_item = GP100_GLOBAL_EXPRESSION_SETTING;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 12u ||
        gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UNCHANGED ||
        gp100_global_controller_move(&controller, false) !=
            GP100_GLOBAL_NAVIGATION_UNCHANGED ||
        gp100_global_controller_move(&controller, true) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 13u ||
        gp100_global_controller_move(&controller, true) !=
            GP100_GLOBAL_NAVIGATION_UNCHANGED ||
        gp100_global_controller_exit(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        controller.menu_item != GP100_GLOBAL_EXPRESSION_SETTING) {
        return 1;
    }
    controller.menu_item = GP100_GLOBAL_EQ;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        controller.page_index != 1u) {
        return 1;
    }
    controller.menu_item = GP100_GLOBAL_LANGUAGE;
    controller.page_index = 0u;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 14u ||
        gp100_global_controller_exit(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        controller.menu_item != GP100_GLOBAL_LANGUAGE) {
        return 1;
    }
    controller.menu_item = GP100_GLOBAL_FACTORY_RESET;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 15u ||
        gp100_global_controller_exit(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED) {
        return 1;
    }
    controller.menu_item = GP100_GLOBAL_EXPRESSION_SETTING;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED) {
        return 1;
    }
    controller.expression_item = GP100_EXPRESSION_CALIBRATE;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 16u ||
        gp100_global_controller_exit(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        gp100_global_controller_factory_page(&controller) != 13u ||
        gp100_global_controller_move(&controller, true) !=
            GP100_GLOBAL_NAVIGATION_UNCHANGED ||
        gp100_global_controller_factory_page(NULL) != 0u ||
        gp100_global_controller_move(NULL, true) !=
            GP100_GLOBAL_NAVIGATION_INVALID ||
        gp100_global_controller_activate(NULL) !=
            GP100_GLOBAL_NAVIGATION_INVALID ||
        gp100_global_controller_exit(NULL) !=
            GP100_GLOBAL_NAVIGATION_INVALID) {
        return 1;
    }
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_INPUT_LEVEL_DB, -3) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_NO_CAB_LEFT, 1) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_NO_CAB_RIGHT, 0) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TAP_SYNC_PRE, 1) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TAP_SYNC_MODULATION, 0) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_TAP_SYNC_DELAY, 1) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_USB_RECORD_LEVEL_DB, 2) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_USB_MONITOR_LEVEL_DB, -6) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT, 0) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT, 1) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_STOMP)) {
        return 2;
    }
    if (!gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_IO, 0u, "2.1.0") ||
        snapshot.page_count != 1u || snapshot.first_visible_item != 0u ||
        strcmp(snapshot.controls[0].value, "-3dB") != 0 ||
        strcmp(snapshot.controls[1].value, "ON") != 0 ||
        strcmp(snapshot.controls[2].value, "OFF") != 0) {
        return 3;
    }
    controller.view = GP100_GLOBAL_VIEW_MENU;
    controller.menu_item = GP100_GLOBAL_IO;
    controller.page_index = 0u;
    if (!gp100_global_controller_snapshot(
            &controller, &snapshot, &settings, "2.1.0") ||
        gp100_global_controller_exit(&controller) !=
            GP100_GLOBAL_NAVIGATION_EXIT_TO_PLAY) {
        return 3;
    }
    if (!gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_IO, 0u, 0u, true) ||
        !gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_IO, 0u, 1u, false) ||
        !gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_TAP_TEMPO_MODE, 0u, 1u, true) ||
        !gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_USB_AUDIO, 0u, 1u, true) ||
        !gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_USB_AUDIO, 1u, 0u, false) ||
        !gp100_global_adjust_panel_setting(
            &settings, GP100_GLOBAL_FOOTSWITCH_MODE, 0u, 1u, false) ||
        !gp100_settings_validate(&settings) ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_INPUT_LEVEL_DB, &adjusted_value) ||
        adjusted_value != -2) {
        return 3;
    }
    if (!gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_TAP_TEMPO_MODE,
            0u, NULL) ||
        strcmp(snapshot.controls[0].value, "OPEN") != 0 ||
        strcmp(snapshot.controls[1].value, "OPEN") != 0 ||
        strcmp(snapshot.controls[2].value, "OPEN") != 0) {
        return 4;
    }
    if (!gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_USB_AUDIO, 1u, NULL) ||
        snapshot.page_count != 2u ||
        strcmp(snapshot.controls[0].value, "DRY") != 0 ||
        snapshot.controls[1].present ||
        gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_USB_AUDIO, 2u, NULL)) {
        return 5;
    }
    for (index = 0u; index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        float value = index == 0u ? 1.0f
                      : index == 1u ? 80.0f
                      : index == 2u ? 12000.0f
                      : index % 3u == 1u ? 0.7f
                      : index % 3u == 2u ? -2.0f
                      : 500.0f;
        write_float(settings.bytes, 0x44u + index * 4u, value);
    }
    if (!gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_EQ, 0u, NULL) ||
        snapshot.page_count != 5u ||
        strcmp(snapshot.controls[0].value, "ON") != 0 ||
        strcmp(snapshot.controls[1].value, "80Hz") != 0 ||
        !gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_EQ, 1u, NULL) ||
        strcmp(snapshot.controls[0].label, "B1 FREQ") != 0 ||
        strcmp(snapshot.controls[1].value, "0.7") != 0 ||
        strcmp(snapshot.controls[2].value, "-2dB") != 0) {
        return 6;
    }
    if (!gp100_global_adjust_equalizer(
            &settings, 0u, 1u, false, 2u) ||
        !gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_EQ, 0u, NULL) ||
        strcmp(snapshot.controls[1].value, "OFF") != 0 ||
        !gp100_global_adjust_equalizer(
            &settings, 1u, 1u, true, 2u) ||
        !gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_EQ, 1u, NULL) ||
        strcmp(snapshot.controls[1].value, "1.0") != 0 ||
        !gp100_settings_validate(&settings) ||
        gp100_global_adjust_equalizer(
            &settings, 5u, 0u, true, 1u)) {
        return 6;
    }
    if (!gp100_global_screen_snapshot_from_settings(
            &snapshot, &settings, GP100_GLOBAL_ABOUT, 0u, "2.1.0") ||
        snapshot.first_visible_item != GP100_GLOBAL_MENU_ITEMS - GP100_GLOBAL_VISIBLE_ITEMS ||
        strcmp(snapshot.detail, "2.1.0") != 0 ||
        gp100_global_menu_item_name(
            (gp100_global_menu_item_t)GP100_GLOBAL_MENU_ITEMS) !=
            NULL) {
        return 7;
    }
    memset(pixels, 0xa5, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_global_screen_default_theme(
            &theme, &empty_font, &empty_font, NULL) ||
        !gp100_global_screen_render(&canvas, &snapshot, &theme)) {
        return 8;
    }
    if (pixels[0] != theme.background_rgb565 ||
        pixels[81u * GP100_UI_LOGICAL_WIDTH] != theme.panel_rgb565 ||
        pixels[56u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.selected_rgb565 ||
        pixels[20u * GP100_UI_LOGICAL_WIDTH + 156u] !=
            theme.track_rgb565) {
        return 9;
    }
    controller.view = GP100_GLOBAL_VIEW_MENU;
    controller.menu_item = GP100_GLOBAL_OUTPUT_LIMITER;
    controller.page_index = 0u;
    if (!gp100_global_controller_snapshot(&controller, &snapshot, &settings, NULL) ||
        snapshot.limiter_enabled ||
        strcmp(snapshot.controls[0].value, "0dB") != 0 ||
        strcmp(snapshot.controls[1].value, "0dB") != 0 ||
        strcmp(snapshot.controls[2].value, "15dB/s") != 0) {
        return 30;
    }
    event.code = gp100_button_event_code(GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(&controller, &settings, &event, &next_mode) !=
        GP100_GLOBAL_EVENT_SETTINGS_CHANGED) {
        return 31;
    }
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_1, true, 14u);
    if (gp100_global_controller_handle_event(&controller, &settings, &event, &next_mode) !=
        GP100_GLOBAL_EVENT_SETTINGS_CHANGED) {
        return 32;
    }
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_2, true, 1u);
    if (gp100_global_controller_handle_event(&controller, &settings, &event, &next_mode) !=
        GP100_GLOBAL_EVENT_UNHANDLED) {
        return 33;
    }
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_2, false, 1u);
    if (gp100_global_controller_handle_event(&controller, &settings, &event, &next_mode) !=
        GP100_GLOBAL_EVENT_SETTINGS_CHANGED) {
        return 34;
    }
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_3, false, 4u);
    if (gp100_global_controller_handle_event(&controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_SETTINGS_CHANGED ||
        !gp100_global_controller_snapshot(&controller, &snapshot, &settings, NULL) ||
        !snapshot.limiter_enabled || strcmp(snapshot.controls[0].value, "+12dB") != 0 ||
        strcmp(snapshot.controls[1].value, "-1dB") != 0 ||
        strcmp(snapshot.controls[2].value, "11dB/s") != 0 ||
        !gp100_global_screen_render(&canvas, &snapshot, &theme) ||
        !gp100_settings_validate(&settings)) {
        return 35;
    }
    controller.menu_item = GP100_GLOBAL_IO;
    controller.page_index = 0u;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_SETTINGS_CHANGED ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_INPUT_LEVEL_DB,
            &adjusted_value) || adjusted_value != -1) {
        return 10;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 1u);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.menu_item != GP100_GLOBAL_TAP_TEMPO_MODE) {
        return 11;
    }
    controller.menu_item = GP100_GLOBAL_USB_AUDIO;
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.page_index != 1u) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_SAVE ||
        gp100_global_controller_handle_event(
            NULL, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_INVALID) {
        return 14;
    }
    event.code = UINT32_C(0x40030000);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_LOOPER) {
        return 28;
    }
    controller.view = GP100_GLOBAL_VIEW_EXPRESSION;
    controller.menu_item = GP100_GLOBAL_EXPRESSION_SETTING;
    controller.expression_item = GP100_EXPRESSION_VOLUME_RANGE;
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_GLOBAL ||
        controller.view != GP100_GLOBAL_VIEW_EXPRESSION ||
        controller.menu_item != GP100_GLOBAL_EXPRESSION_SETTING ||
        controller.expression_item != GP100_EXPRESSION_VOLUME_RANGE) {
        return 29;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_DRUM_TOGGLE_REQUESTED) {
        return 30;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_EDIT) {
        return 31;
    }
    controller.view = GP100_GLOBAL_VIEW_EXPRESSION;
    controller.expression_item = GP100_EXPRESSION_VOLUME_RANGE;
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 2u);
    if (gp100_global_controller_handle_expression_event(
            &controller, &preset, &event) !=
            GP100_GLOBAL_EVENT_PRESET_CHANGED ||
        !gp100_preset_control_range(
            &preset, GP100_CONTROL_RANGE_VOLUME,
            &range_minimum, &range_maximum) ||
        range_minimum != 12.0f || range_maximum != 90.0f) {
        return 15;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_2, false, 1u);
    if (gp100_global_controller_handle_expression_event(
            &controller, &preset, &event) !=
            GP100_GLOBAL_EVENT_PRESET_CHANGED ||
        !gp100_preset_volume_pedal_position(
            &preset, &pedal_position) ||
        pedal_position != GP100_VOLUME_PEDAL_PRE) {
        return 16;
    }
    controller.view = GP100_GLOBAL_VIEW_LANGUAGE;
    controller.menu_item = GP100_GLOBAL_LANGUAGE;
    memset(&controller.language, 0, sizeof(controller.language));
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 2u);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.language.selected_index != 2u) {
        return 18;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_SETTINGS_CHANGED ||
        !controller.language.committed ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_LANGUAGE_INDEX,
            &adjusted_value) || adjusted_value != 2 ||
        gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.view != GP100_GLOBAL_VIEW_MENU ||
        controller.menu_item != GP100_GLOBAL_LANGUAGE) {
        return 19;
    }
    controller.view = GP100_GLOBAL_VIEW_MENU;
    controller.menu_item = GP100_GLOBAL_FACTORY_RESET;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED ||
        controller.factory_reset.phase !=
            GP100_FACTORY_RESET_CONFIRM) {
        return 20;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 1u);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        !controller.factory_reset.reset_selected) {
        return 21;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_FACTORY_RESET_REQUESTED ||
        controller.factory_reset.phase !=
            GP100_FACTORY_RESET_REQUESTED) {
        return 22;
    }
    next_mode = GP100_UI_MODE_GLOBAL;
    event.code = UINT32_C(0x40030000);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_UNHANDLED ||
        next_mode != GP100_UI_MODE_GLOBAL) {
        return 22;
    }
    if (!gp100_factory_reset_begin(
            &controller.factory_reset, 2u) ||
        !gp100_factory_reset_update(
            &controller.factory_reset, 2u)) {
        return 22;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.view != GP100_GLOBAL_VIEW_MENU) {
        return 22;
    }
    controller.view = GP100_GLOBAL_VIEW_EXPRESSION;
    controller.expression_item = GP100_EXPRESSION_CALIBRATE;
    if (gp100_global_controller_activate(&controller) !=
            GP100_GLOBAL_NAVIGATION_UPDATED) {
        return 23;
    }
    event.value = 0u;
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 10u;
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED) {
        return 24;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED) {
        return 25;
    }
    event.code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | 200u;
    (void)gp100_global_controller_handle_event(
        &controller, &settings, &event, &next_mode);
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    (void)gp100_global_controller_handle_event(
        &controller, &settings, &event, &next_mode);
    event.value = 1u;
    event.code = GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 240u;
    (void)gp100_global_controller_handle_event(
        &controller, &settings, &event, &next_mode);
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_CALIBRATION_COMMIT ||
        controller.expression_calibration.heel != 10u ||
        controller.expression_calibration.toe != 200u ||
        controller.expression_calibration.trigger != 240u) {
        return 26;
    }
    controller.expression_calibration.step = GP100_EXPRESSION_CALIBRATE_COMPLETE;
    if (gp100_global_controller_handle_event(
            &controller, &settings, &event, &next_mode) !=
            GP100_GLOBAL_EVENT_NAVIGATED ||
        controller.view != GP100_GLOBAL_VIEW_EXPRESSION) {
        return 28;
    }
    if (gp100_global_screen_snapshot_from_settings(
            NULL, &settings, GP100_GLOBAL_IO, 0u, NULL) ||
        gp100_global_screen_default_theme(NULL, NULL, NULL, NULL) ||
        gp100_global_screen_render(NULL, &snapshot, &theme)) {
        return 27;
    }
    return 0;
}
