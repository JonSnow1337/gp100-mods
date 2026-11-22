#ifndef GP100_SAVE_SCREEN_H
#define GP100_SAVE_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/preset_record.h"
#include "ui/ui_geometry.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SAVE_NAME_CHARACTERS 11u
#define GP100_SAVE_ALPHABET_SIZE 92u

typedef struct gp100_save_screen_state {
    char name[GP100_SAVE_NAME_CHARACTERS + 1u];
    uint8_t cursor;
    uint8_t letter_index;
    uint16_t destination;
    gp100_ui_mode_t return_mode;
} gp100_save_screen_state_t;

typedef enum gp100_save_event_result {
    GP100_SAVE_EVENT_INVALID = -1,
    GP100_SAVE_EVENT_UNHANDLED = 0,
    GP100_SAVE_EVENT_STATE_CHANGED,
    GP100_SAVE_EVENT_COMMIT_REQUEST,
    GP100_SAVE_EVENT_DRUM_TOGGLED,
    GP100_SAVE_EVENT_MODE_CHANGED
} gp100_save_event_result_t;

typedef struct gp100_save_screen_theme {
    const gp100_bitmap_font_t *large_font;
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t panel_rgb565;
    uint16_t cursor_rgb565;
    uint16_t delete_rgb565;
} gp100_save_screen_theme_t;

extern const char gp100_save_alphabet[GP100_SAVE_ALPHABET_SIZE + 1u];

/* Factory destinations are always user presets P01..P99. */
bool gp100_save_screen_init(
    gp100_save_screen_state_t *state,
    uint16_t current_logical_preset,
    const gp100_preset_record_t *current_preset);

/* Positive steps rotate clockwise; negative steps rotate counter-clockwise. */
bool gp100_save_screen_adjust_letter(
    gp100_save_screen_state_t *state,
    int step);
bool gp100_save_screen_move_cursor(
    gp100_save_screen_state_t *state,
    int step);
bool gp100_save_screen_delete(
    gp100_save_screen_state_t *state,
    int direction);
bool gp100_save_screen_select_destination(
    gp100_save_screen_state_t *state,
    int step);

/* Build the record and flash address for a board-specific erase/write layer. */
bool gp100_save_screen_prepare_record(
    const gp100_save_screen_state_t *state,
    const gp100_preset_record_t *current_preset,
    gp100_preset_record_t *saved_preset,
    uint32_t *flash_address);

/* Apply the Save screen's editor and navigation actions from a packed event. */
gp100_save_event_result_t gp100_save_screen_handle_event(
    gp100_save_screen_state_t *state,
    bool *drum_enabled,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode);

bool gp100_save_screen_default_theme(
    gp100_save_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

/* destination_name is the existing name in the selected P01..P99 slot. */
bool gp100_save_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_save_screen_state_t *state,
    const char *destination_name,
    const gp100_save_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
