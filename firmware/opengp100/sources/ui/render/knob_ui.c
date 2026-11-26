#include "knob_ui.h"

#include "ui/ui_geometry.h"

#include <string.h>

enum { kBoxWidth = 50, kInnerWidth = 44 };
static const int32_t box_left[GP100_KNOB_COLUMNS] = {1, 55, 109};

int32_t gp100_knob_box_left(size_t column)
{
    return column < GP100_KNOB_COLUMNS ? box_left[column] : box_left[0];
}

int32_t gp100_knob_box_center(size_t column)
{
    return gp100_knob_box_left(column) + kBoxWidth / 2;
}

static void circle(gp100_display_canvas_t *canvas, int32_t x, int32_t y, bool fill, uint16_t color)
{
    static const char *const shape[5] = {".###.", "#####", "#####", "#####", ".###."};
    for (int32_t row = 0; row < 5; ++row) {
        for (int32_t col = 0; col < 5; ++col) {
            const bool inside = shape[row][col] == '#';
            const bool edge = inside &&
                (row == 0 || row == 4 || col == 0 || col == 4 ||
                 shape[row][col - 1] == '.' || shape[row][col + 1] == '.' ||
                 shape[row - 1][col] == '.' || shape[row + 1][col] == '.');
            if (inside && (fill || edge)) {
                (void)gp100_display_put_pixel(canvas, x + col, y + row, color);
            }
        }
    }
}

bool gp100_knob_glyph_draw(gp100_display_canvas_t *canvas, int32_t center_x, int32_t y,
                           size_t column, uint16_t lit, uint16_t unlit)
{
    if (canvas == NULL || column >= GP100_KNOB_COLUMNS) {
        return false;
    }
    for (size_t i = 0u; i < GP100_KNOB_COLUMNS; ++i) {
        circle(canvas, center_x - 11 + (int32_t)i * 8, y, i == column, i == column ? lit : unlit);
    }
    return true;
}

static bool draw_fitted(gp100_display_canvas_t *canvas, const gp100_bitmap_font_t *font,
                        int32_t center_x, int32_t y, const char *text, uint16_t max_width,
                        uint16_t color, uint16_t background)
{
    char fitted[24];
    uint16_t width;
    uint16_t height;
    size_t length = strlen(text);
    const gp100_text_style_t style = {color, background, GP100_TEXT_ALIGN_CENTER, true};

    if (length >= sizeof(fitted)) {
        length = sizeof(fitted) - 1u;
    }
    memcpy(fitted, text, length);
    fitted[length] = '\0';
    while (length != 0u && gp100_display_measure_text(font, fitted, &width, &height) &&
           width > max_width) {
        fitted[--length] = '\0';
    }
    return gp100_display_draw_text(canvas, font, center_x, y, fitted, &style);
}

bool gp100_knob_box_draw(gp100_display_canvas_t *canvas, const gp100_knob_style_t *style,
                         size_t column, const char *label, const char *value, float bar_ratio)
{
    if (canvas == NULL || style == NULL || style->font == NULL || column >= GP100_KNOB_COLUMNS ||
        label == NULL || value == NULL) {
        return false;
    }
    const int32_t left = gp100_knob_box_left(column);
    const int32_t center = gp100_knob_box_center(column);

    (void)gp100_display_fill_rect(canvas, left, GP100_KNOB_BOX_Y, kBoxWidth, GP100_KNOB_BOX_HEIGHT,
                                  style->box_rgb565);
    (void)gp100_knob_glyph_draw(canvas, center, GP100_KNOB_BOX_Y + 2, column,
                                style->accent_rgb565, style->track_rgb565);
    if (!draw_fitted(canvas, style->font, center, GP100_KNOB_BOX_Y + 9, label, kInnerWidth,
                     style->label_rgb565, style->box_rgb565)) {
        return false;
    }
    if (bar_ratio >= 0.0f) {
        const float ratio = bar_ratio > 1.0f ? 1.0f : bar_ratio;
        const uint16_t filled = (uint16_t)(ratio * (float)kInnerWidth + 0.5f);
        (void)gp100_display_fill_rect(canvas, left + 3, GP100_KNOB_BOX_Y + 17, kInnerWidth, 3u,
                                      style->track_rgb565);
        if (filled != 0u) {
            (void)gp100_display_fill_rect(canvas, left + 3, GP100_KNOB_BOX_Y + 17, filled, 3u,
                                          style->accent_rgb565);
        }
    }
    return draw_fitted(canvas, style->font, center, GP100_KNOB_BOX_Y + 21, value, kInnerWidth,
                       style->value_rgb565, style->box_rgb565);
}

bool gp100_hint_draw(gp100_display_canvas_t *canvas, const gp100_bitmap_font_t *font,
                     int32_t x, int32_t y, uint16_t max_width, const char *markup,
                     uint16_t chip_rgb565, uint16_t chip_text_rgb565,
                     uint16_t text_rgb565, uint16_t background_rgb565)
{
    int32_t cursor = x;
    const int32_t limit = x + (int32_t)max_width;

    if (canvas == NULL || font == NULL || markup == NULL) {
        return false;
    }
    while (*markup != '\0') {
        char piece[32];
        size_t length = 0u;
        uint16_t width;
        uint16_t height;
        const bool chip = *markup == '[';

        if (chip) {
            ++markup;
            while (*markup != '\0' && *markup != ']' && length + 1u < sizeof(piece)) {
                piece[length++] = *markup++;
            }
            if (*markup == ']') {
                ++markup;
            }
        } else {
            while (*markup != '\0' && *markup != '[' && length + 1u < sizeof(piece)) {
                piece[length++] = *markup++;
            }
        }
        piece[length] = '\0';
        if (length == 0u || !gp100_display_measure_text(font, piece, &width, &height)) {
            continue;
        }
        const int32_t needed = (int32_t)width + (chip ? 5 : 0);
        if (cursor + needed > limit) {
            break;                                   /* the rest does not fit: leave it out */
        }
        if (chip) {
            const gp100_text_style_t style = {chip_text_rgb565, chip_rgb565, GP100_TEXT_ALIGN_LEFT, false};
            (void)gp100_display_fill_rect(canvas, cursor, y - 1, (uint16_t)(width + 5u),
                                          (uint16_t)(height + 2u), chip_rgb565);
            (void)gp100_display_draw_text(canvas, font, cursor + 2, y, piece, &style);
        } else {
            const gp100_text_style_t style = {text_rgb565, background_rgb565, GP100_TEXT_ALIGN_LEFT, true};
            (void)gp100_display_draw_text(canvas, font, cursor, y, piece, &style);
        }
        cursor += needed;
    }
    return true;
}
