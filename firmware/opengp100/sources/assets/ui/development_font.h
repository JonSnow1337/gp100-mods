#ifndef GP100_DEVELOPMENT_FONT_H
#define GP100_DEVELOPMENT_FONT_H

#include "ui/render/display_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Repository-owned ASCII text and heading sizes for the standalone UI. */
const gp100_bitmap_font_t *gp100_development_font_5x7(void);
const gp100_bitmap_font_t *gp100_development_font_10x14(void);

#ifdef __cplusplus
}
#endif

#endif
