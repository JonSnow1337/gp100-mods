#ifndef GP100_KNOB_UI_H
#define GP100_KNOB_UI_H

#include "ui/render/display_text.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The three Quick knobs sit under the screen. Whatever a knob does is drawn in the column above it:
 * column 0 = Quick 1 (left), 1 = Quick 2 (middle), 2 = Quick 3 (right). Buttons never use these columns.
 */
#define GP100_KNOB_COLUMNS 3u
#define GP100_KNOB_BOX_Y 89
#define GP100_KNOB_BOX_HEIGHT 30u

typedef struct gp100_knob_style {
    const gp100_bitmap_font_t *font;
    uint16_t box_rgb565;
    uint16_t label_rgb565;
    uint16_t value_rgb565;
    uint16_t accent_rgb565;        /* lit knob circle and bar fill */
    uint16_t track_rgb565;         /* unlit circles and the empty bar */
} gp100_knob_style_t;

/* Left edge and width of a column's box, and its centre. */
int32_t gp100_knob_box_left(size_t column);
int32_t gp100_knob_box_center(size_t column);

/* Row of three small circles; the one for `column` is filled. 19 x 5 pixels. */
bool gp100_knob_glyph_draw(gp100_display_canvas_t *canvas, int32_t center_x, int32_t y,
                           size_t column, uint16_t lit, uint16_t unlit);

/* One knob column: box, glyph, label, optional bar (bar_ratio < 0 = none) and value. */
bool gp100_knob_box_draw(gp100_display_canvas_t *canvas, const gp100_knob_style_t *style,
                         size_t column, const char *label, const char *value, float bar_ratio);

/*
 * Button hints. "[PARA] keep  [EXIT] cancel": words in square brackets are drawn as button chips,
 * the rest as plain text. Whatever does not fit in the width is left out.
 */
bool gp100_hint_draw(gp100_display_canvas_t *canvas, const gp100_bitmap_font_t *font,
                     int32_t x, int32_t y, uint16_t max_width, const char *markup,
                     uint16_t chip_rgb565, uint16_t chip_text_rgb565,
                     uint16_t text_rgb565, uint16_t background_rgb565);

#ifdef __cplusplus
}
#endif

#endif
