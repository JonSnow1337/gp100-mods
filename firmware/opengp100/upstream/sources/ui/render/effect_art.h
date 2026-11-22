#ifndef GP100_EFFECT_ART_H
#define GP100_EFFECT_ART_H

#include "storage/preset_record.h"
#include "ui/render/display_canvas.h"

#define GP100_EFFECT_ART_WIDTH 27u
#define GP100_EFFECT_ART_HEIGHT 25u

/* Draw the same enclosure at card or thumbnail size. */
void gp100_effect_art_draw(gp100_display_canvas_t *canvas, int32_t x, int32_t y,
                           uint16_t width, uint16_t height,
                           gp100_effect_family_t family, uint16_t model,
                           uint16_t accent, uint16_t background, bool enabled);
void gp100_effect_art_draw_silhouette(gp100_display_canvas_t *canvas,
    int32_t x, int32_t y, gp100_effect_family_t family, uint16_t model, uint16_t color);

#endif
