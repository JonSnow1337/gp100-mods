#include "ui/render/display_text.h"

#include <stdio.h>
#include <string.h>

static const uint8_t glyph_a[] = {0x40u, 0xa0u, 0xe0u, 0xa0u, 0xa0u};
static const uint8_t glyph_b[] = {0xc0u, 0xa0u, 0xc0u, 0xa0u, 0xc0u};

static const gp100_bitmap_glyph_t glyphs[] = {
    {' ', NULL, 0u, 0u, 0u, 2u, 0, 0},
    {'A', glyph_a, 1u, 3u, 5u, 4u, 0, 0},
    {'B', glyph_b, 1u, 3u, 5u, 4u, 0, 0},
};

static const gp100_bitmap_font_t font = {
    glyphs,
    sizeof(glyphs) / sizeof(glyphs[0]),
    6u,
    3u,
};

static int test_measure(void)
{
    uint16_t width;
    uint16_t height;

    if (!gp100_display_measure_text(&font, "AB\nA", &width, &height) ||
        width != 8u || height != 12u) {
        return 1;
    }
    if (gp100_bitmap_font_find(&font, 'A') != &glyphs[1] ||
        gp100_bitmap_font_find(&font, 'Z') != NULL) {
        return 2;
    }
    if (gp100_display_measure_text(&font, "\xc0\x80", &width,
                                         &height) ||
        gp100_display_measure_text(NULL, "A", &width, &height)) {
        return 3;
    }
    return 0;
}

static int test_draw(void)
{
    uint16_t pixels[12u * 12u];
    gp100_display_canvas_t canvas;
    gp100_text_style_t style = {
        0xffffu,
        0x0000u,
        GP100_TEXT_ALIGN_LEFT,
        true,
    };

    memset(pixels, 0, sizeof(pixels));
    if (!gp100_display_canvas_init(&canvas, pixels, 12u, 12u, 12u) ||
        !gp100_display_draw_text(&canvas, &font, 1, 1, "A B", &style)) {
        return 1;
    }
    if (pixels[1u * 12u + 2u] != 0xffffu ||
        pixels[1u * 12u + 1u] != 0x0000u ||
        pixels[3u * 12u + 1u] != 0xffffu ||
        pixels[1u * 12u + 8u] != 0xffffu) {
        return 2;
    }

    memset(pixels, 0, sizeof(pixels));
    style.alignment = GP100_TEXT_ALIGN_CENTER;
    if (!gp100_display_draw_text(&canvas, &font, 6, 0, "AB", &style) ||
        pixels[0u * 12u + 3u] != 0xffffu ||
        pixels[0u * 12u + 7u] != 0xffffu) {
        return 3;
    }
    style.alignment = (gp100_text_alignment_t)9;
    if (gp100_display_draw_text(&canvas, &font, 0, 0, "A", &style)) {
        return 4;
    }
    return 0;
}

int main(void)
{
    int result = test_measure();
    if (result != 0) {
        fprintf(stderr, "Display text measure test failed: %d\n", result);
        return 10 + result;
    }
    result = test_draw();
    if (result != 0) {
        fprintf(stderr, "Display text draw test failed: %d\n", result);
        return 20 + result;
    }
    return 0;
}
