#include "ui/render/display_canvas.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_pixels_and_rectangles(void)
{
    uint16_t pixels[6u * 5u];
    gp100_display_canvas_t canvas;
    size_t index;

    if (gp100_display_bgr888_to_rgb565(0x0000ffu) != 0xf800u ||
        gp100_display_bgr888_to_rgb565(0xff0000u) != 0x001fu) {
        return 4;
    }
    memset(pixels, 0, sizeof(pixels));
    if (!gp100_display_canvas_init(&canvas, pixels, 5u, 5u, 6u) ||
        !gp100_display_put_pixel(&canvas, 4, 4, UINT16_C(0x1234)) ||
        gp100_display_put_pixel(&canvas, 5, 4, 1u) ||
        !gp100_display_fill_rect(&canvas, -2, 1, 4u, 3u,
                                      UINT16_C(0xabcd))) {
        return 1;
    }
    for (index = 0u; index < sizeof(pixels) / sizeof(pixels[0]); ++index) {
        size_t x = index % 6u;
        size_t y = index / 6u;
        uint16_t expected =
            x < 2u && y >= 1u && y < 4u ? UINT16_C(0xabcd) : 0u;
        if (x == 4u && y == 4u) {
            expected = UINT16_C(0x1234);
        }
        if (pixels[index] != expected) {
            return 2;
        }
    }
    return gp100_display_fill_rect(&canvas, 9, 9, 2u, 2u, 1u)
               ? 3
               : 0;
}

static int test_indexed_replace_and_clip(void)
{
    static const uint8_t source[] = {
        0x1bu, /* 00 01 10 11 */
        0xe4u  /* 11 10 01 00 */
    };
    static const uint16_t palette[] = {
        UINT16_C(0x0000), UINT16_C(0x1111),
        UINT16_C(0x2222), UINT16_C(0x3333)
    };
    uint16_t pixels[3u * 2u] = {0};
    gp100_display_canvas_t canvas;
    static const uint16_t expected[] = {
        UINT16_C(0x2222), UINT16_C(0x3333), 0u,
        UINT16_C(0x1111), UINT16_C(0x0000), 0u
    };

    if (!gp100_display_canvas_init(&canvas, pixels, 3u, 2u, 3u) ||
        !gp100_display_blit_indexed(
            &canvas, -2, 0, 4u, 2u, source, 1u, 0u, 2u,
            palette, 4u, false)) {
        return 1;
    }
    return memcmp(pixels, expected, sizeof(expected)) == 0 ? 0 : 2;
}

static int test_indexed_depths_and_transparency(void)
{
    static const uint8_t one_bit[] = {0xa0u};
    static const uint8_t four_bit[] = {0x05u, 0xa0u};
    static const uint8_t eight_bit[] = {0u, 7u, 0u};
    uint16_t palette16[16];
    uint16_t palette256[256];
    uint16_t pixels[8];
    gp100_display_canvas_t canvas;
    size_t index;

    for (index = 0u; index < 16u; ++index) {
        palette16[index] = (uint16_t)(0x1000u + index);
    }
    for (index = 0u; index < 256u; ++index) {
        palette256[index] = (uint16_t)(0x2000u + index);
    }
    for (index = 0u; index < 8u; ++index) {
        pixels[index] = UINT16_C(0xffff);
    }
    if (!gp100_display_canvas_init(&canvas, pixels, 8u, 1u, 8u) ||
        !gp100_display_blit_indexed(
            &canvas, 0, 0, 4u, 1u, one_bit, 1u, 0u, 1u,
            NULL, 0u, true) ||
        !gp100_display_blit_indexed(
            &canvas, 4, 0, 2u, 1u, four_bit, 2u, 1u, 4u,
            palette16, 16u, true) ||
        !gp100_display_blit_indexed(
            &canvas, 5, 0, 3u, 1u, eight_bit, 3u, 0u, 8u,
            palette256, 256u, true)) {
        return 1;
    }
    if (pixels[0] != 1u || pixels[1] != UINT16_C(0xffff) ||
        pixels[2] != 1u || pixels[3] != UINT16_C(0xffff) ||
        pixels[4] != UINT16_C(0x1005) ||
        pixels[5] != UINT16_C(0x100a) ||
        pixels[6] != UINT16_C(0x2007) ||
        pixels[7] != UINT16_C(0xffff)) {
        return 2;
    }
    return 0;
}

