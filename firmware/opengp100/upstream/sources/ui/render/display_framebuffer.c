#include "display_framebuffer.h"

const gp100_display_init_command_t
    gp100_display_init_commands[GP100_DISPLAY_INIT_COMMAND_COUNT] = {
        {0x11u, 0u, 120u, {0}},
        {0xb1u, 3u, 0u, {0x05u, 0x3au, 0x3au}},
        {0xb2u, 3u, 0u, {0x05u, 0x3au, 0x3au}},
        {0xb3u, 6u, 0u, {0x05u, 0x3au, 0x3au, 0x05u, 0x3au, 0x3au}},
        {0xb4u, 1u, 0u, {0x03u}},
        {0xc0u, 3u, 0u, {0x62u, 0x02u, 0x04u}},
        {0xc1u, 1u, 0u, {0xc0u}},
        {0xc2u, 2u, 0u, {0x0du, 0x00u}},
        {0xc3u, 2u, 0u, {0x8du, 0x6au}},
        {0xc4u, 2u, 0u, {0x8du, 0xeeu}},
        {0xc5u, 1u, 0u, {0x12u}},
        {0xe0u, 16u, 0u,
         {0x03u, 0x1bu, 0x12u, 0x11u, 0x3fu, 0x3au, 0x32u, 0x34u,
          0x2fu, 0x2bu, 0x30u, 0x3au, 0x00u, 0x01u, 0x02u, 0x05u}},
        {0xe1u, 16u, 0u,
         {0x03u, 0x1bu, 0x12u, 0x11u, 0x32u, 0x2fu, 0x2au, 0x2fu,
          0x2eu, 0x2cu, 0x35u, 0x3fu, 0x00u, 0x00u, 0x01u, 0x05u}},
        {0x36u, 1u, 0u, {0xc8u}},
        {0x3au, 1u, 0u, {0x55u}},
        {0x2cu, 0u, 0u, {0}}
    };

bool gp100_display_hooks_are_valid(
    const gp100_display_hooks_t *hooks)
{
    return hooks != NULL && hooks->clean_data_cache != NULL &&
           hooks->set_window != NULL &&
           hooks->transfer_rgb565_blocking != NULL;
}

gp100_display_flush_result_t gp100_display_flush(
    gp100_display_framebuffer_t *framebuffer,
    const gp100_display_hooks_t *hooks)
{
    const size_t byte_count =
        GP100_DISPLAY_PIXEL_COUNT * sizeof(uint16_t);
    size_t index;

    if (framebuffer == NULL || framebuffer->render_pixels == NULL ||
        framebuffer->transfer_pixels == NULL ||
        !gp100_display_hooks_are_valid(hooks)) {
        return GP100_DISPLAY_FLUSH_INVALID;
    }

    for (index = 0u; index < GP100_DISPLAY_PIXEL_COUNT; ++index) {
        if (framebuffer->transfer_pixels[index] !=
            framebuffer->render_pixels[index]) {
            framebuffer->transfer_pixels[index] =
                framebuffer->render_pixels[index];
            framebuffer->dirty = true;
        }
    }

    if (!framebuffer->dirty) {
        return GP100_DISPLAY_UNCHANGED;
    }

    hooks->clean_data_cache(hooks->context, framebuffer->render_pixels,
                            byte_count);
    hooks->clean_data_cache(hooks->context, framebuffer->transfer_pixels,
                            byte_count);
    if (!hooks->set_window(hooks->context, 0u,
                           GP100_DISPLAY_WIDTH - 1u, 0u,
                           GP100_DISPLAY_HEIGHT - 1u) ||
        !hooks->transfer_rgb565_blocking(
            hooks->context, framebuffer->transfer_pixels,
            GP100_DISPLAY_PIXEL_COUNT)) {
        /* Keep dirty asserted: the caller can retry the complete frame once
         * the transport recovers. */
        return GP100_DISPLAY_FLUSH_TRANSFER_FAILED;
    }
    framebuffer->dirty = false;
    return GP100_DISPLAY_FLUSHED;
}
