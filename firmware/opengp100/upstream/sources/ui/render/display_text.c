#include "display_text.h"

#include <limits.h>

static bool font_is_valid(const gp100_bitmap_font_t *font)
{
    return font != NULL && font->line_height != 0u &&
           (font->glyph_count == 0u || font->glyphs != NULL);
}

static bool decode_utf8(const char **cursor, uint32_t *codepoint)
{
    const uint8_t *input;
    uint32_t value;
    uint8_t first;
    uint8_t count;
    uint8_t index;

    if (cursor == NULL || *cursor == NULL || codepoint == NULL) {
        return false;
    }
    input = (const uint8_t *)*cursor;
    first = input[0];
    if (first == 0u) {
        return false;
    }
    if (first < 0x80u) {
        *codepoint = first;
        *cursor += 1;
        return true;
    }
    if ((first & 0xe0u) == 0xc0u) {
        value = first & 0x1fu;
        count = 2u;
        if (value < 2u) {
            return false;
        }
    } else if ((first & 0xf0u) == 0xe0u) {
        value = first & 0x0fu;
        count = 3u;
    } else if ((first & 0xf8u) == 0xf0u && first <= 0xf4u) {
        value = first & 0x07u;
        count = 4u;
    } else {
        return false;
    }

    for (index = 1u; index < count; ++index) {
        if ((input[index] & 0xc0u) != 0x80u) {
            return false;
        }
        value = (value << 6) | (input[index] & 0x3fu);
    }
    if ((count == 3u && value < 0x800u) ||
        (count == 4u && value < 0x10000u) ||
        (value >= 0xd800u && value <= 0xdfffu) || value > 0x10ffffu) {
        return false;
    }
    *codepoint = value;
    *cursor += count;
    return true;
}

const gp100_bitmap_glyph_t *gp100_bitmap_font_find(
    const gp100_bitmap_font_t *font,
    uint32_t codepoint)
{
    size_t low = 0u;
    size_t high;

    if (!font_is_valid(font)) {
        return NULL;
    }
    high = font->glyph_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        uint32_t candidate = font->glyphs[middle].codepoint;

        if (candidate < codepoint) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }
    if (low < font->glyph_count && font->glyphs[low].codepoint == codepoint) {
        return &font->glyphs[low];
    }
    return NULL;
}

static bool measure_line(const gp100_bitmap_font_t *font,
                         const char *begin,
                         const char *end,
                         uint16_t *width)
{
    const char *cursor = begin;
    uint32_t total = 0u;

    while (cursor < end) {
        uint32_t codepoint;
        const gp100_bitmap_glyph_t *glyph;

        if (!decode_utf8(&cursor, &codepoint) || cursor > end) {
            return false;
        }
        glyph = gp100_bitmap_font_find(font, codepoint);
        total += glyph != NULL ? glyph->advance : font->default_advance;
        if (total > UINT16_MAX) {
            return false;
        }
    }
    *width = (uint16_t)total;
    return true;
}

bool gp100_display_measure_text(const gp100_bitmap_font_t *font,
                                     const char *utf8,
                                     uint16_t *width,
                                     uint16_t *height)
{
    const char *line;
    const char *cursor;
    uint16_t widest = 0u;
    uint32_t lines = 1u;

    if (!font_is_valid(font) || utf8 == NULL || width == NULL ||
        height == NULL) {
        return false;
    }
    line = utf8;
    cursor = utf8;
    for (;;) {
        if (*cursor == '\n' || *cursor == '\r' || *cursor == '\0') {
            uint16_t line_width;

            if (!measure_line(font, line, cursor, &line_width)) {
                return false;
            }
            if (line_width > widest) {
                widest = line_width;
            }
            if (*cursor == '\0') {
                break;
            }
            if (*cursor == '\r' && cursor[1] == '\n') {
                ++cursor;
            }
            ++cursor;
            line = cursor;
            ++lines;
            continue;
        }
        ++cursor;
    }
    if (lines > UINT16_MAX / font->line_height) {
        return false;
    }
    *width = widest;
    *height = (uint16_t)(lines * font->line_height);
    return true;
}

static int32_t aligned_x(int32_t anchor,
                         uint16_t width,
                         gp100_text_alignment_t alignment)
{
    if (alignment == GP100_TEXT_ALIGN_RIGHT) {
        return anchor - width;
    }
    if (alignment == GP100_TEXT_ALIGN_CENTER) {
        return anchor - (int32_t)(width / 2u);
    }
    return anchor;
}

static bool draw_line(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t anchor_x,
                      int32_t y,
                      const char *begin,
                      const char *end,
                      const gp100_text_style_t *style)
{
    const char *cursor = begin;
    uint16_t width;
    uint16_t palette[2];
    int32_t pen_x;

    if (!measure_line(font, begin, end, &width)) {
        return false;
    }
    pen_x = aligned_x(anchor_x, width, style->alignment);
    palette[0] = style->background_rgb565;
    palette[1] = style->foreground_rgb565;

    while (cursor < end) {
        uint32_t codepoint;
        const gp100_bitmap_glyph_t *glyph;

        if (!decode_utf8(&cursor, &codepoint) || cursor > end) {
            return false;
        }
        glyph = gp100_bitmap_font_find(font, codepoint);
        if (glyph != NULL) {
            if (glyph->bitmap != NULL && glyph->width != 0u &&
                glyph->height != 0u && glyph->row_bytes != 0u) {
                (void)gp100_display_blit_indexed(
                    canvas, pen_x + glyph->x_offset, y + glyph->y_offset,
                    glyph->width, glyph->height, glyph->bitmap,
                    glyph->row_bytes, 0u, 1u, palette, 2u,
                    style->transparent_background);
            }
            pen_x += glyph->advance;
        } else {
            pen_x += font->default_advance;
        }
    }
    return true;
}

bool gp100_display_draw_text(gp100_display_canvas_t *canvas,
                                  const gp100_bitmap_font_t *font,
                                  int32_t x,
                                  int32_t y,
                                  const char *utf8,
                                  const gp100_text_style_t *style)
{
    const char *line;
    const char *cursor;
    int32_t line_y = y;

    if (canvas == NULL || canvas->pixels == NULL || !font_is_valid(font) ||
        utf8 == NULL || style == NULL ||
        style->alignment > GP100_TEXT_ALIGN_CENTER) {
        return false;
    }
    line = utf8;
    cursor = utf8;
    for (;;) {
        if (*cursor == '\n' || *cursor == '\r' || *cursor == '\0') {
            if (!draw_line(canvas, font, x, line_y, line, cursor, style)) {
                return false;
            }
            if (*cursor == '\0') {
                return true;
            }
            if (*cursor == '\r' && cursor[1] == '\n') {
                ++cursor;
            }
            ++cursor;
            line = cursor;
            line_y += font->line_height;
            continue;
        }
        ++cursor;
    }
}
