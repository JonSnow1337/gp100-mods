#include "hardware_test_screen.h"

#include <stddef.h>

enum {
    kSliderX = 50,
    kSliderY = 34,
    kSliderWidth = 70,
    kSliderHeight = 13,
    kKeyX = 50,
    kKeyY = 50,
    kKeyWidth = 80,
    kKeyHeight = 16,
    kEffectX = 3,
    kEffectY = 68,
    kEffectWidth = 60,
    kEffectHeight = 17,
    kInstructionX = 2,
    kInstructionY = 87,
    kInstructionWidth = 130,
    kInstructionHeight = 13,
};

static bool append_text(char *buffer,
                        size_t capacity,
                        size_t *length,
                        const char *text)
{
    if (buffer == NULL || length == NULL || text == NULL ||
        *length >= capacity) {
        return false;
    }
    while (*text != '\0') {
        if (*length + 1u >= capacity) {
            return false;
        }
        buffer[*length] = *text;
        ++*length;
        ++text;
    }
    buffer[*length] = '\0';
    return true;
}

static bool format_fixed(char *buffer,
                         size_t capacity,
                         int32_t value,
                         uint8_t width)
{
    uint32_t magnitude;
    size_t digits = 1u;
    size_t prefix = 0u;
    size_t index;
    bool negative = value < 0;

    if (buffer == NULL || capacity <= width || width == 0u) {
        return false;
    }
    magnitude = negative ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
    for (uint32_t probe = magnitude; probe >= 10u; probe /= 10u) {
        ++digits;
    }
    if (negative) {
        ++digits;
    }
    if (digits < width) {
        prefix = width - digits;
    }
    for (index = 0u; index < prefix; ++index) {
        buffer[index] = ' ';
    }
    index = width;
    buffer[index] = '\0';
    do {
        buffer[--index] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u && index != 0u);
    if (negative && index != 0u) {
        buffer[--index] = '-';
    }
    return true;
}

static void draw_outline(gp100_display_canvas_t *canvas,
                         int32_t x,
                         int32_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t color)
{
    (void)gp100_display_fill_rect(canvas, x, y, width, 1u, color);
    (void)gp100_display_fill_rect(
        canvas, x, y + (int32_t)height - 1, width, 1u, color);
    (void)gp100_display_fill_rect(canvas, x, y, 1u, height, color);
    (void)gp100_display_fill_rect(
        canvas, x + (int32_t)width - 1, y, 1u, height, color);
}

static bool draw_box(gp100_display_canvas_t *canvas,
                     const gp100_bitmap_font_t *font,
                     int32_t x,
                     int32_t y,
                     uint16_t width,
                     uint16_t height,
                     const char *text,
                     bool active,
                     const gp100_hardware_test_theme_t *theme)
{
    gp100_text_style_t style;
    uint16_t foreground = active ? theme->background_rgb565
                                 : theme->foreground_rgb565;
    uint16_t background = active ? theme->accent_rgb565
                                 : theme->background_rgb565;
    int32_t text_y;

    (void)gp100_display_fill_rect(canvas, x, y, width, height,
                                       background);
    draw_outline(canvas, x, y, width, height, theme->foreground_rgb565);
    style.foreground_rgb565 = foreground;
    style.background_rgb565 = background;
    style.alignment = GP100_TEXT_ALIGN_CENTER;
    style.transparent_background = true;
    text_y = y + ((int32_t)height - font->line_height) / 2;
    return gp100_display_draw_text(
        canvas, font, x + (int32_t)(width / 2u), text_y, text, &style);
}

