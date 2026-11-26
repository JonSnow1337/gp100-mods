#include "mode_chip.h"

#include "ui/ui_palette.h"

uint16_t gp100_mode_chip_color(gp100_ui_mode_t mode)
{
    switch (mode) {
    case GP100_UI_MODE_PLAY: return GP100_UI_MODE_PLAY_COLOR;
    case GP100_UI_MODE_EDIT: return GP100_UI_MODE_EDIT_COLOR;
    case GP100_UI_MODE_CONSTRUCT: return GP100_UI_MODE_CHAIN_COLOR;
    case GP100_UI_MODE_SAVE: return GP100_UI_MODE_SAVE_COLOR;
    case GP100_UI_MODE_GLOBAL: return GP100_UI_MODE_GLOBAL_COLOR;
    case GP100_UI_MODE_TUNER: return GP100_UI_MODE_TUNER_COLOR;
    case GP100_UI_MODE_DRUM: return GP100_UI_MODE_DRUM_COLOR;
    case GP100_UI_MODE_LOOPER: return GP100_UI_MODE_LOOPER_COLOR;
    default: return GP100_UI_FOREGROUND;
    }
}

const char *gp100_mode_chip_label(gp100_ui_mode_t mode)
{
    switch (mode) {
    case GP100_UI_MODE_PLAY: return "PLAY";
    case GP100_UI_MODE_EDIT: return "EDIT";
    case GP100_UI_MODE_CONSTRUCT: return "SIGNAL CHAIN";
    case GP100_UI_MODE_SAVE: return "SAVE";
    case GP100_UI_MODE_GLOBAL: return "GLOBAL";
    case GP100_UI_MODE_TUNER: return "TUNER";
    case GP100_UI_MODE_DRUM: return "DRUM";
    case GP100_UI_MODE_LOOPER: return "LOOPER";
    default: return NULL;
    }
}

bool gp100_mode_chip_draw(gp100_display_canvas_t *canvas,
                          const gp100_bitmap_font_t *font,
                          const char *label, uint16_t color,
                          uint16_t text_color,
                          int32_t right_x, int32_t y)
{
    uint16_t width;
    uint16_t height;
    gp100_text_style_t style = {text_color, color, GP100_TEXT_ALIGN_RIGHT, false};

    if (canvas == NULL || font == NULL || label == NULL ||
        !gp100_display_measure_text(font, label, &width, &height)) {
        return false;
    }
    (void)gp100_display_fill_rect(canvas, right_x - (int32_t)width - 5, y,
                                  (uint16_t)(width + 6u), (uint16_t)(height + 3u), color);
    return gp100_display_draw_text(canvas, font, right_x - 2, y + 2, label, &style);
}
