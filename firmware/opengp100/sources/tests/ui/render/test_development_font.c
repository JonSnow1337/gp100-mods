#include "assets/ui/development_font.h"

#include "ui/render/display_canvas.h"

#include <string.h>

int main(void)
{
    uint16_t pixels[32u * 8u];
    gp100_display_canvas_t canvas;
    gp100_text_style_t style = {
        0xffffu, 0u, GP100_TEXT_ALIGN_LEFT, false
    };
    const gp100_bitmap_font_t *font =
        gp100_development_font_5x7();
    const gp100_bitmap_font_t *large =
        gp100_development_font_10x14();
    const gp100_bitmap_glyph_t *uppercase;
    const gp100_bitmap_glyph_t *lowercase;
    const gp100_bitmap_glyph_t *sharp;
    uint16_t width;
    uint16_t height;
    size_t index;
    bool have_foreground = false;

    if (font == NULL || font->glyph_count != 95u ||
        font != gp100_development_font_5x7()) {
        return 1;
    }
    uppercase = gp100_bitmap_font_find(font, 'A');
    lowercase = gp100_bitmap_font_find(font, 'a');
    sharp = gp100_bitmap_font_find(font, '#');
    if (uppercase == NULL || lowercase == NULL || sharp == NULL ||
        uppercase->bitmap == NULL || lowercase->bitmap == NULL ||
        memcmp(uppercase->bitmap, lowercase->bitmap, 7u) == 0 ||
        sharp->bitmap == NULL ||
        gp100_bitmap_font_find(font, 0x7fu) != NULL) {
        return 2;
    }
    memset(pixels, 0, sizeof(pixels));
    if (!gp100_display_canvas_init(
            &canvas, pixels, 32u, 8u, 32u) ||
        !gp100_display_draw_text(
            &canvas, font, 0, 0, "A1-z", &style)) {
        return 3;
    }
    for (index = 0u; index < sizeof(pixels) / sizeof(pixels[0]); ++index) {
        if (pixels[index] == 0xffffu) {
            have_foreground = true;
            break;
        }
    }
    if (!have_foreground) {
        return 4;
    }
    if (large != gp100_development_font_10x14() ||
        !gp100_display_measure_text(large, "P01", &width, &height) ||
        width != 36u || height != 16u) {
        return 5;
    }
    return 0;
}