static int test_invalid_arguments(void)
{
    uint16_t pixel;
    uint8_t source = 0u;
    uint16_t palette[2] = {0u, 1u};
    gp100_display_canvas_t canvas;

    if (gp100_display_canvas_init(NULL, &pixel, 1u, 1u, 1u) ||
        !gp100_display_canvas_init(&canvas, &pixel, 1u, 1u, 1u) ||
        gp100_display_blit_indexed(
            &canvas, 0, 0, 1u, 1u, &source, 1u, 0u, 3u,
            NULL, 0u, false) ||
        gp100_display_blit_indexed(
            &canvas, 0, 0, 1u, 1u, &source, 1u, 0u, 2u,
            palette, 2u, false) ||
        gp100_display_blit_indexed(
            &canvas, 0, 0, 5u, 1u, &source, 1u, 0u, 2u,
            NULL, 0u, false)) {
        return 1;
    }
    return 0;
}

static int test_rotated_storage(void)
{
    uint16_t pixels[6] = {0};
    gp100_display_canvas_t canvas;

    if (!gp100_display_canvas_init_rotated(
            &canvas, pixels, 2u, 3u, 2u,
            GP100_DISPLAY_ROTATION_CLOCKWISE) ||
        canvas.width != 3u || canvas.height != 2u ||
        !gp100_display_put_pixel(&canvas, 0, 0, 0x1111u) ||
        !gp100_display_put_pixel(&canvas, 2, 1, 0x2222u) ||
        pixels[4] != 0x1111u || pixels[1] != 0x2222u) {
        return 1;
    }

    memset(pixels, 0, sizeof(pixels));
    if (!gp100_display_canvas_init_rotated(
            &canvas, pixels, 2u, 3u, 2u,
            GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE) ||
        !gp100_display_fill_rect(&canvas, 0, 0, 3u, 1u, 0x3333u) ||
        pixels[1] != 0x3333u || pixels[3] != 0x3333u ||
        pixels[5] != 0x3333u || pixels[0] != 0u) {
        return 2;
    }
    if (gp100_display_canvas_init_rotated(
            &canvas, pixels, 2u, 3u, 2u,
            GP100_DISPLAY_ROTATION_NONE)) {
        return 3;
    }
    return 0;
}

static int test_lines_and_triangles(void)
{
    uint16_t pixels[8u * 8u] = {0};
    gp100_display_canvas_t canvas;

    if (!gp100_display_canvas_init(&canvas, pixels, 8u, 8u, 8u) ||
        !gp100_display_draw_line(&canvas, -2, 2, 4, 2, 0x1111u) ||
        !gp100_display_draw_line(&canvas, 7, 0, 1, 7, 0x3333u) ||
        !gp100_display_fill_triangle(
            &canvas, 2, 4, 6, 4, 4, 7, 0x2222u)) {
        return 1;
    }
    if (pixels[2u * 8u] != 0x1111u ||
        pixels[2u * 8u + 4u] != 0x1111u ||
        pixels[4u * 8u + 2u] != 0x2222u ||
        pixels[5u * 8u + 4u] != 0x2222u ||
        pixels[7u * 8u + 4u] != 0x2222u ||
        pixels[7u * 8u + 1u] != 0x3333u ||
        pixels[7u * 8u + 2u] != 0u) {
        return 2;
    }
    return gp100_display_fill_triangle(
               &canvas, -70000, 0, 0, 0, 0, 1, 0x3333u)
               ? 3
               : 0;
}

int main(void)
{
    int result = test_pixels_and_rectangles();
    if (result != 0) {
        fprintf(stderr, "pixel/rectangle test failed: %d\n", result);
        return 10 + result;
    }
    result = test_indexed_replace_and_clip();
    if (result != 0) {
        fprintf(stderr, "indexed replacement test failed: %d\n", result);
        return 20 + result;
    }
    result = test_indexed_depths_and_transparency();
    if (result != 0) {
        fprintf(stderr, "indexed depth test failed: %d\n", result);
        return 30 + result;
    }
    result = test_invalid_arguments();
    if (result != 0) {
        fprintf(stderr, "invalid argument test failed: %d\n", result);
        return 40 + result;
    }
    result = test_rotated_storage();
    if (result != 0) {
        fprintf(stderr, "rotated storage test failed: %d\n", result);
        return 50 + result;
    }
    result = test_lines_and_triangles();
    if (result != 0) {
        fprintf(stderr, "line/triangle test failed: %d\n", result);
        return 60 + result;
    }
    return 0;
}
