#include "ui/screens/language_screen.h"

#include <string.h>

static const gp100_bitmap_font_t empty_font = {
    NULL, 0u, 8u, 5u
};

int main(void)
{
    gp100_language_catalog_t catalog;
    gp100_language_catalog_t invalid;
    gp100_language_screen_state_t state;
    gp100_language_screen_theme_t theme;
    gp100_settings_record_t settings;
    gp100_display_canvas_t canvas;
    uint16_t pixels[GP100_UI_LOGICAL_WIDTH *
                    GP100_UI_LOGICAL_HEIGHT];
    uint8_t identifier = 0u;
    int16_t stored;

    gp100_language_catalog_from_mask(0u, &catalog);
    if (catalog.count != GP100_LANGUAGE_COUNT ||
        strcmp(gp100_language_name(0u), "English") != 0 ||
        strcmp(gp100_language_name(8u),
               "Ð ÑÑÑÐºÐ¸Ð¹") != 0 ||
        gp100_language_name(9u) != NULL ||
        !gp100_language_screen_init(&state, &catalog, 6u) ||
        state.current_index != 6u || state.selected_index != 6u ||
        gp100_language_screen_first_visible(&state) != 2u) {
        return 1;
    }
    if (!gp100_language_screen_move(&state, 2) ||
        state.selected_index != 8u ||
        gp100_language_screen_first_visible(&state) != 4u ||
        !gp100_language_screen_move(&state, 20) ||
        state.selected_index != 8u ||
        !gp100_language_screen_move(&state, -20) ||
        state.selected_index != 0u ||
        !gp100_language_screen_move(&state, 8)) {
        return 2;
    }
    memset(&settings, 0, sizeof(settings));
    if (!gp100_language_screen_commit(
            &state, &settings, &identifier) || identifier != 8u ||
        state.current_index != 8u || !state.committed ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_LANGUAGE_INDEX, &stored) ||
        stored != 8 || !gp100_settings_validate(&settings)) {
        return 3;
    }
    if (!gp100_display_canvas_init(
            &canvas, pixels, GP100_UI_LOGICAL_WIDTH,
            GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH) ||
        !gp100_language_screen_default_theme(
            &theme, &empty_font, NULL) ||
        !gp100_language_screen_render(&canvas, &state, &theme) ||
        pixels[22u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.background_rgb565 ||
        pixels[94u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.selected_rgb565 ||
        pixels[112u * GP100_UI_LOGICAL_WIDTH + 2u] !=
            theme.panel_rgb565) {
        return 4;
    }
    if (!gp100_language_screen_init(&state, &catalog, 31u) ||
        state.current_index != 0u || state.selected_index != 0u) {
        return 5;
    }
    invalid = catalog;
    invalid.identifiers[1] = invalid.identifiers[0];
    if (gp100_language_screen_init(&state, &invalid, 0u)) {
        return 6;
    }
    return 0;
}
