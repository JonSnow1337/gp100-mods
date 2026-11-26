#include "display_canvas.h"

#include <limits.h>

static bool canvas_is_valid(const gp100_display_canvas_t *canvas)
{
    return canvas != NULL && canvas->pixels != NULL && canvas->width != 0u &&
           canvas->height != 0u && canvas->storage_width != 0u &&
           canvas->storage_height != 0u &&
           canvas->stride_pixels >= canvas->storage_width &&
           canvas->rotation <= GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE;
}

uint16_t gp100_display_bgr888_to_rgb565(uint32_t bgr888)
{
    uint16_t red = (uint16_t)((bgr888 >> 3) & 0x1fu);
    uint16_t green = (uint16_t)((bgr888 >> 10) & 0x3fu);
    uint16_t blue = (uint16_t)((bgr888 >> 19) & 0x1fu);

    return (uint16_t)((red << 11) | (green << 5) | blue);
}

bool gp100_display_canvas_init(gp100_display_canvas_t *canvas,
                                    uint16_t *pixels,
                                    uint16_t width,
                                    uint16_t height,
                                    size_t stride_pixels)
{
    if (canvas == NULL || pixels == NULL || width == 0u || height == 0u ||
        stride_pixels < width || stride_pixels > SIZE_MAX / height) {
        return false;
    }
    canvas->pixels = pixels;
    canvas->width = width;
    canvas->height = height;
    canvas->storage_width = width;
    canvas->storage_height = height;
    canvas->stride_pixels = stride_pixels;
    canvas->rotation = GP100_DISPLAY_ROTATION_NONE;
    return true;
}

bool gp100_display_canvas_init_rotated(
    gp100_display_canvas_t *canvas,
    uint16_t *pixels,
    uint16_t storage_width,
    uint16_t storage_height,
    size_t stride_pixels,
    gp100_display_rotation_t rotation)
{
    if (canvas == NULL || pixels == NULL || storage_width == 0u ||
        storage_height == 0u || stride_pixels < storage_width ||
        stride_pixels > SIZE_MAX / storage_height ||
        (rotation != GP100_DISPLAY_ROTATION_CLOCKWISE &&
         rotation != GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE)) {
        return false;
    }
    canvas->pixels = pixels;
    canvas->width = storage_height;
    canvas->height = storage_width;
    canvas->storage_width = storage_width;
    canvas->storage_height = storage_height;
    canvas->stride_pixels = stride_pixels;
    canvas->rotation = (uint8_t)rotation;
    return true;
}

static size_t pixel_index(const gp100_display_canvas_t *canvas,
                          size_t x,
                          size_t y)
{
    size_t storage_x;
    size_t storage_y;

    if (canvas->rotation == GP100_DISPLAY_ROTATION_CLOCKWISE) {
        storage_x = y;
        storage_y = (size_t)canvas->storage_height - 1u - x;
    } else if (canvas->rotation ==
               GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE) {
        storage_x = (size_t)canvas->storage_width - 1u - y;
        storage_y = x;
    } else {
        storage_x = x;
        storage_y = y;
    }
    return storage_y * canvas->stride_pixels + storage_x;
}

bool gp100_display_put_pixel(gp100_display_canvas_t *canvas,
                                  int32_t x,
                                  int32_t y,
                                  uint16_t rgb565)
{
    if (!canvas_is_valid(canvas) || x < 0 || y < 0 ||
        x >= canvas->width || y >= canvas->height) {
        return false;
    }
    canvas->pixels[pixel_index(canvas, (size_t)x, (size_t)y)] = rgb565;
    return true;
}

