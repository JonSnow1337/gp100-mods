#include "effect_art.h"

#include "ui/ui_palette.h"

typedef struct effect_art {
    gp100_display_canvas_t *canvas;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    uint16_t paint;
    uint16_t edge;
    uint16_t ink;
    uint16_t highlight;
} effect_art_t;

static uint16_t mix_color(uint16_t a, uint16_t b, unsigned part)
{
    const unsigned rest = 4u - part;
    const unsigned r = (((a >> 11) * rest + (b >> 11) * part) / 4u) << 11;
    const unsigned g = ((((a >> 5) & 63u) * rest + ((b >> 5) & 63u) * part) / 4u) << 5;
    const unsigned bl = ((a & 31u) * rest + (b & 31u) * part) / 4u;
    return (uint16_t)(r | g | bl);
}

static void rect(const effect_art_t *art, int x, int y, int w, int h, uint16_t color)
{
    const int32_t left = x * art->width / (int32_t)GP100_EFFECT_ART_WIDTH;
    const int32_t top = y * art->height / (int32_t)GP100_EFFECT_ART_HEIGHT;
    const int32_t right = (x + w) * art->width / (int32_t)GP100_EFFECT_ART_WIDTH;
    const int32_t bottom = (y + h) * art->height / (int32_t)GP100_EFFECT_ART_HEIGHT;
    (void)gp100_display_fill_rect(art->canvas, art->x + left, art->y + top,
                                  (uint16_t)(right > left ? right - left : 1),
                                  (uint16_t)(bottom > top ? bottom - top : 1),
                                  color);
}

static void line(const effect_art_t *art, int x1, int y1, int x2, int y2,
                 uint16_t color)
{
    (void)gp100_display_draw_line(
        art->canvas,
        art->x + x1 * art->width / (int32_t)GP100_EFFECT_ART_WIDTH,
        art->y + y1 * art->height / (int32_t)GP100_EFFECT_ART_HEIGHT,
        art->x + x2 * art->width / (int32_t)GP100_EFFECT_ART_WIDTH,
        art->y + y2 * art->height / (int32_t)GP100_EFFECT_ART_HEIGHT, color);
}

static void dial(const effect_art_t *art, int x, int y)
{
    rect(art, x - 1, y - 1, 3, 3, art->ink);
    rect(art, x, y - 1, 1, 1, art->highlight);
}

static void speaker(const effect_art_t *art, int x, int y, int diameter)
{
    const int half = diameter / 2;
    rect(art, x - half, y - half + 1, diameter, diameter - 2, art->ink);
    rect(art, x - half + 1, y - half, diameter - 2, diameter, art->ink);
    rect(art, x - 1, y - 1, 3, 3, art->edge);
    rect(art, x, y, 1, 1, art->highlight);
}

static void box(const effect_art_t *art, int x, int y, int w, int h)
{
    rect(art, x + 1, y, w - 2, h, art->edge);
    rect(art, x, y + 1, w, h - 2, art->edge);
    rect(art, x + 1, y + 1, w - 2, h - 3, art->paint);
    line(art, x + 2, y + 1, x + w - 3, y + 1, art->highlight);
}

static void stompbox(const effect_art_t *art, unsigned model, bool compact)
{
    const int x = compact ? 5 : 3;
    const int w = compact ? 17 : 21;
    box(art, x, 1, w, 23);
    dial(art, x + 4, 5);
    dial(art, 13, 5);
    if ((model & 1u) == 0u) {
        dial(art, x + w - 5, 5);
    } else {
        rect(art, x + w - 6, 4, 3, 2, art->highlight);
    }
    rect(art, 12, 18, 3, 3, art->ink);
    rect(art, 13, 18, 1, 1, art->highlight);
}

static void wave(const effect_art_t *art, int x, int y, bool upper)
{
    line(art, x, y, x + 3, y - 3, art->ink);
    line(art, x + 3, y - 3, x + 8, y + 2, art->ink);
    line(art, x + 8, y + 2, x + 12, y - 2, art->ink);
    if (upper) {
        line(art, x + 1, y + 2, x + 4, y - 1, art->highlight);
        line(art, x + 4, y - 1, x + 8, y + 3, art->highlight);
    }
}

