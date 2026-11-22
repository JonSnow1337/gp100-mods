#ifndef GP100_DISPLAY_TEXT_H
#define GP100_DISPLAY_TEXT_H

#include "display_canvas.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_bitmap_glyph {
    uint32_t codepoint;
    const uint8_t *bitmap;
    size_t row_bytes;
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    int8_t x_offset;
    int8_t y_offset;
} gp100_bitmap_glyph_t;

typedef struct gp100_bitmap_font {
    const gp100_bitmap_glyph_t *glyphs;
    size_t glyph_count;
    uint8_t line_height;
    uint8_t default_advance;
} gp100_bitmap_font_t;

typedef enum gp100_text_alignment {
    GP100_TEXT_ALIGN_LEFT = 0,
    GP100_TEXT_ALIGN_RIGHT = 1,
    GP100_TEXT_ALIGN_CENTER = 2
} gp100_text_alignment_t;

typedef struct gp100_text_style {
    uint16_t foreground_rgb565;
    uint16_t background_rgb565;
    gp100_text_alignment_t alignment;
    bool transparent_background;
} gp100_text_style_t;

/* Glyphs must be sorted by codepoint. Missing glyphs use default_advance. */
const gp100_bitmap_glyph_t *gp100_bitmap_font_find(
    const gp100_bitmap_font_t *font,
    uint32_t codepoint);

/* Measure the widest UTF-8 line. Newline starts another line. */
bool gp100_display_measure_text(const gp100_bitmap_font_t *font,
                                     const char *utf8,
                                     uint16_t *width,
                                     uint16_t *height);

/*
 * Draw UTF-8 text at an alignment anchor. For left alignment x is the first
 * pixel; for right/center it is the right edge or center of each line.
 */
bool gp100_display_draw_text(gp100_display_canvas_t *canvas,
                                  const gp100_bitmap_font_t *font,
                                  int32_t x,
                                  int32_t y,
                                  const char *utf8,
                                  const gp100_text_style_t *style);

#ifdef __cplusplus
}
#endif

#endif