bool gp100_display_fill_rect(gp100_display_canvas_t *canvas,
                                  int32_t x,
                                  int32_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t rgb565)
{
    int64_t left = x;
    int64_t top = y;
    int64_t right = left + width;
    int64_t bottom = top + height;
    int64_t row;

    if (!canvas_is_valid(canvas) || width == 0u || height == 0u) {
        return false;
    }
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > canvas->width) {
        right = canvas->width;
    }
    if (bottom > canvas->height) {
        bottom = canvas->height;
    }
    if (left >= right || top >= bottom) {
        return false;
    }

    for (row = top; row < bottom; ++row) {
        int64_t column;
        for (column = left; column < right; ++column) {
            canvas->pixels[pixel_index(canvas, (size_t)column,
                                       (size_t)row)] = rgb565;
        }
    }
    return true;
}

static int32_t absolute_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

bool gp100_display_draw_line(gp100_display_canvas_t *canvas,
                                  int32_t x0,
                                  int32_t y0,
                                  int32_t x1,
                                  int32_t y1,
                                  uint16_t rgb565)
{
    int32_t dx;
    int32_t dy;
    int32_t step_x;
    int32_t step_y;
    int32_t error;
    int32_t twice_error;
    bool drew_pixel = false;

    if (!canvas_is_valid(canvas) || x0 < -65535 || x0 > 131070 ||
        y0 < -65535 || y0 > 131070 || x1 < -65535 || x1 > 131070 ||
        y1 < -65535 || y1 > 131070) {
        return false;
    }
    dx = absolute_i32(x1 - x0);
    dy = -absolute_i32(y1 - y0);
    step_x = x0 < x1 ? 1 : -1;
    step_y = y0 < y1 ? 1 : -1;
    error = dx + dy;
    for (;;) {
        if (gp100_display_put_pixel(canvas, x0, y0, rgb565)) {
            drew_pixel = true;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        /* Both decisions use the same pre-update error value. */
        twice_error = 2 * error;
        if (twice_error >= dy) {
            error += dy;
            x0 += step_x;
        }
        if (twice_error <= dx) {
            error += dx;
            y0 += step_y;
        }
    }
    return drew_pixel;
}

static int64_t triangle_edge(int32_t ax,
                             int32_t ay,
                             int32_t bx,
                             int32_t by,
                             int32_t px,
                             int32_t py)
{
    return (int64_t)(px - ax) * (by - ay) -
           (int64_t)(py - ay) * (bx - ax);
}

bool gp100_display_fill_triangle(gp100_display_canvas_t *canvas,
                                      int32_t x0,
                                      int32_t y0,
                                      int32_t x1,
                                      int32_t y1,
                                      int32_t x2,
                                      int32_t y2,
                                      uint16_t rgb565)
{
    int32_t minimum_x;
    int32_t maximum_x;
    int32_t minimum_y;
    int32_t maximum_y;
    int64_t area;
    bool drew_pixel = false;

    if (!canvas_is_valid(canvas) || x0 < -65535 || x0 > 131070 ||
        y0 < -65535 || y0 > 131070 || x1 < -65535 || x1 > 131070 ||
        y1 < -65535 || y1 > 131070 || x2 < -65535 || x2 > 131070 ||
        y2 < -65535 || y2 > 131070) {
        return false;
    }
    area = triangle_edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) {
        bool first = gp100_display_draw_line(
            canvas, x0, y0, x1, y1, rgb565);
        bool second = gp100_display_draw_line(
            canvas, x1, y1, x2, y2, rgb565);
        return gp100_display_draw_line(
                   canvas, x2, y2, x0, y0, rgb565) ||
               first || second;
    }
    minimum_x = x0 < x1 ? x0 : x1;
    minimum_x = minimum_x < x2 ? minimum_x : x2;
    maximum_x = x0 > x1 ? x0 : x1;
    maximum_x = maximum_x > x2 ? maximum_x : x2;
    minimum_y = y0 < y1 ? y0 : y1;
    minimum_y = minimum_y < y2 ? minimum_y : y2;
    maximum_y = y0 > y1 ? y0 : y1;
    maximum_y = maximum_y > y2 ? maximum_y : y2;
    if (minimum_x < 0) {
        minimum_x = 0;
    }
    if (minimum_y < 0) {
        minimum_y = 0;
    }
    if (maximum_x >= canvas->width) {
        maximum_x = canvas->width - 1;
    }
    if (maximum_y >= canvas->height) {
        maximum_y = canvas->height - 1;
    }
    for (int32_t y = minimum_y; y <= maximum_y; ++y) {
        for (int32_t x = minimum_x; x <= maximum_x; ++x) {
            int64_t edge0 = triangle_edge(x0, y0, x1, y1, x, y);
            int64_t edge1 = triangle_edge(x1, y1, x2, y2, x, y);
            int64_t edge2 = triangle_edge(x2, y2, x0, y0, x, y);
            if ((area > 0 && edge0 >= 0 && edge1 >= 0 && edge2 >= 0) ||
                (area < 0 && edge0 <= 0 && edge1 <= 0 && edge2 <= 0)) {
                canvas->pixels[pixel_index(canvas, (size_t)x,
                                           (size_t)y)] = rgb565;
                drew_pixel = true;
            }
        }
    }
    return drew_pixel;
}