static void pre_fx(const effect_art_t *art, unsigned model)
{
    if (model >= 5u && model <= 8u) {
        box(art, 6, 1, 15, 23);
        rect(art, 9, 6, 9, 14, art->ink);
        for (int y = 8; y < 19; y += 3) {
            line(art, 10, y, 16, y - 1, art->highlight);
        }
        rect(art, 12, 21, 3, 1, art->highlight);
        return;
    }
    if (model == 2u || model == 3u) {
        box(art, 6, 2, 15, 22);
        speaker(art, 13, 8, 7);
        line(art, 10, 15, 16, 15, art->ink);
        line(art, 13, 12, 13, 18, art->ink);
        rect(art, 12, 20, 3, 2, art->highlight);
        return;
    }

    stompbox(art, model, model != 1u && model != 13u);
    if (model < 2u) {
        rect(art, 7, 10, 13, 5, art->ink);
        for (int x = 9; x < 19; x += 2) {
            rect(art, x, 11, 1, x > 13 ? 2 : 3, art->highlight);
        }
    } else if (model >= 9u && model <= 12u) {
        line(art, 8, 15, 17, 9, art->ink);
        line(art, 17, 9, 13, 9, art->ink);
        line(art, 17, 9, 17, 13, art->ink);
        rect(art, 8, 10, 2, 2, art->highlight);
    } else {
        wave(art, 7, 13, (model & 1u) != 0u);
    }
}

static void amp(const effect_art_t *art, unsigned model)
{
    box(art, 0, 5, 27, 16);
    rect(art, 2, 7, 23, 5, art->ink);
    for (int x = 5; x < 25; x += 4) {
        rect(art, x, 9, 2, 1, art->highlight);
    }
    const int badge = 4 + (int)(model % 13u);
    line(art, badge, 15, badge + 6, 15, art->ink);
    rect(art, 2, 21, 4, 1, art->edge);
    rect(art, 21, 21, 4, 1, art->edge);
}

static void cab(const effect_art_t *art, unsigned model)
{
    box(art, 1, 1, 25, 23);
    if ((model >= 7u && model <= 21u) || (model >= 31u && model <= 35u && model != 32u)) {
        speaker(art, 8, 7, 7);
        speaker(art, 18, 7, 7);
        speaker(art, 8, 17, 7);
        speaker(art, 18, 17, 7);
    } else if (model <= 6u || model == 32u) {
        speaker(art, 7, 12, 9);
        speaker(art, 19, 12, 9);
    } else {
        speaker(art, 13, 12, 13);
    }
    rect(art, 3 + (int)(model % 16u), 21, 3, 1, art->highlight);
}

