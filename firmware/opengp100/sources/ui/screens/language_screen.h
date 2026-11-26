#ifndef GP100_LANGUAGE_SCREEN_H
#define GP100_LANGUAGE_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/settings_record.h"
#include "ui/ui_geometry.h"
#include "protocol/midi/sysex/vendor_sysex_system.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_LANGUAGE_COUNT 9u
#define GP100_LANGUAGE_VISIBLE_ROWS 5u

typedef struct gp100_language_screen_state {
    gp100_language_catalog_t catalog;
    size_t selected_index;
    size_t current_index;
    bool committed;
} gp100_language_screen_state_t;

typedef struct gp100_language_screen_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t panel_rgb565;
} gp100_language_screen_theme_t;

/* Return the exact V2.1 display name for a runtime language identifier. */
const char *gp100_language_name(uint8_t identifier);

bool gp100_language_screen_init(
    gp100_language_screen_state_t *state,
    const gp100_language_catalog_t *catalog,
    uint8_t current_identifier);

bool gp100_language_screen_move(
    gp100_language_screen_state_t *state,
    int step);

/* Commit the selected identifier to the settings-record payload at 0x12. */
bool gp100_language_screen_commit(
    gp100_language_screen_state_t *state,
    gp100_settings_record_t *settings,
    uint8_t *selected_identifier);

size_t gp100_language_screen_first_visible(
    const gp100_language_screen_state_t *state);

bool gp100_language_screen_default_theme(
    gp100_language_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_language_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_language_screen_state_t *state,
    const gp100_language_screen_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
