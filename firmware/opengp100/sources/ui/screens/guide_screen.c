#include "guide_screen.h"

#include "ui/render/knob_ui.h"
#include "ui/ui_geometry.h"
#include "ui/ui_palette.h"

typedef struct guide_page {
    const char *title;
    const char *lines[GP100_GUIDE_MAX_LINES];
} guide_page_t;

/* At most 26 characters per line (small font, 160 px wide). {1} {2} {3} draw the knob circles:
 * the filled one is the knob (left, middle, right), the same marker the screens show above each knob. */
static const guide_page_t pages[GP100_GUIDE_PAGE_COUNT] = {
    {"Basics",
     {"PARA turn: select effect", "PARA press: next page",
      "EDIT: change the effect", "Hold PARA: signal chain",
      "EXIT: go back one step", "Hold GLOBAL: this guide",
      "The 3 knobs, left to right:", "{1}Q1   {2}Q2   {3}Q3"}},
    {"Change an effect",
     {"In EDIT press EDIT.", "Turn PARA to browse: the",
      "effect plays while you", "scroll.",
      "{1}group  {2}effect", "{3}preview on / off",
      "PARA keeps the effect,", "EXIT puts the old one back"}},
    {"Signal chain",
     {"Hold PARA in EDIT.", "Turn PARA: select a slot",
      "PARA on [+]: add effect", "{1}footswitch  {3}move",
      "EDIT: insert, remove, EXP", "or clear the whole preset",
      "Change an effect in EDIT", "Hold PARA: back to EDIT"}},
    {"Remove an effect",
     {"Signal chain: EDIT, then", "Remove. EXIT right after",
      "undoes it.", "",
      "Global > Editing: hold", "EDIT to remove at once.",
      NULL, NULL}},
    {"Saving",
     {"Press SAVE. Choose the", "slot with PARA.",
      "{1}letter  {2}cursor", "{3}delete",
      "Press SAVE again to", "store it. EXIT cancels.",
      "* = unsaved: PARA in Play", "asks before dropping it"}},
    {"Footswitches",
     {"Global > Footswitch Mode:", "PATCH: -/+ change preset",
      "STOMP: -/+ switch effects", "(set in the signal chain",
      "with {1})", "Hold both: looper", NULL, NULL}},
    {"Drums",
     {"DRUM: drums on / off", "Hold DRUM: drum settings",
      "EXIT: back to playing", NULL, NULL, NULL, NULL, NULL}}
};

size_t gp100_guide_page_for_mode(gp100_ui_mode_t mode, bool picker_open)
{
    if (picker_open) {
        return 1u;
    }
    switch (mode) {
    case GP100_UI_MODE_EDIT: return 1u;
    case GP100_UI_MODE_CONSTRUCT: return 2u;
    case GP100_UI_MODE_SAVE: return 4u;
    case GP100_UI_MODE_DRUM:
    case GP100_UI_MODE_LOOPER: return 6u;
    default: return 0u;
    }
}

const char *gp100_guide_page_title(size_t page)
{
    return page < GP100_GUIDE_PAGE_COUNT ? pages[page].title : NULL;
}

const char *gp100_guide_page_line(size_t page, size_t index)
{
    return page < GP100_GUIDE_PAGE_COUNT && index < GP100_GUIDE_MAX_LINES
               ? pages[page].lines[index] : NULL;
}

static bool draw(gp100_display_canvas_t *canvas, const gp100_bitmap_font_t *font,
                 int32_t x, int32_t y, const char *text, gp100_text_alignment_t alignment,
                 uint16_t color)
{
    const gp100_text_style_t style = {color, GP100_UI_BACKGROUND, alignment, true};
    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

/* Draw one guide line; "{1}" "{2}" "{3}" are knob circles. */
static bool draw_line(gp100_display_canvas_t *canvas, const gp100_bitmap_font_t *font,
                      int32_t x, int32_t y, const char *text)
{
    while (*text != '\0') {
        if (text[0] == '{' && text[1] >= '1' && text[1] <= '3' && text[2] == '}') {
            if (!gp100_knob_glyph_draw(canvas, x + 11, y + 1, (size_t)(text[1] - '1'),
                                       GP100_UI_ACCENT, GP100_UI_TRACK)) {
                return false;
            }
            x += 25;
            text += 3;
        } else {
            char piece[32];
            size_t length = 0u;
            uint16_t width;
            uint16_t height;

            while (text[length] != '\0' && text[length] != '{' && length + 1u < sizeof(piece)) {
                piece[length] = text[length];
                ++length;
            }
            piece[length] = '\0';
            text += length;
            if (!gp100_display_measure_text(font, piece, &width, &height) ||
                !draw(canvas, font, x, y, piece, GP100_TEXT_ALIGN_LEFT, GP100_UI_FOREGROUND)) {
                return false;
            }
            x += width;
        }
    }
    return true;
}

bool gp100_guide_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font,
    size_t page)
{
    char counter[8] = {(char)('1' + page), '/', (char)('0' + GP100_GUIDE_PAGE_COUNT), '\0'};

    if (canvas == NULL || large_font == NULL || regular_font == NULL || small_font == NULL ||
        page >= GP100_GUIDE_PAGE_COUNT || canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT) {
        return false;
    }
    (void)gp100_display_fill_rect(canvas, 0, 0, GP100_UI_LOGICAL_WIDTH, GP100_UI_LOGICAL_HEIGHT,
                                  GP100_UI_BACKGROUND);
    if (!draw(canvas, large_font, 5, 4, "GUIDE", GP100_TEXT_ALIGN_LEFT, GP100_UI_FOREGROUND) ||
        !draw(canvas, small_font, 157, 22, counter, GP100_TEXT_ALIGN_RIGHT, GP100_UI_MUTED) ||
        !draw(canvas, regular_font, 5, 23, pages[page].title, GP100_TEXT_ALIGN_LEFT,
              GP100_UI_ACCENT)) {
        return false;
    }
    (void)gp100_display_fill_rect(canvas, 0, 36, GP100_UI_LOGICAL_WIDTH, 1u, GP100_UI_TRACK);
    for (size_t i = 0u; i < GP100_GUIDE_MAX_LINES; ++i) {
        const char *line = pages[page].lines[i];
        if (line != NULL && !draw_line(canvas, small_font, 5, 41 + (int32_t)i * 10, line)) {
            return false;
        }
    }
    return draw(canvas, small_font, 80, 121, "PARA page   EXIT back", GP100_TEXT_ALIGN_CENTER,
                GP100_UI_MUTED);
}
