#ifndef GP100_MODE_CHIP_H
#define GP100_MODE_CHIP_H

#include "ui/render/display_text.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Accent color of a mode (the Signal Chain editor and the effect picker have their own). */
uint16_t gp100_mode_chip_color(gp100_ui_mode_t mode);

/* Short name shown on the chip, e.g. "PLAY" or "GLOBAL". NULL for modes without a chip. */
const char *gp100_mode_chip_label(gp100_ui_mode_t mode);

/* Filled chip with dark text, right edge at right_x, top edge at y. */
bool gp100_mode_chip_draw(gp100_display_canvas_t *canvas,
                          const gp100_bitmap_font_t *font,
                          const char *label, uint16_t color,
                          uint16_t text_color,
                          int32_t right_x, int32_t y);

#ifdef __cplusplus
}
#endif

#endif
