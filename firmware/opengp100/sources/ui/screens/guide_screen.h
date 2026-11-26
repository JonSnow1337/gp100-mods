#ifndef GP100_GUIDE_SCREEN_H
#define GP100_GUIDE_SCREEN_H

#include "ui/render/display_text.h"
#include "ui/ui_model.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_GUIDE_PAGE_COUNT 7u
#define GP100_GUIDE_MAX_LINES 8u

size_t gp100_guide_page_for_mode(gp100_ui_mode_t mode, bool picker_open);
const char *gp100_guide_page_title(size_t page);
/* Line `index` of a page, or NULL past its last line. */
const char *gp100_guide_page_line(size_t page, size_t index);

bool gp100_guide_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font,
    size_t page);

#ifdef __cplusplus
}
#endif

#endif
