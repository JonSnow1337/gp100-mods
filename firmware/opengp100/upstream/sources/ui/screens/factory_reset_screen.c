#include "factory_reset_screen.h"

#include "ui/ui_palette.h"

#include <string.h>

void gp100_factory_reset_init(gp100_factory_reset_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->phase = GP100_FACTORY_RESET_CONFIRM;
        state->total_steps = GP100_FACTORY_RESET_SECTOR_STEPS;
    }
}

bool gp100_factory_reset_select(
    gp100_factory_reset_state_t *state,
    bool reset_selected)
{
    if (state == NULL || state->phase != GP100_FACTORY_RESET_CONFIRM) {
        return false;
    }
    state->reset_selected = reset_selected;
    return true;
}

bool gp100_factory_reset_activate(
    gp100_factory_reset_state_t *state,
    bool *reset_requested,
    bool *exit_requested)
{
    if (state == NULL || reset_requested == NULL || exit_requested == NULL ||
        state->phase != GP100_FACTORY_RESET_CONFIRM) {
        return false;
    }
    *reset_requested = state->reset_selected;
    *exit_requested = !state->reset_selected;
    state->phase = state->reset_selected
                       ? GP100_FACTORY_RESET_REQUESTED
                       : GP100_FACTORY_RESET_CANCELLED;
    return true;
}

bool gp100_factory_reset_begin(
    gp100_factory_reset_state_t *state,
    size_t total_steps)
{
    if (state == NULL ||
        state->phase != GP100_FACTORY_RESET_REQUESTED ||
        total_steps == 0u || total_steps > SIZE_MAX / 100u) {
        return false;
    }
    state->phase = GP100_FACTORY_RESET_PROGRESS;
    state->completed_steps = 0u;
    state->total_steps = total_steps;
    return true;
}

bool gp100_factory_reset_update(
    gp100_factory_reset_state_t *state,
    size_t completed_steps)
{
    if (state == NULL || state->phase != GP100_FACTORY_RESET_PROGRESS ||
        state->total_steps == 0u || completed_steps > state->total_steps ||
        completed_steps < state->completed_steps) {
        return false;
    }
    state->completed_steps = completed_steps;
    if (completed_steps == state->total_steps) {
        state->phase = GP100_FACTORY_RESET_COMPLETE;
    }
    return true;
}

unsigned gp100_factory_reset_percent(
    const gp100_factory_reset_state_t *state)
{
    size_t whole;
    size_t remainder;

    if (state == NULL || state->total_steps == 0u) {
        return 0u;
    }
    if (state->completed_steps >= state->total_steps) {
        return 100u;
    }
    whole = state->completed_steps / state->total_steps;
    remainder = state->completed_steps % state->total_steps;
    return (unsigned)(whole * 100u +
                      (remainder * 100u) / state->total_steps);
}

bool gp100_factory_reset_default_theme(
    gp100_factory_reset_theme_t *theme,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->track_rgb565 = GP100_UI_TRACK;
    return true;
}

static bool draw_text(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t x,
                      int32_t y,
                      const char *text,
                      gp100_text_alignment_t alignment,
                      uint16_t foreground,
                      uint16_t background)
{
    gp100_text_style_t style = {
        foreground, background, alignment, true
    };
    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

static void format_percent(char output[5], unsigned percent)
{
    size_t position = 0u;

    if (percent >= 100u) {
        output[position++] = '1';
        output[position++] = '0';
        output[position++] = '0';
    } else {
        if (percent >= 10u) {
            output[position++] = (char)('0' + percent / 10u);
        }
        output[position++] = (char)('0' + percent % 10u);
    }
    output[position++] = '%';
    output[position] = '\0';
}

static bool render_confirmation(
    gp100_display_canvas_t *canvas,
    const gp100_factory_reset_state_t *state,
    const gp100_factory_reset_theme_t *theme)
{
    uint16_t cancel_background = state->reset_selected
                                     ? theme->panel_rgb565
                                     : theme->selected_rgb565;
    uint16_t reset_background = state->reset_selected
                                    ? theme->selected_rgb565
                                    : theme->panel_rgb565;

    (void)gp100_display_fill_rect(
        canvas, 2, 83, 156u, 43u, theme->panel_rgb565);
    (void)gp100_display_fill_rect(
        canvas, 8, 101, 65u, 19u, cancel_background);
    (void)gp100_display_fill_rect(
        canvas, 87, 101, 65u, 19u, reset_background);
    return draw_text(canvas, theme->regular_font, 80, 30,
                     "Delete all saved changes?",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           draw_text(canvas, theme->small_font, 80, 52,
                     "This cannot be undone",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 40, 104, "Cancel",
                     GP100_TEXT_ALIGN_CENTER,
                     state->reset_selected ? theme->foreground_rgb565
                                           : theme->background_rgb565,
                     cancel_background) &&
           draw_text(canvas, theme->regular_font, 120, 104, "OK",
                     GP100_TEXT_ALIGN_CENTER,
                     state->reset_selected ? theme->background_rgb565
                                           : theme->foreground_rgb565,
                     reset_background);
}

static bool render_progress(
    gp100_display_canvas_t *canvas,
    const gp100_factory_reset_state_t *state,
    const gp100_factory_reset_theme_t *theme)
{
    unsigned percent = gp100_factory_reset_percent(state);
    char percent_text[5];

    format_percent(percent_text, percent);
    (void)gp100_display_fill_rect(
        canvas, 10, 72, 140u, 12u, theme->track_rgb565);
    if (percent != 0u) {
        (void)gp100_display_fill_rect(
            canvas, 10, 72, (uint16_t)(140u * percent / 100u), 12u,
            theme->selected_rgb565);
    }
    return draw_text(canvas, theme->regular_font, 80, 31,
                     "Resetting...",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           draw_text(canvas, theme->small_font, 80, 52,
                     "Do not disconnect power",
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 80, 92, percent_text,
                     GP100_TEXT_ALIGN_CENTER,
                     theme->foreground_rgb565, theme->background_rgb565);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_factory_reset_render(
    gp100_display_canvas_t *canvas,
    const gp100_factory_reset_state_t *state,
    const gp100_factory_reset_theme_t *theme)
{
    if (canvas == NULL || state == NULL || theme == NULL ||
        theme->regular_font == NULL || theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        (unsigned)state->phase > GP100_FACTORY_RESET_CANCELLED) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    if (!draw_text(canvas, theme->regular_font, 5, 5, "< Factory Reset",
                   GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }
    if (state->phase == GP100_FACTORY_RESET_CONFIRM) {
        return render_confirmation(canvas, state, theme);
    }
    if (state->phase == GP100_FACTORY_RESET_REQUESTED ||
        state->phase == GP100_FACTORY_RESET_PROGRESS) {
        return render_progress(canvas, state, theme);
    }
    if (state->phase == GP100_FACTORY_RESET_COMPLETE) {
        (void)gp100_display_fill_rect(
            canvas, 2, 83, 156u, 43u, theme->panel_rgb565);
        return draw_text(canvas, theme->regular_font, 80, 43,
                         "Reset complete",
                         GP100_TEXT_ALIGN_CENTER,
                         theme->foreground_rgb565,
                         theme->background_rgb565) &&
               draw_text(canvas, theme->regular_font, 80, 98, "OK",
                         GP100_TEXT_ALIGN_CENTER,
                         theme->foreground_rgb565, theme->panel_rgb565);
    }
    return true;
}
