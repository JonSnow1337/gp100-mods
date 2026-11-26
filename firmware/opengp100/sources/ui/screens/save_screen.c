#include "save_screen.h"

#include "ui/ui_palette.h"

#include <string.h>

const char gp100_save_alphabet[GP100_SAVE_ALPHABET_SIZE + 1u] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
    "~!@#$%^&*()_+-={}[];':\",.<>/\\";

static bool valid_state(const gp100_save_screen_state_t *state)
{
    return state != NULL &&
           state->cursor < GP100_SAVE_NAME_CHARACTERS &&
           state->letter_index < GP100_SAVE_ALPHABET_SIZE &&
           state->destination < GP100_PRESETS_PER_BANK &&
           state->name[GP100_SAVE_NAME_CHARACTERS] == '\0';
}

static void copy_save_name(char destination[GP100_SAVE_NAME_CHARACTERS + 1u],
                           const char *source)
{
    size_t index = 0u;

    while (index < GP100_SAVE_NAME_CHARACTERS &&
           source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
    while (++index <= GP100_SAVE_NAME_CHARACTERS) {
        destination[index] = '\0';
    }
}

bool gp100_save_screen_init(
    gp100_save_screen_state_t *state,
    uint16_t current_logical_preset,
    const gp100_preset_record_t *current_preset)
{
    char name[GP100_PRESET_NAME_SIZE + 1u];

    if (state == NULL || current_preset == NULL ||
        current_logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    gp100_preset_name(current_preset, name);
    copy_save_name(state->name, name);
    state->destination = current_logical_preset % GP100_PRESETS_PER_BANK;
    state->return_mode = GP100_UI_MODE_PLAY;
    return true;
}

static uint8_t wrapped_index(uint8_t current, int step, uint8_t count)
{
    int next = (int)current + step % (int)count;
    if (next < 0) {
        next += count;
    } else if (next >= count) {
        next -= count;
    }
    return (uint8_t)next;
}

bool gp100_save_screen_adjust_letter(
    gp100_save_screen_state_t *state,
    int step)
{
    if (!valid_state(state)) {
        return false;
    }
    state->letter_index = wrapped_index(
        state->letter_index, step, GP100_SAVE_ALPHABET_SIZE);
    state->name[state->cursor] = gp100_save_alphabet[state->letter_index];
    if (state->cursor + 1u < sizeof(state->name) &&
        state->name[state->cursor + 1u] == '\0') {
        state->name[state->cursor + 1u] = '\0';
    }
    return true;
}

static void extend_to_cursor(gp100_save_screen_state_t *state)
{
    size_t length = strlen(state->name);
    while (length <= state->cursor &&
           length < GP100_SAVE_NAME_CHARACTERS) {
        state->name[length++] = ' ';
        state->name[length] = '\0';
    }
}

bool gp100_save_screen_move_cursor(
    gp100_save_screen_state_t *state,
    int step)
{
    int next;

    if (!valid_state(state)) {
        return false;
    }
    next = (int)state->cursor + step;
    if (next < 0) {
        next = 0;
    } else if (next >= (int)GP100_SAVE_NAME_CHARACTERS) {
        next = (int)GP100_SAVE_NAME_CHARACTERS - 1;
    }
    state->cursor = (uint8_t)next;
    extend_to_cursor(state);
    return true;
}

bool gp100_save_screen_delete(
    gp100_save_screen_state_t *state,
    int direction)
{
    if (!gp100_save_screen_move_cursor(state, direction)) {
        return false;
    }
    state->name[state->cursor] = ' ';
    return true;
}

bool gp100_save_screen_select_destination(
    gp100_save_screen_state_t *state,
    int step)
{
    if (!valid_state(state)) {
        return false;
    }
    state->destination = wrapped_index(
        (uint8_t)state->destination, step, GP100_PRESETS_PER_BANK);
    return true;
}

bool gp100_save_screen_prepare_record(
    const gp100_save_screen_state_t *state,
    const gp100_preset_record_t *current_preset,
    gp100_preset_record_t *saved_preset,
    uint32_t *flash_address)
{
    char trimmed[GP100_SAVE_NAME_CHARACTERS + 1u];
    size_t length;

    if (!valid_state(state) || current_preset == NULL ||
        saved_preset == NULL || flash_address == NULL) {
        return false;
    }
    memcpy(trimmed, state->name, sizeof(trimmed));
    length = strlen(trimmed);
    while (length != 0u && trimmed[length - 1u] == ' ') {
        trimmed[--length] = '\0';
    }
    *saved_preset = *current_preset;
    if (!gp100_preset_set_name(saved_preset, trimmed) ||
        !gp100_preset_flash_address(state->destination,
                                         flash_address)) {
        return false;
    }
    gp100_preset_update_crc(saved_preset);
    return true;
}

gp100_save_event_result_t gp100_save_screen_handle_event(
    gp100_save_screen_state_t *state,
    bool *drum_enabled,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    int step;
    bool changed;

    if (state == NULL || drum_enabled == NULL || event == NULL ||
        next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_SAVE_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        step = decoded.action == GP100_BUTTON_PRESS ? 1 : -1;
        switch ((gp100_encoder_id_t)decoded.control) {
        case GP100_ENCODER_QUICK_1:
            changed = gp100_save_screen_adjust_letter(state, step);
            break;
        case GP100_ENCODER_QUICK_2:
            changed = gp100_save_screen_move_cursor(state, step);
            break;
        case GP100_ENCODER_QUICK_3:
            changed = gp100_save_screen_delete(state, step);
            break;
        case GP100_ENCODER_PARA:
            changed = gp100_save_screen_select_destination(state, step);
            break;
        default:
            return GP100_SAVE_EVENT_INVALID;
        }
        return changed ? GP100_SAVE_EVENT_STATE_CHANGED
                       : GP100_SAVE_EVENT_INVALID;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON) {
        return GP100_SAVE_EVENT_UNHANDLED;
    }
    if ((decoded.control == GP100_BUTTON_PARA ||
         decoded.control == GP100_BUTTON_SAVE) &&
        decoded.action == GP100_BUTTON_RELEASE) {
        *next_mode = GP100_UI_MODE_PLAY;
        return GP100_SAVE_EVENT_COMMIT_REQUEST;
    }
    if (decoded.control == GP100_BUTTON_DRUM) {
        if (decoded.action == GP100_BUTTON_RELEASE) {
            *drum_enabled = !*drum_enabled;
            return GP100_SAVE_EVENT_DRUM_TOGGLED;
        }
        if (decoded.action == GP100_BUTTON_HOLD) {
            *next_mode = GP100_UI_MODE_DRUM;
            return GP100_SAVE_EVENT_MODE_CHANGED;
        }
        return GP100_SAVE_EVENT_UNHANDLED;
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_SAVE_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_FOOTSWITCH_MINUS ||
        decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS) {
        *next_mode = GP100_UI_MODE_PLAY;
    } else if (decoded.control == GP100_BUTTON_EXIT) {
        *next_mode = state->return_mode >= GP100_UI_MODE_PLAY
            ? state->return_mode : GP100_UI_MODE_PLAY;
    } else if (decoded.control == GP100_BUTTON_GLOBAL) {
        *next_mode = GP100_UI_MODE_GLOBAL;
    } else if (decoded.control == GP100_BUTTON_EDIT) {
        *next_mode = GP100_UI_MODE_EDIT;
    } else {
        return GP100_SAVE_EVENT_UNHANDLED;
    }
    return GP100_SAVE_EVENT_MODE_CHANGED;
}

bool gp100_save_screen_default_theme(
    gp100_save_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->large_font = large_font != NULL ? large_font : regular_font;
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->cursor_rgb565 = GP100_UI_ACCENT;
    theme->delete_rgb565 = GP100_UI_DANGER;
    return true;
}

static bool draw_text(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t x,
                      int32_t y,
                      const char *text,
                      gp100_text_alignment_t alignment,
                      uint16_t foreground,
                      uint16_t background)
{
    gp100_text_style_t style = {
        foreground, background, alignment, true
    };
    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

static void patch_code(uint16_t destination, char code[4])
{
    unsigned number = (unsigned)destination + 1u;
    code[0] = 'P';
    code[1] = (char)('0' + number / 10u);
    code[2] = (char)('0' + number % 10u);
    code[3] = '\0';
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_save_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_save_screen_state_t *state,
    const char *destination_name,
    const gp100_save_screen_theme_t *theme)
{
    static const int32_t panel_left[3] = {3, 57, 111};
    static const int32_t panel_center[3] = {26, 80, 134};
    static const char *const panel_label[3] = {"Letter", "Cursor", "Delete"};
    char code[4];
    char prefix[GP100_SAVE_NAME_CHARACTERS + 1u];
    int32_t cursor_x;
    size_t index;
    uint16_t name_width;
    uint16_t prefix_width;
    uint16_t ignored_height;

    if (canvas == NULL || !valid_state(state) || destination_name == NULL ||
        theme == NULL || canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        theme->large_font == NULL || theme->regular_font == NULL ||
        theme->small_font == NULL) {
        return false;
    }
    patch_code(state->destination, code);
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    if (!draw_text(canvas, theme->large_font, 4, 3, "SAVE",
                   GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                   theme->background_rgb565) ||
        !draw_text(canvas, theme->large_font, 80, 24, code,
                   GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                   theme->background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 80, 43, destination_name,
                   GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                   theme->background_rgb565) ||
        !draw_text(canvas, theme->regular_font, 80, 66, state->name,
                   GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }
    (void)gp100_display_fill_triangle(
        canvas, 29, 34, 36, 29, 36, 39, theme->foreground_rgb565);
    (void)gp100_display_fill_triangle(
        canvas, 131, 34, 124, 29, 124, 39, theme->foreground_rgb565);
    memcpy(prefix, state->name, state->cursor);
    prefix[state->cursor] = '\0';
    if (!gp100_display_measure_text(
            theme->regular_font, state->name, &name_width, &ignored_height) ||
        !gp100_display_measure_text(
            theme->regular_font, prefix, &prefix_width, &ignored_height)) {
        return false;
    }
    cursor_x = 80 - (int32_t)name_width / 2 + (int32_t)prefix_width;
    (void)gp100_display_fill_rect(
        canvas, cursor_x, 62, 5u, 2u, theme->cursor_rgb565);
    (void)gp100_display_fill_triangle(
        canvas, cursor_x + 2, 83, cursor_x - 2, 89,
        cursor_x + 6, 89, theme->cursor_rgb565);
    for (index = 0u; index < 3u; ++index) {
        uint16_t label_color = index == 1u ? theme->cursor_rgb565
                               : index == 2u ? theme->delete_rgb565
                               : theme->foreground_rgb565;
        (void)gp100_display_fill_rect(
            canvas, panel_left[index], 94, 49u, 34u, theme->panel_rgb565);
        if (!draw_text(canvas, theme->regular_font, panel_center[index], 106,
                       panel_label[index], GP100_TEXT_ALIGN_CENTER,
                       label_color, theme->panel_rgb565)) {
            return false;
        }
    }
    return true;
}