static void pedals(const effect_art_t *art, gp100_effect_family_t family,
                   unsigned model)
{
    switch (family) {
    case GP100_EFFECT_DISTORTION:
        if (model == 11u) {
            box(art, 1, 5, 25, 17);
            dial(art, 7, 9);
            dial(art, 19, 9);
            line(art, 7, 16, 10, 16, art->ink);
            line(art, 10, 16, 10, 13, art->ink);
            line(art, 10, 13, 15, 13, art->ink);
            line(art, 15, 13, 15, 16, art->ink);
            line(art, 15, 16, 19, 16, art->ink);
            rect(art, 12, 18, 3, 2, art->highlight);
            break;
        }
        stompbox(art, model, model == 17u || model == 18u);
        line(art, 7, 14, 11, 10, art->ink);
        line(art, 11, 10, 15, 14, art->ink);
        line(art, 15, 14, 19, 10 + (int)(model % 3u), art->ink);
        rect(art, 7 + (int)(model % 11u), 16, 3, 1, art->highlight);
        break;
    case GP100_EFFECT_NOISE_REDUCTION:
        box(art, 7, 2, 13, 22);
        dial(art, 13, 6);
        rect(art, 10, 10, 2, 7, art->ink);
        rect(art, 15, 10, 2, 7, art->ink);
        line(art, 11, 10 + (int)model * 4, 16, 10 + (int)model * 4, art->highlight);
        dial(art, 13, 20);
        break;
    case GP100_EFFECT_EQUALIZER:
        box(art, 1, 3, 25, 19);
        for (int i = 0; i < 4; ++i) {
            const int x = 5 + 4 * i;
            const int y = 7 + (int)((model + (unsigned)i * 3u) % 8u);
            rect(art, x, 6, 1, 11, art->ink);
            rect(art, x - 1, y, 3, 2, art->highlight);
        }
        rect(art, 21, 11, 2, 3, art->ink);
        break;
    case GP100_EFFECT_MODULATION:
        stompbox(art, model, model < 4u);
        if (model == 6u) {
            speaker(art, 13, 12, 9);
        } else {
            wave(art, 7, 13, model < 5u);
        }
        if (model >= 8u) {
            rect(art, 8, 16, 3 + (int)(model - 8u), 1, art->ink);
        }
        break;
    case GP100_EFFECT_DELAY:
        stompbox(art, model, false);
        if (model == 2u || model == 3u || model == 7u) {
            speaker(art, 8, 12, 5);
            speaker(art, 18, 12, 5);
            line(art, 8, 15, 18, 15, art->ink);
        } else {
            rect(art, 7, 10, 13, 6, art->ink);
            for (int i = 0; i < 3; ++i) {
                rect(art, 9 + i * 3, 11 + (int)((model + (unsigned)i) % 3u),
                     1, 2, art->highlight);
            }
        }
        break;
    case GP100_EFFECT_REVERB:
        stompbox(art, model, false);
        if (model == 4u) {
            for (int i = 0; i < 4; ++i) {
                const int x = 7 + i * 3;
                line(art, x, 15, x + 2, 10, art->ink);
                line(art, x + 2, 10, x + 3, 15, art->ink);
            }
            break;
        }
        line(art, 7, 15, 10, 11, art->ink);
        line(art, 10, 11, 16, 11, art->ink);
        line(art, 16, 11, 19, 15, art->ink);
        line(art, 10, 15, 12, 13, art->highlight);
        line(art, 14, 13, 16 + (int)(model % 3u), 15, art->highlight);
        break;
    default:
        break;
    }
}

static void draw_enclosure(const effect_art_t *art, gp100_effect_family_t family,
                           uint16_t model)
{
    switch (family) {
    case GP100_EFFECT_PRE_FX: pre_fx(art, model); break;
    case GP100_EFFECT_AMPLIFIER: amp(art, model); break;
    case GP100_EFFECT_CABINET: cab(art, model); break;
    default: pedals(art, family, model); break;
    }
}

void gp100_effect_art_draw(gp100_display_canvas_t *canvas, int32_t x, int32_t y,
                           uint16_t width, uint16_t height,
                           gp100_effect_family_t family, uint16_t model,
                           uint16_t accent, uint16_t background, bool enabled)
{
    const uint16_t dark = GP100_UI_RGB565(16, 20, 25);
    const uint16_t light = GP100_UI_RGB565(252, 246, 228);
    /* Distinguish the named drive voicings and fuzz within the same family. */
    if (family == GP100_EFFECT_DISTORTION) {
        switch (model) {
        case 6u: accent = GP100_UI_RGB565(120, 202, 134); break;
        case 7u: accent = GP100_UI_RGB565(240, 207, 80); break;
        case 9u: accent = GP100_UI_RGB565(98, 167, 240); break;
        case 11u: accent = GP100_UI_RGB565(218, 155, 230); break;
        default: break;
        }
    }
    const effect_art_t art = {
        .canvas = canvas, .x = x, .y = y,
        .width = width, .height = height,
        .paint = enabled ? mix_color(accent, (model & 2u) ? light : dark,
                                      1u + (unsigned)(model & 1u))
                         : mix_color(accent, background, 3u),
        .edge = mix_color(accent, background, enabled ? 2u : 3u),
        .ink = dark,
        .highlight = enabled ? light : mix_color(light, background, 3u)
    };

    draw_enclosure(&art, family, model);
}

void gp100_effect_art_draw_silhouette(gp100_display_canvas_t *canvas,
    int32_t x, int32_t y, gp100_effect_family_t family, uint16_t model, uint16_t color)
{
    const effect_art_t art = {
        .canvas = canvas, .x = x, .y = y,
        .width = GP100_EFFECT_ART_WIDTH, .height = GP100_EFFECT_ART_HEIGHT,
        .paint = color, .edge = color, .ink = color, .highlight = color
    };
    draw_enclosure(&art, family, model);
}
