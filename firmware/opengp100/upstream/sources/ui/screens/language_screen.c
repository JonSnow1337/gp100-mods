#include "language_screen.h"

#include "ui/ui_palette.h"

#include <string.h>

static const char *const language_names[GP100_LANGUAGE_COUNT] = {
    "English",
    "ç®ä½ä¸­æ",
    "ç¹é«ä¸­æ",
    "æ¥æ¬èª",
    "íêµ­ì´",
    "EspaÃ±ol",
    "Deutsch",
    "Italiano",
    "Ð ÑÑÑÐºÐ¸Ð¹"
};

const char *gp100_language_name(uint8_t identifier)
{
    if (identifier >= GP100_LANGUAGE_COUNT) {
        return NULL;
    }
    return language_names[identifier];
}

static bool catalog_is_valid(const gp100_language_catalog_t *catalog)
{
    size_t outer;
    size_t inner;

    if (catalog == NULL || catalog->count == 0u ||
        catalog->count > GP100_SYSEX_SYSTEM_MAX_LANGUAGES) {
        return false;
    }
    for (outer = 0u; outer < catalog->count; ++outer) {
        if (gp100_language_name(catalog->identifiers[outer]) == NULL) {
            return false;
        }
        for (inner = 0u; inner < outer; ++inner) {
            if (catalog->identifiers[inner] == catalog->identifiers[outer]) {
                return false;
            }
        }
    }
    return true;
}

bool gp100_language_screen_init(
    gp100_language_screen_state_t *state,
    const gp100_language_catalog_t *catalog,
    uint8_t current_identifier)
{
    size_t index;

    if (state == NULL || !catalog_is_valid(catalog)) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->catalog = *catalog;
    for (index = 0u; index < catalog->count; ++index) {
        if (catalog->identifiers[index] == current_identifier) {
            state->current_index = index;
            state->selected_index = index;
            return true;
        }
    }
    /* The factory identifier-to-index helper falls back to entry zero. */
    state->current_index = 0u;
    state->selected_index = 0u;
    return true;
}

bool gp100_language_screen_move(
    gp100_language_screen_state_t *state,
    int step)
{
    size_t candidate;

    if (state == NULL || !catalog_is_valid(&state->catalog) ||
        state->selected_index >= state->catalog.count) {
        return false;
    }
    candidate = state->selected_index;
    if (step > 0) {
        size_t amount = (size_t)step;
        if (amount >= state->catalog.count - candidate) {
            candidate = state->catalog.count - 1u;
        } else {
            candidate += amount;
        }
    } else if (step < 0) {
        size_t amount = (size_t)(-(int64_t)step);
        candidate = amount > candidate ? 0u : candidate - amount;
    }
    state->selected_index = candidate;
    state->committed = false;
    return true;
}

bool gp100_language_screen_commit(
    gp100_language_screen_state_t *state,
    gp100_settings_record_t *settings,
    uint8_t *selected_identifier)
{
    uint8_t identifier;

    if (state == NULL || settings == NULL ||
        !catalog_is_valid(&state->catalog) ||
        state->selected_index >= state->catalog.count) {
        return false;
    }
    identifier = state->catalog.identifiers[state->selected_index];
    if (!gp100_settings_set_i16(
            settings, GP100_SETTINGS_LANGUAGE_INDEX,
            (int16_t)identifier)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    state->current_index = state->selected_index;
    state->committed = true;
    if (selected_identifier != NULL) {
        *selected_identifier = identifier;
    }
    return true;
}

size_t gp100_language_screen_first_visible(
    const gp100_language_screen_state_t *state)
{
    size_t maximum;

    if (state == NULL || state->catalog.count == 0u ||
        state->selected_index >= state->catalog.count ||
        state->catalog.count <= GP100_LANGUAGE_VISIBLE_ROWS) {
        return 0u;
    }
    maximum = state->catalog.count - GP100_LANGUAGE_VISIBLE_ROWS;
    if (state->selected_index < GP100_LANGUAGE_VISIBLE_ROWS) {
        return 0u;
    }
    if (state->selected_index - (GP100_LANGUAGE_VISIBLE_ROWS - 1u) >
        maximum) {
        return maximum;
    }
    return state->selected_index - (GP100_LANGUAGE_VISIBLE_ROWS - 1u);
}

bool gp100_language_screen_default_theme(
    gp100_language_screen_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->panel_rgb565 = GP100_UI_PANEL;
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

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_language_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_language_screen_state_t *state,
    const gp100_language_screen_theme_t *theme)
{
    size_t first;
    size_t row;

    if (canvas == NULL || state == NULL || theme == NULL ||
        theme->regular_font == NULL || theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        !catalog_is_valid(&state->catalog) ||
        state->selected_index >= state->catalog.count ||
        state->current_index >= state->catalog.count) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    if (!draw_text(canvas, theme->regular_font, 5, 5, "< Language",
                   GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }
    first = gp100_language_screen_first_visible(state);
    for (row = 0u; row < GP100_LANGUAGE_VISIBLE_ROWS; ++row) {
        size_t index = first + row;
        int32_t y = (int32_t)(23u + row * 18u);
        bool selected;

        if (index >= state->catalog.count) {
            break;
        }
        selected = index == state->selected_index;
        if (selected) {
            (void)gp100_display_fill_rect(
                canvas, 2, y - 1, 156u, 17u, theme->selected_rgb565);
        }
        if (index == state->current_index &&
            !draw_text(canvas, theme->small_font, 7, y, "*",
                       GP100_TEXT_ALIGN_LEFT,
                       selected ? theme->background_rgb565
                                : theme->selected_rgb565,
                       selected ? theme->selected_rgb565
                                : theme->background_rgb565)) {
            return false;
        }
        if (!draw_text(
                canvas, theme->regular_font, 22, y,
                gp100_language_name(state->catalog.identifiers[index]),
                GP100_TEXT_ALIGN_LEFT,
                selected ? theme->background_rgb565
                         : theme->foreground_rgb565,
                selected ? theme->selected_rgb565
                         : theme->background_rgb565)) {
            return false;
        }
    }
    if (state->committed) {
        (void)gp100_display_fill_rect(
            canvas, 2, 112, 156u, 14u, theme->panel_rgb565);
        return draw_text(canvas, theme->small_font, 80, 114, "Finished",
                         GP100_TEXT_ALIGN_CENTER,
                         theme->foreground_rgb565, theme->panel_rgb565);
    }
    return true;
}