static bool indexed_arguments_are_valid(
    const gp100_display_canvas_t *canvas,
    uint16_t width,
    uint16_t height,
    const uint8_t *source,
    size_t source_row_bytes,
    size_t source_x,
    uint8_t bits_per_pixel,
    const uint16_t *palette,
    size_t palette_count)
{
    size_t required_bits;
    size_t entries;

    if (!canvas_is_valid(canvas) || source == NULL || width == 0u ||
        height == 0u || (bits_per_pixel != 1u && bits_per_pixel != 2u &&
                         bits_per_pixel != 4u && bits_per_pixel != 8u)) {
        return false;
    }
    if (source_x > (SIZE_MAX / bits_per_pixel) - width ||
        source_row_bytes > SIZE_MAX / CHAR_BIT) {
        return false;
    }
    required_bits = (source_x + width) * bits_per_pixel;
    if (required_bits > source_row_bytes * CHAR_BIT) {
        return false;
    }
    entries = (size_t)1u << bits_per_pixel;
    if (palette != NULL && palette_count < entries) {
        return false;
    }
    return source_row_bytes <= SIZE_MAX / height;
}

bool gp100_display_blit_indexed(
    gp100_display_canvas_t *canvas,
    int32_t destination_x,
    int32_t destination_y,
    uint16_t width,
    uint16_t height,
    const uint8_t *source,
    size_t source_row_bytes,
    size_t source_x,
    uint8_t bits_per_pixel,
    const uint16_t *palette,
    size_t palette_count,
    bool transparent_zero)
{
    uint16_t row;
    uint8_t index_mask;
    bool drew_pixel = false;

    if (!indexed_arguments_are_valid(canvas, width, height, source,
                                     source_row_bytes, source_x,
                                     bits_per_pixel, palette,
                                     palette_count)) {
        return false;
    }
    index_mask = (uint8_t)(((uint16_t)1u << bits_per_pixel) - 1u);

    for (row = 0u; row < height; ++row) {
        const uint8_t *source_row = source + (size_t)row * source_row_bytes;
        uint16_t column;
        int64_t y = (int64_t)destination_y + row;

        if (y < 0 || y >= canvas->height) {
            continue;
        }
        for (column = 0u; column < width; ++column) {
            size_t bit_index = (source_x + column) * bits_per_pixel;
            uint8_t packed = source_row[bit_index / CHAR_BIT];
            uint8_t shift = (uint8_t)(CHAR_BIT - bits_per_pixel -
                                      (bit_index % CHAR_BIT));
            uint8_t index = (uint8_t)((packed >> shift) & index_mask);
            int64_t x = (int64_t)destination_x + column;

            if (x < 0 || x >= canvas->width ||
                (transparent_zero && index == 0u)) {
                continue;
            }
            canvas->pixels[pixel_index(canvas, (size_t)x, (size_t)y)] =
                palette == NULL ? (uint16_t)index : palette[index];
            drew_pixel = true;
        }
    }
    return drew_pixel;
}
