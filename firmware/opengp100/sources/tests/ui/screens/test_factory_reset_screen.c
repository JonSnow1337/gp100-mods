#include "ui/screens/factory_reset_screen.h"

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

int main(void)
{
    gp100_factory_reset_state_t state;
    gp100_factory_reset_theme_t theme;
    gp100_display_canvas_t canvas;
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    bool reset_requested;
    bool exit_requested;

    gp100_factory_reset_init(&state);
    if (state.phase != GP100_FACTORY_RESET_CONFIRM ||
        state.reset_selected ||
        state.total_steps != GP100_FACTORY_RESET_SECTOR_STEPS ||
        !gp100_factory_reset_activate(
            &state, &reset_requested, &exit_requested) ||
        reset_requested || !exit_requested ||
        state.phase != GP100_FACTORY_RESET_CANCELLED) {
        return 1;
    }
    gp100_factory_reset_init(&state);
    if (!gp100_factory_reset_select(&state, true) ||
        !gp100_factory_reset_activate(
            &state, &reset_requested, &exit_requested) ||
        !reset_requested || exit_requested ||
        state.phase != GP100_FACTORY_RESET_REQUESTED ||
        !gp100_factory_reset_begin(
            &state, GP100_FACTORY_RESET_SECTOR_STEPS) ||
        state.phase != GP100_FACTORY_RESET_PROGRESS ||
        gp100_factory_reset_percent(&state) != 0u) {
        return 2;
    }
    if (!gp100_factory_reset_update(&state, 84u) ||
        gp100_factory_reset_percent(&state) != 50u ||
        gp100_factory_reset_update(&state, 83u) ||
        gp100_factory_reset_update(
            &state, GP100_FACTORY_RESET_SECTOR_STEPS + 1u) ||
        !gp100_factory_reset_update(
            &state, GP100_FACTORY_RESET_SECTOR_STEPS) ||
        state.phase != GP100_FACTORY_RESET_COMPLETE ||
        gp100_factory_reset_percent(&state) != 100u) {
        return 3;
    }
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_factory_reset_default_theme(
            &theme, &empty_font, NULL) ||
        !gp100_factory_reset_render(&canvas, &state, &theme) ||
        pixels[83u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.panel_rgb565) {
        return 4;
    }
    gp100_factory_reset_init(&state);
    if (!gp100_factory_reset_render(&canvas, &state, &theme) ||
        pixels[101u * GP100_UI_LOGICAL_WIDTH + 8u] !=
            theme.selected_rgb565 ||
        !gp100_factory_reset_select(&state, true) ||
        !gp100_factory_reset_render(&canvas, &state, &theme) ||
        pixels[101u * GP100_UI_LOGICAL_WIDTH + 87u] !=
            theme.selected_rgb565) {
        return 5;
    }
    return 0;
}
