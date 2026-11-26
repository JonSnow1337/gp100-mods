#include "ui/screens/edit_screen.h"
#include "ui/ui_palette.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8);
    bytes[offset + 2u] = (uint8_t)(value >> 16);
    bytes[offset + 3u] = (uint8_t)(value >> 24);
}

static bool make_preset(gp100_preset_record_t *preset)
{
    uint16_t positions[GP100_EFFECT_FAMILY_COUNT];
    size_t assignment;
    size_t family;

    memset(preset, 0, sizeof(*preset));
    memcpy(&preset->bytes[2], "It's GP-100", 11u);
    for (family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        uint32_t algorithm_id = gp100_effect_default_algorithm_id(
            (gp100_effect_family_t)family);
        positions[family] = (uint16_t)family;
        write_u32(preset->bytes, 0x020u + family * 4u, algorithm_id);
        write_u16(preset->bytes, 0x260u + family * 2u,
                  family == GP100_EFFECT_PRE_FX ? 1u : 0u);
    }
    for (assignment = 0u;
         assignment < GP100_STOMP_FOOTSWITCH_COUNT *
                          GP100_STOMP_ASSIGNMENTS_PER_FOOTSWITCH;
         ++assignment) {
        write_u16(preset->bytes, 0x276u + assignment * 2u, UINT16_MAX);
    }
    return gp100_preset_set_effect_positions(preset, positions);
}