static bool draw_plain(gp100_display_canvas_t *canvas,
                       const gp100_bitmap_font_t *font,
                       int32_t x,
                       int32_t y,
                       const char *text,
                       const gp100_hardware_test_theme_t *theme)
{
    gp100_text_style_t style = {
        theme->foreground_rgb565,
        theme->background_rgb565,
        GP100_TEXT_ALIGN_LEFT,
        true,
    };

    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

/* GP100_UI_ASSET_REPLACEMENT: diagnostic layout, key and encoder indicators. */
bool gp100_hardware_test_render(
    gp100_display_canvas_t *canvas,
    const gp100_hardware_test_snapshot_t *snapshot,
    const gp100_hardware_test_theme_t *theme)
{
    char hardware_line[64] = "Board: ";
    char version_line[48] = "version:";
    char value[16];
    size_t hardware_length = sizeof("Board: ") - 1u;
    size_t version_length = 8u;
    uint16_t slider_fill;
    const char *key_label;
    const char *effect_label;
    const char *instruction;
    const gp100_bitmap_font_t *small_font;

    if (canvas == NULL || snapshot == NULL || theme == NULL ||
        snapshot->test == NULL || theme->regular_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT) {
        return false;
    }
    small_font = theme->small_font != NULL ? theme->small_font
                                           : theme->regular_font;
    if (!append_text(hardware_line, sizeof(hardware_line), &hardware_length,
                     snapshot->hardware_info != NULL
                         ? snapshot->hardware_info
                         : "") ||
        !append_text(version_line, sizeof(version_line), &version_length,
                     snapshot->version != NULL ? snapshot->version : "")) {
        return false;
    }

    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);

    if (!draw_plain(canvas, theme->regular_font, 0, 16, "TUNER:", theme) ||
        !draw_plain(canvas, theme->regular_font, 0, 0, "PEDAL:", theme) ||
        !draw_plain(canvas, theme->regular_font, 0, 32, "Encode:", theme) ||
        !draw_plain(canvas, theme->regular_font, 125, 32, "ID:", theme) ||
        !draw_plain(canvas, theme->regular_font, 0, 48, "Key:", theme)) {
        return false;
    }
    if (!format_fixed(value, sizeof(value), snapshot->tuner_value, 3u) ||
        !draw_plain(canvas, theme->regular_font, 48, 16, value, theme) ||
        !format_fixed(value, sizeof(value), snapshot->pedal_value, 3u) ||
        !draw_plain(canvas, theme->regular_font, 48, 0, value, theme) ||
        !format_fixed(value, sizeof(value), snapshot->encoder_id, 1u) ||
        !draw_plain(canvas, theme->regular_font, 142, 32, value, theme)) {
        return false;
    }

    (void)gp100_display_fill_rect(
        canvas, kSliderX, kSliderY, kSliderWidth, kSliderHeight,
        theme->background_rgb565);
    draw_outline(canvas, kSliderX, kSliderY, kSliderWidth, kSliderHeight,
                 theme->foreground_rgb565);
    slider_fill = (uint16_t)(((uint32_t)(kSliderWidth - 2) *
                              snapshot->test->encoder_value) /
                             GP100_HARDWARE_TEST_ENCODER_MAX);
    if (slider_fill != 0u) {
        (void)gp100_display_fill_rect(
            canvas, kSliderX + 1, kSliderY + 1, slider_fill,
            kSliderHeight - 2, theme->accent_rgb565);
    }

    key_label = gp100_hardware_test_key_label(snapshot->test);
    effect_label = snapshot->test->effect_enabled ? "EFFECT" : "BYPASS";
    instruction = snapshot->test->effect_enabled
                      ? "Hold L+R for BYPASS"
                      : "Hold L+R for EFFECT";
    if (!draw_box(canvas, small_font, kKeyX, kKeyY, kKeyWidth, kKeyHeight,
                  key_label, snapshot->test->key_pressed, theme) ||
        !draw_box(canvas, small_font, kEffectX, kEffectY, kEffectWidth,
                  kEffectHeight, effect_label, false, theme) ||
        !draw_box(canvas, small_font, kInstructionX, kInstructionY,
                  kInstructionWidth, kInstructionHeight, instruction, false,
                  theme) ||
        !draw_plain(canvas, small_font, 2, 102, version_line, theme) ||
        !draw_plain(canvas, small_font, 2, 111, hardware_line, theme) ||
        (snapshot->hardware_variant != NULL &&
         !draw_plain(canvas, small_font, 2, 120, snapshot->hardware_variant,
                     theme))) {
        return false;
    }
    if (snapshot->test->completion_visible &&
        (!draw_plain(canvas, small_font, 80, 0, "Test Complete", theme) ||
         !draw_plain(canvas, small_font, 82, 11, "Press any key", theme) ||
         !draw_plain(canvas, small_font, 82, 22, "to exit", theme))) {
        return false;
    }
    return true;
}
