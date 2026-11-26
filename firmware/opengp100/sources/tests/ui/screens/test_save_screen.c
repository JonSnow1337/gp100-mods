#include "ui/screens/save_screen.h"
#include "ui/ui_runtime.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

static bool test_save_return_modes(gp100_preset_record_t *preset)
{
    const gp100_ui_mode_t origin_modes[] = {
        GP100_UI_MODE_PLAY, GP100_UI_MODE_DRUM,
        GP100_UI_MODE_EDIT, GP100_UI_MODE_GLOBAL,
        GP100_UI_MODE_LOOPER
    };
    gp100_settings_record_t settings = {0};
    gp100_looper_engine_t looper = {0};

    for (size_t i = 0u; i < sizeof(origin_modes) / sizeof(origin_modes[0]); ++i) {
        const gp100_ui_mode_t origin = origin_modes[i];
        const gp100_ui_mode_t expected =
            origin == GP100_UI_MODE_LOOPER ? GP100_UI_MODE_PLAY : origin;
        gp100_ui_runtime_t runtime;
        gp100_event_t event = {
            .code = gp100_button_event_code(
                GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE)
        };
        uint32_t updates;

        if (!gp100_ui_runtime_init(&runtime, origin, 0u, false, true,
                                        preset, &settings) ||
            !gp100_ui_runtime_handle_event(
                &runtime, preset, &settings, &looper, &event, &updates) ||
            runtime.mode != GP100_UI_MODE_SAVE) {
            return false;
        }
        event.code = gp100_button_event_code(
            GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
        if (!gp100_ui_runtime_handle_event(
                &runtime, preset, &settings, &looper, &event, &updates) ||
            runtime.mode != expected ||
            (updates & GP100_UI_UPDATE_SAVE_COMMIT) != 0u) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    gp100_preset_record_t current;
    gp100_preset_record_t saved;
    gp100_save_screen_state_t state;
    gp100_save_screen_theme_t theme;
    gp100_display_canvas_t canvas;
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    uint32_t address;
    char name[GP100_PRESET_NAME_SIZE + 1u];
    bool drum_enabled = false;
    gp100_ui_mode_t next_mode = GP100_UI_MODE_SAVE;
    gp100_event_t event = {0u, 0u};

    memset(&current, 0, sizeof(current));
    if (!gp100_preset_set_name(&current, "It's GP-100") ||
        !gp100_save_screen_init(&state, 99u, &current)) {
        return 1;
    }
    if (state.destination != 0u || state.cursor != 0u ||
        state.letter_index != 0u || strcmp(state.name, "It's GP-100") != 0 ||
        strlen(gp100_save_alphabet) != GP100_SAVE_ALPHABET_SIZE) {
        return 2;
    }
    if (!gp100_save_screen_adjust_letter(&state, 1) ||
        state.name[0] != 'B' || state.letter_index != 1u ||
        !gp100_save_screen_adjust_letter(&state, -2) ||
        state.name[0] != '\\' ||
        state.letter_index != GP100_SAVE_ALPHABET_SIZE - 1u) {
        return 3;
    }
    if (!gp100_save_screen_move_cursor(&state, 50) ||
        state.cursor != GP100_SAVE_NAME_CHARACTERS - 1u ||
        !gp100_save_screen_delete(&state, -1) ||
        state.cursor != GP100_SAVE_NAME_CHARACTERS - 2u ||
        state.name[state.cursor] != ' ') {
        return 4;
    }
    if (!gp100_save_screen_select_destination(&state, -1) ||
        state.destination != GP100_PRESETS_PER_BANK - 1u ||
        !gp100_save_screen_select_destination(&state, 2) ||
        state.destination != 1u) {
        return 5;
    }
    memcpy(state.name, "Saved Name ", 11u);
    state.name[11] = '\0';
    if (!gp100_save_screen_prepare_record(
            &state, &current, &saved, &address) ||
        address != GP100_USER_PRESET_BASE + GP100_PRESET_RECORD_SIZE ||
        gp100_preset_validate(&saved) != GP100_PRESET_VALID) {
        return 6;
    }
    gp100_preset_name(&saved, name);
    if (strcmp(name, "Saved Name") != 0) {
        return 7;
    }
    memset(pixels, 0xa5, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_save_screen_default_theme(
            &theme, &empty_font, &empty_font, NULL) ||
        !gp100_save_screen_render(
            &canvas, &state, "Old Name", &theme)) {
        return 8;
    }
    if (pixels[0] != theme.background_rgb565 ||
        pixels[94u * GP100_UI_LOGICAL_WIDTH + 3u] !=
            theme.panel_rgb565 ||
        pixels[94u * GP100_UI_LOGICAL_WIDTH + 57u] !=
            theme.panel_rgb565 ||
        pixels[94u * GP100_UI_LOGICAL_WIDTH + 111u] !=
            theme.panel_rgb565) {
        return 9;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_QUICK_1, true, 1u);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_STATE_CHANGED) {
        return 10;
    }
    event.code = gp100_encoder_event_code(
        GP100_ENCODER_PARA, false, 1u);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_STATE_CHANGED ||
        state.destination != 0u) {
        return 11;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_COMMIT_REQUEST ||
        next_mode != GP100_UI_MODE_PLAY) {
        return 12;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_DRUM, GP100_BUTTON_RELEASE);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_DRUM_TOGGLED || !drum_enabled) {
        return 13;
    }
    event.code = gp100_button_event_code(
        GP100_BUTTON_GLOBAL, GP100_BUTTON_RELEASE);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_MODE_CHANGED ||
        next_mode != GP100_UI_MODE_GLOBAL) {
        return 14;
    }
    event.code = UINT32_C(0x40000000);
    if (gp100_save_screen_handle_event(
            &state, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_UNHANDLED ||
        gp100_save_screen_handle_event(
            NULL, &drum_enabled, &event, &next_mode) !=
            GP100_SAVE_EVENT_INVALID) {
        return 15;
    }
    if (gp100_save_screen_init(NULL, 0u, &current) ||
        gp100_save_screen_prepare_record(
            NULL, &current, &saved, &address) ||
        gp100_save_screen_default_theme(NULL, NULL, NULL, NULL) ||
        gp100_save_screen_render(NULL, &state, "", &theme)) {
        return 16;
    }
    if (!test_save_return_modes(&current)) {
        return 17;
    }
    return 0;
}
