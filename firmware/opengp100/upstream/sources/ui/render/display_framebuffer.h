#ifndef GP100_DISPLAY_FRAMEBUFFER_H
#define GP100_DISPLAY_FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DISPLAY_WIDTH 128u
#define GP100_DISPLAY_HEIGHT 160u
#define GP100_DISPLAY_PIXEL_COUNT \
    (GP100_DISPLAY_WIDTH * GP100_DISPLAY_HEIGHT)
#define GP100_DISPLAY_SPI_HZ UINT32_C(10000000)
#define GP100_DISPLAY_INIT_COMMAND_COUNT 16u
#define GP100_DISPLAY_INIT_DATA_MAX 16u

typedef struct gp100_display_init_command {
    uint8_t command;
    uint8_t data_length;
    uint16_t delay_after_ms;
    uint8_t data[GP100_DISPLAY_INIT_DATA_MAX];
} gp100_display_init_command_t;

extern const gp100_display_init_command_t
    gp100_display_init_commands[GP100_DISPLAY_INIT_COMMAND_COUNT];

typedef enum gp100_display_flush_result {
    GP100_DISPLAY_FLUSH_TRANSFER_FAILED = -2,
    GP100_DISPLAY_FLUSH_INVALID = -1,
    GP100_DISPLAY_UNCHANGED = 0,
    GP100_DISPLAY_FLUSHED = 1
} gp100_display_flush_result_t;

/*
 * V2.1 renders into one RGB565 buffer and keeps a second DMA-visible copy.
 * dirty is retained explicitly because the factory routine also carries it
 * between calls, although normal blocking transfers clear it immediately.
 */
typedef struct gp100_display_framebuffer {
    uint16_t *render_pixels;
    uint16_t *transfer_pixels;
    bool dirty;
} gp100_display_framebuffer_t;

typedef struct gp100_display_hooks {
    void *context;
    void (*clean_data_cache)(void *context,
                             const void *address,
                             size_t byte_count);
    bool (*set_window)(void *context,
                       uint16_t x_start,
                       uint16_t x_end,
                       uint16_t y_start,
                       uint16_t y_end);
    bool (*transfer_rgb565_blocking)(void *context,
                                     const uint16_t *pixels,
                                     size_t pixel_count);
} gp100_display_hooks_t;

bool gp100_display_hooks_are_valid(
    const gp100_display_hooks_t *hooks);

gp100_display_flush_result_t gp100_display_flush(
    gp100_display_framebuffer_t *framebuffer,
    const gp100_display_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
