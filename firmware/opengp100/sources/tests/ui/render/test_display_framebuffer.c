#include "ui/render/display_framebuffer.h"

#include <stdint.h>
#include <stdio.h>

enum test_event {
    EVENT_CLEAN_RENDER = 1,
    EVENT_CLEAN_TRANSFER,
    EVENT_SET_WINDOW,
    EVENT_TRANSFER
};

typedef struct test_context {
    uint16_t *render_pixels;
    uint16_t *transfer_pixels;
    unsigned events[8];
    size_t event_count;
    bool arguments_valid;
    bool window_succeeds;
    bool transfer_succeeds;
} test_context_t;

static void clean_data_cache(void *opaque,
                             const void *address,
                             size_t byte_count)
{
    test_context_t *context = opaque;
    unsigned event = address == context->render_pixels
                         ? EVENT_CLEAN_RENDER
                         : EVENT_CLEAN_TRANSFER;
    if (byte_count != GP100_DISPLAY_PIXEL_COUNT * sizeof(uint16_t) ||
        (address != context->render_pixels &&
         address != context->transfer_pixels)) {
        context->arguments_valid = false;
    }
    context->events[context->event_count++] = event;
}

static bool set_window(void *opaque,
                       uint16_t x_start,
                       uint16_t x_end,
                       uint16_t y_start,
                       uint16_t y_end)
{
    test_context_t *context = opaque;
    if (x_start != 0u || x_end != 127u ||
        y_start != 0u || y_end != 159u) {
        context->arguments_valid = false;
    }
    context->events[context->event_count++] = EVENT_SET_WINDOW;
    return context->window_succeeds;
}

static bool transfer_rgb565_blocking(void *opaque,
                                     const uint16_t *pixels,
                                     size_t pixel_count)
{
    test_context_t *context = opaque;
    if (pixels != context->transfer_pixels ||
        pixel_count != GP100_DISPLAY_PIXEL_COUNT) {
        context->arguments_valid = false;
    }
    context->events[context->event_count++] = EVENT_TRANSFER;
    return context->transfer_succeeds;
}

static gp100_display_hooks_t make_hooks(test_context_t *context)
{
    gp100_display_hooks_t hooks = {
        .context = context,
        .clean_data_cache = clean_data_cache,
        .set_window = set_window,
        .transfer_rgb565_blocking = transfer_rgb565_blocking
    };
    return hooks;
}

static int test_unchanged_frame(void)
{
    static uint16_t render_pixels[GP100_DISPLAY_PIXEL_COUNT];
    static uint16_t transfer_pixels[GP100_DISPLAY_PIXEL_COUNT];
    gp100_display_framebuffer_t framebuffer = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .dirty = false
    };
    test_context_t context = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .arguments_valid = true,
        .window_succeeds = true,
        .transfer_succeeds = true
    };
    gp100_display_hooks_t hooks = make_hooks(&context);

    return gp100_display_flush(&framebuffer, &hooks) ==
                   GP100_DISPLAY_UNCHANGED &&
               context.event_count == 0u && !framebuffer.dirty
               ? 0
               : 1;
}

static int test_initialization_script(void)
{
    const gp100_display_init_command_t *positive_gamma =
        &gp100_display_init_commands[11];
    const gp100_display_init_command_t *negative_gamma =
        &gp100_display_init_commands[12];

    return gp100_display_init_commands[0].command == 0x11u &&
               gp100_display_init_commands[0].delay_after_ms == 120u &&
               positive_gamma->command == 0xe0u &&
               positive_gamma->data_length == 16u &&
               positive_gamma->data[4] == 0x3fu &&
               negative_gamma->command == 0xe1u &&
               negative_gamma->data[6] == 0x2au &&
               gp100_display_init_commands[15].command == 0x2cu
               ? 0
               : 1;
}

