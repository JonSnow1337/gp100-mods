#ifndef GP100_FACTORY_RESET_SCREEN_H
#define GP100_FACTORY_RESET_SCREEN_H

#include "ui/render/display_text.h"
#include "storage/factory_reset_plan.h"
#include "ui/ui_geometry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_factory_reset_phase {
    GP100_FACTORY_RESET_CONFIRM = 0,
    GP100_FACTORY_RESET_REQUESTED,
    GP100_FACTORY_RESET_PROGRESS,
    GP100_FACTORY_RESET_COMPLETE,
    GP100_FACTORY_RESET_CANCELLED
} gp100_factory_reset_phase_t;

typedef struct gp100_factory_reset_state {
    gp100_factory_reset_phase_t phase;
    bool reset_selected;
    size_t completed_steps;
    size_t total_steps;
} gp100_factory_reset_state_t;

typedef struct gp100_factory_reset_theme {
    const gp100_bitmap_font_t *regular_font;
    const gp100_bitmap_font_t *small_font;
    uint16_t background_rgb565;
    uint16_t foreground_rgb565;
    uint16_t selected_rgb565;
    uint16_t panel_rgb565;
    uint16_t track_rgb565;
} gp100_factory_reset_theme_t;

void gp100_factory_reset_init(gp100_factory_reset_state_t *state);

bool gp100_factory_reset_select(
    gp100_factory_reset_state_t *state,
    bool reset_selected);

/*
 * CONFIRM activation either requests a board-owned reset or cancels. This
 * portable controller never erases flash itself.
 */
bool gp100_factory_reset_activate(
    gp100_factory_reset_state_t *state,
    bool *reset_requested,
    bool *exit_requested);

bool gp100_factory_reset_begin(
    gp100_factory_reset_state_t *state,
    size_t total_steps);

bool gp100_factory_reset_update(
    gp100_factory_reset_state_t *state,
    size_t completed_steps);

unsigned gp100_factory_reset_percent(
    const gp100_factory_reset_state_t *state);

bool gp100_factory_reset_default_theme(
    gp100_factory_reset_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font);

bool gp100_factory_reset_render(
    gp100_display_canvas_t *canvas,
    const gp100_factory_reset_state_t *state,
    const gp100_factory_reset_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif
