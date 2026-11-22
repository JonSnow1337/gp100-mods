#ifndef GP100_DISPLAY_CANVAS_H
#define GP100_DISPLAY_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_display_canvas {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t storage_width;
    uint16_t storage_height;
    size_t stride_pixels;
    uint8_t rotation;
} gp100_display_canvas_t;

typedef enum gp100_display_rotation {
    GP100_DISPLAY_ROTATION_NONE = 0,
    GP100_DISPLAY_ROTATION_CLOCKWISE = 1,
    GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE = 2
} gp100_display_rotation_t;

/* Convert the factory GUI's 0x00BBGGRR color literals to RGB565. */
uint16_t gp100_display_bgr888_to_rgb565(uint32_t bgr888);

bool gp100_display_canvas_init(gp100_display_canvas_t *canvas,
                                    uint16_t *pixels,
                                    uint16_t width,
                                    uint16_t height,
                                    size_t stride_pixels);

/*
 * Present physical portrait storage as a rotated logical canvas. A 128x160
 * buffer becomes a 160x128 drawing surface without allocating another frame.
 * The factory selects COUNTER_CLOCKWISE: (x, y) maps to (127 - y, x).
 */
bool gp100_display_canvas_init_rotated(
    gp100_display_canvas_t *canvas,
    uint16_t *pixels,
    uint16_t storage_width,
    uint16_t storage_height,
    size_t stride_pixels,
    gp100_display_rotation_t rotation);

bool gp100_display_put_pixel(gp100_display_canvas_t *canvas,
                                  int32_t x,
                                  int32_t y,
                                  uint16_t rgb565);

/* Fill the visible intersection of the rectangle and canvas. */
bool gp100_display_fill_rect(gp100_display_canvas_t *canvas,
                                  int32_t x,
                                  int32_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t rgb565);

/* Draw a clipped one-pixel Bresenham line. */
bool gp100_display_draw_line(gp100_display_canvas_t *canvas,
                                  int32_t x0,
                                  int32_t y0,
                                  int32_t x1,
                                  int32_t y1,
                                  uint16_t rgb565);

/* Fill a clipped triangle. Vertices may be clockwise or counter-clockwise. */
bool gp100_display_fill_triangle(gp100_display_canvas_t *canvas,
                                      int32_t x0,
                                      int32_t y0,
                                      int32_t x1,
                                      int32_t y1,
                                      int32_t x2,
                                      int32_t y2,
                                      uint16_t rgb565);

/*
 * Expand an MSB-first 1/2/4/8-bit indexed bitmap. source_x selects the first
 * source pixel in every row. A NULL palette writes the index as the RGB565
 * value. When transparent_zero is true, index zero leaves the destination
 * unchanged, matching factory render mode 2.
 */
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
    bool transparent_zero);

#ifdef __cplusplus
}
#endif

#endif