static int test_changed_frame(void)
{
    static const unsigned expected[] = {
        EVENT_CLEAN_RENDER, EVENT_CLEAN_TRANSFER,
        EVENT_SET_WINDOW, EVENT_TRANSFER
    };
    static uint16_t render_pixels[GP100_DISPLAY_PIXEL_COUNT];
    static uint16_t transfer_pixels[GP100_DISPLAY_PIXEL_COUNT];
    gp100_display_framebuffer_t framebuffer = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .dirty = false
    };
    test_context_t context = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .arguments_valid = true,
        .window_succeeds = true,
        .transfer_succeeds = true
    };
    gp100_display_hooks_t hooks = make_hooks(&context);
    size_t index;

    render_pixels[0] = UINT16_C(0x1234);
    render_pixels[GP100_DISPLAY_PIXEL_COUNT - 1u] = UINT16_C(0xabcd);
    if (gp100_display_flush(&framebuffer, &hooks) !=
            GP100_DISPLAY_FLUSHED ||
        framebuffer.dirty || !context.arguments_valid ||
        context.event_count != sizeof(expected) / sizeof(expected[0]) ||
        transfer_pixels[0] != render_pixels[0] ||
        transfer_pixels[GP100_DISPLAY_PIXEL_COUNT - 1u] !=
            render_pixels[GP100_DISPLAY_PIXEL_COUNT - 1u]) {
        return 1;
    }
    for (index = 0u; index < context.event_count; ++index) {
        if (context.events[index] != expected[index]) {
            return 2;
        }
    }
    return 0;
}

static int test_carried_dirty_flag(void)
{
    static uint16_t render_pixels[GP100_DISPLAY_PIXEL_COUNT];
    static uint16_t transfer_pixels[GP100_DISPLAY_PIXEL_COUNT];
    gp100_display_framebuffer_t framebuffer = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .dirty = true
    };
    test_context_t context = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .arguments_valid = true,
        .window_succeeds = true,
        .transfer_succeeds = true
    };
    gp100_display_hooks_t hooks = make_hooks(&context);

    return gp100_display_flush(&framebuffer, &hooks) ==
                   GP100_DISPLAY_FLUSHED &&
               context.event_count == 4u && !framebuffer.dirty
               ? 0
               : 1;
}

static int test_failed_transfer_is_retried(void)
{
    static uint16_t render_pixels[GP100_DISPLAY_PIXEL_COUNT];
    static uint16_t transfer_pixels[GP100_DISPLAY_PIXEL_COUNT];
    gp100_display_framebuffer_t framebuffer = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .dirty = true
    };
    test_context_t context = {
        .render_pixels = render_pixels,
        .transfer_pixels = transfer_pixels,
        .arguments_valid = true,
        .window_succeeds = true,
        .transfer_succeeds = false
    };
    gp100_display_hooks_t hooks = make_hooks(&context);

    if (gp100_display_flush(&framebuffer, &hooks) !=
            GP100_DISPLAY_FLUSH_TRANSFER_FAILED ||
        !framebuffer.dirty || context.event_count != 4u) {
        return 1;
    }

    context.event_count = 0u;
    context.transfer_succeeds = true;
    return gp100_display_flush(&framebuffer, &hooks) ==
                   GP100_DISPLAY_FLUSHED &&
               !framebuffer.dirty && context.event_count == 4u
               ? 0
               : 2;
}

static int test_invalid_arguments(void)
{
    gp100_display_framebuffer_t framebuffer = {0};
    gp100_display_hooks_t hooks = {0};

    return gp100_display_flush(NULL, &hooks) ==
                   GP100_DISPLAY_FLUSH_INVALID &&
               gp100_display_flush(&framebuffer, &hooks) ==
                   GP100_DISPLAY_FLUSH_INVALID &&
               !gp100_display_hooks_are_valid(NULL) &&
               !gp100_display_hooks_are_valid(&hooks)
               ? 0
               : 1;
}

int main(void)
{
    int result = test_initialization_script();
    if (result != 0) {
        fprintf(stderr, "initialization script test failed: %d\n", result);
        return 1;
    }
    result = test_unchanged_frame();
    if (result != 0) {
        fprintf(stderr, "unchanged frame test failed: %d\n", result);
        return 10 + result;
    }
    result = test_changed_frame();
    if (result != 0) {
        fprintf(stderr, "changed frame test failed: %d\n", result);
        return 20 + result;
    }
    result = test_carried_dirty_flag();
    if (result != 0) {
        fprintf(stderr, "dirty flag test failed: %d\n", result);
        return 30 + result;
    }
    result = test_invalid_arguments();
    if (result != 0) {
        fprintf(stderr, "invalid argument test failed: %d\n", result);
        return 40 + result;
    }
    result = test_failed_transfer_is_retried();
    if (result != 0) {
        fprintf(stderr, "failed transfer retry test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