int main(void)
{
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    gp100_preset_record_t preset;
    gp100_settings_record_t settings = {{0}};
    gp100_edit_screen_snapshot_t snapshot;
    gp100_edit_screen_theme_t theme;
    gp100_display_canvas_t canvas;
    size_t page;
    size_t slot;
    gp100_edit_controller_t controller;
    bool enabled;
    size_t position;
    uint32_t algorithm_id;
    float parameter;
    bool drum_enabled = false;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_EDIT;
    gp100_event_t event = {0u, 0u};
    uint16_t logical_preset = 0u;

    if (!make_preset(&preset) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_PATCH) ||
        !gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, 0u, false, true)) {
        return 1;
    }
    if (snapshot.logical_preset != 0u ||
        strcmp(snapshot.preset_name, "It's GP-100") != 0 ||
        snapshot.selected_family != GP100_EFFECT_PRE_FX ||
        snapshot.chain[8] != GP100_EFFECT_REVERB ||
        !snapshot.effect_enabled[GP100_EFFECT_PRE_FX] ||
        snapshot.algorithm_name == NULL || snapshot.page_count != 1u ||
        snapshot.slots[0].kind != GP100_EDIT_SLOT_EFFECT_ENABLED ||
        strcmp(snapshot.slots[0].value, "ON") != 0 ||
        snapshot.slots[1].kind != GP100_EDIT_SLOT_PARAMETER ||
        snapshot.slots[1].parameter_index != 0u ||
        snapshot.slots[2].kind != GP100_EDIT_SLOT_PARAMETER ||
        snapshot.slots[2].parameter_index != 1u ||
        !snapshot.preset_modified) {
        return 2;
    }
    if (!gp100_edit_screen_parameter_page(
            GP100_FOOTSWITCH_MODE_PATCH, 0u, &page, &slot) ||
        page != 0u || slot != 1u ||
        !gp100_edit_screen_parameter_page(
            GP100_FOOTSWITCH_MODE_PATCH, 1u, &page, &slot) ||
        page != 0u || slot != 2u ||
        !gp100_edit_screen_parameter_page(
            GP100_FOOTSWITCH_MODE_STOMP, 0u, &page, &slot) ||
        page != 0u || slot != 2u ||
        gp100_edit_screen_parameter_page(
            (gp100_footswitch_mode_t)2, 0u, &page, &slot)) {
        return 3;
    }
    if (!gp100_preset_set_control_assignment(
            &preset, 1u, 0u, GP100_EFFECT_PRE_FX) ||
        !gp100_edit_screen_snapshot_from_preset(
            &snapshot, 98u, &preset, GP100_FOOTSWITCH_MODE_STOMP,
            0u, 0u, false, false) ||
        snapshot.stomp_switch != 1 || snapshot.page_count != 2u ||
        snapshot.slots[1].kind != GP100_EDIT_SLOT_STOMP_SWITCH ||
        strcmp(snapshot.slots[1].value, "FS2") != 0) {
        return 4;
    }
    memset(pixels, 0xa5, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_edit_screen_default_theme(
            &theme, &empty_font, &empty_font, NULL) ||
        !gp100_edit_screen_render(&canvas, &snapshot, &theme)) {
        return 5;
    }
    if (pixels[0] != theme.background_rgb565 ||
        theme.chain_selection_rgb565 != GP100_UI_RGB565(255, 76, 76) ||
        pixels[36u * GP100_UI_LOGICAL_WIDTH + 1u] !=
            theme.chain_selection_rgb565 ||
        pixels[37u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.chain_selection_rgb565 ||
        pixels[36u * GP100_UI_LOGICAL_WIDTH + 41u] != theme.disabled_rgb565 ||
        pixels[86u * GP100_UI_LOGICAL_WIDTH] != theme.panel_rgb565 ||
        pixels[41u * GP100_UI_LOGICAL_WIDTH + 6u] !=
            theme.effect_rgb565[GP100_EFFECT_PRE_FX]) {
        return 6;
    }
    if (!gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            8u, 0u, true, false) ||
        !gp100_edit_screen_render(&canvas, &snapshot, &theme) ||
        pixels[36u * GP100_UI_LOGICAL_WIDTH + 1u] !=
            theme.chain_selection_rgb565) {
        return 7;
    }
    if (gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            9u, 0u, false, false) ||
        gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, 9u, false, false) ||
        gp100_edit_screen_default_theme(NULL, NULL, NULL, NULL)) {
        return 8;
    }
    gp100_edit_controller_init(&controller);
    if (gp100_edit_controller_move(&controller, true) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        controller.selected_position != 1u || controller.page_index != 0u ||
        gp100_edit_controller_move(&controller, false) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        controller.selected_position != 0u ||
        gp100_edit_controller_next_page(
            &controller, &preset, GP100_FOOTSWITCH_MODE_STOMP) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        controller.page_index != 1u ||
        gp100_edit_controller_next_page(
            &controller, &preset, GP100_FOOTSWITCH_MODE_STOMP) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        controller.page_index != 0u) {
        return 9;
    }
    if (gp100_edit_controller_next_page(
            &controller, &preset, GP100_FOOTSWITCH_MODE_STOMP) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        gp100_edit_controller_move(&controller, false) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        controller.selected_position != 0u || controller.page_index != 0u) {
        return 9;
    }
    if (gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, false, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_PRE_FX, &enabled) || enabled ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            1u, true, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_preset_algorithm_id(
            &preset, GP100_EFFECT_PRE_FX, &algorithm_id) ||
        gp100_effect_algorithm_index(
            GP100_EFFECT_PRE_FX, algorithm_id) != 0 ||
        !gp100_preset_parameter(
            &preset, GP100_EFFECT_PRE_FX, 0u, &parameter) || parameter != 1.0f ||
        gp100_edit_controller_set_chain_management(&controller, true) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            1u, true, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_preset_algorithm_id(
            &preset, GP100_EFFECT_PRE_FX, &algorithm_id) ||
        gp100_effect_algorithm_index(GP100_EFFECT_PRE_FX, algorithm_id) != 1 ||
        gp100_edit_controller_set_chain_management(&controller, false) !=
            GP100_EDIT_RESULT_NAVIGATED ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            1u, true, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_preset_parameter(
            &preset, GP100_EFFECT_PRE_FX, 0u, &parameter) ||
        parameter != 21.0f ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID) {
        return 10;
    }
    if (!gp100_preset_set_control_assignment(
            &preset, 1u, 0u, GP100_EFFECT_PRE_FX) ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_STOMP,
            1u, false, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_edit_screen_snapshot_from_preset(
            &snapshot, 0u, &preset, GP100_FOOTSWITCH_MODE_STOMP,
            0u, 0u, false, true) || snapshot.stomp_switch != 0) {
        return 11;
    }
    if (gp100_edit_controller_set_chain_management(
            &controller, true) != GP100_EDIT_RESULT_NAVIGATED ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            2u, true, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        controller.selected_position != 1u ||
        !gp100_preset_effect_position(
            &preset, GP100_EFFECT_PRE_FX, &position) ||
        position != 1u ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID) {
        return 12;
    }
    if (gp100_edit_controller_adjust(
            NULL, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, true, 1u) != GP100_EDIT_RESULT_INVALID ||
        gp100_edit_controller_next_page(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH) !=
            GP100_EDIT_RESULT_INVALID) {
        return 13;
    }
    gp100_edit_controller_init(&controller);
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, true, 1u);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_NAVIGATED ||
        controller.selected_position != 1u) {
        return 14;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_NAVIGATED ||
        !controller.manage_signal_chain || controller.selected_position != 0u) {
        return 15;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_UNHANDLED ||
        !controller.manage_signal_chain) {
        return 16;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_3, true, 1u);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_PRESET_CHANGED ||
        !controller.manage_signal_chain || controller.selected_position != 1u ||
        !gp100_preset_effect_position(
            &preset, GP100_EFFECT_PRE_FX, &position) || position != 0u) {
        return 16;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_NAVIGATED ||
        controller.manage_signal_chain || controller.selected_position != 1u) {
        return 16;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_DRUM_TOGGLED || !drum_enabled) {
        return 17;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 18;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_RELEASE);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_PRESET_SELECTED ||
        logical_preset != 1u) {
        return 19;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_REPEAT);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_PRESET_SELECTED ||
        logical_preset != 2u ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            GP100_FOOTSWITCH_MODE_STOMP) ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_PRE_FX, &enabled)) {
        return 21;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_REPEAT);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_STOMP,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_PRESET_CHANGED) {
        return 22;
    }
    {
        bool toggled;

        if (!gp100_preset_effect_enabled(
                &preset, GP100_EFFECT_PRE_FX, &toggled) ||
            toggled == enabled) {
            return 23;
        }
    }
    event.code = UINT32_C(0x40030000);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_LOOPER ||
        gp100_edit_controller_handle_event(
            NULL, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH,
            &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_INVALID) {
        return 20;
    }
    if (!gp100_preset_effect_position(
            &preset, GP100_EFFECT_CABINET, &position) ||
        !gp100_preset_set_effect_enabled(
            &preset, GP100_EFFECT_CABINET, true)) {
        return 24;
    }
    gp100_edit_controller_init(&controller);
    controller.selected_position = position;
    event.code = gp100_encoder_event_code(GP100_ENCODER_QUICK_1, false, 1u);
    if (gp100_edit_controller_handle_event(
            &controller, &preset, &settings, &logical_preset,
            GP100_FOOTSWITCH_MODE_PATCH, &drum_enabled, &event, &next_mode) !=
            GP100_EDIT_EVENT_PRESET_CHANGED ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_CABINET, &enabled) || enabled ||
        gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
        gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, true, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        gp100_edit_controller_set_chain_management(
            &controller, true) != GP100_EDIT_RESULT_NAVIGATED ||
        !gp100_preset_select_algorithm(
            &preset, GP100_EFFECT_AMPLIFIER, 46u)) {
        return 24;
    }
    controller.selected_position = position;
    if (gp100_edit_controller_adjust(
            &controller, &preset, GP100_FOOTSWITCH_MODE_PATCH,
            0u, false, 1u) != GP100_EDIT_RESULT_PRESET_CHANGED ||
        !gp100_preset_effect_enabled(
            &preset, GP100_EFFECT_CABINET, &enabled) || enabled) {
        return 25;
    }
    return 0;
}
