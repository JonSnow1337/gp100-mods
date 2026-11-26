#include "drum_screen.h"

#include "ui/ui_palette.h"

#include "play_screen.h"
#include "rhythm/rhythm_player.h"
#include "protocol/midi/sysex/vendor_sysex_performance.h"

#include <stddef.h>

static const char *const genre_names[GP100_DRUM_GENRE_COUNT] = {
    "Electronic", "Rock", "Funk", "Pop",
    "Blues", "World", "Jazz", "Metro"
};

/* The product manual's 100 rhythm entries, in the factory absolute order. */
static const char *const style_names[GP100_DRUM_FACTORY_STYLE_COUNT] = {
    "D&B", "Electr01", "Electr02", "Techno", "TripHop", "E-POP",
    "Break", "H-Hop1", "H-Hop2", "H-Hop3", "H-Hop4",
    "Prog", "Rock 1", "Rock 2", "Rock 3", "Surfin", "Shuffle",
    "R'n'R", "Ballad", "SF3/4", "Rock5/4", "Classic", "SF4/4",
    "Garag", "Hard 1", "Hard 2", "Nu 1", "Nu 2", "Metal 1",
    "Metal 2", "Punk 1", "Punk 2", "Punk 3", "Punk 4", "Punk 5",
    "P Punk 1", "P Punk 2", "EMO", "Core", "Nwave", "P Rock 1",
    "P Rock 2", "P Rock 3", "Hard 3",
    "Funk 1", "Funk 2", "Funk 3", "Funk 4",
    "Pub", "Pop 1", "Pop 2", "Pop 3",
    "Blues 1", "Blues 2", "Blues 3", "B-grass", "Country", "Folk",
    "Blues 4",
    "Latin 1", "Latin 2", "Latin 3", "Pop 1", "Pop 2", "Bossa1",
    "Bossa2", "Beguine", "Mazuke", "Samba", "Army", "March 1",
    "March 2", "Musette", "NuAge1", "NuAge2", "Polka", "Tango",
    "Ska", "Waltz", "RAG1", "RAG2", "World",
    "Jazz 1", "Jazz 2", "Jazz 3", "Jazz 4", "Funk 1", "Funk2",
    "Funk3", "Fusion",
    "1/4", "2/4", "3/4", "4/4", "5/4", "6/4", "7/4", "6/8",
    "7/8", "9/8"
};

static bool get_setting(const gp100_settings_record_t *settings,
                        gp100_settings_field_t field,
                        uint16_t *value)
{
    int16_t signed_value;

    if (!gp100_settings_get_i16(settings, field, &signed_value) ||
        signed_value < 0) {
        return false;
    }
    *value = (uint16_t)signed_value;
    return true;
}

static bool snapshot_is_valid(
    const gp100_drum_screen_snapshot_t *snapshot)
{
    uint16_t actual_genre;
    uint16_t actual_style_in_genre;

    return snapshot != NULL &&
           snapshot->genre < GP100_DRUM_GENRE_COUNT &&
           snapshot->absolute_style < GP100_DRUM_FACTORY_STYLE_COUNT &&
           snapshot->tempo_bpm >= GP100_RHYTHM_MIN_TEMPO_BPM &&
           snapshot->tempo_bpm <= GP100_RHYTHM_MAX_TEMPO_BPM &&
           snapshot->volume <= 99u &&
           gp100_rhythm_style_location(
               snapshot->absolute_style, &actual_genre,
               &actual_style_in_genre) &&
           snapshot->genre == actual_genre &&
           snapshot->style_in_genre == actual_style_in_genre;
}

bool gp100_drum_screen_snapshot_from_settings(
    gp100_drum_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    bool enabled)
{
    if (snapshot == NULL || settings == NULL ||
        !get_setting(settings, GP100_SETTINGS_DRUM_GENRE,
                     &snapshot->genre) ||
        !get_setting(settings, GP100_SETTINGS_DRUM_STYLE,
                     &snapshot->absolute_style) ||
        !get_setting(settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE,
                     &snapshot->style_in_genre) ||
        !get_setting(settings, GP100_SETTINGS_DRUM_TEMPO,
                     &snapshot->tempo_bpm) ||
        !get_setting(settings, GP100_SETTINGS_DRUM_VOLUME,
                     &snapshot->volume)) {
        return false;
    }
    snapshot->enabled = enabled;
    return snapshot_is_valid(snapshot);
}

static bool read_nonnegative_setting(
    const gp100_settings_record_t *settings,
    gp100_settings_field_t field,
    uint16_t *value)
{
    return get_setting(settings, field, value);
}

static bool write_setting(gp100_settings_record_t *settings,
                          gp100_settings_field_t field,
                          uint16_t value)
{
    return gp100_settings_set_i16(settings, field, (int16_t)value);
}

static uint16_t clamp_step(uint16_t value,
                           uint16_t minimum,
                           uint16_t maximum,
                           bool increase)
{
    if (increase) {
        return value < minimum ? minimum
             : value < maximum ? (uint16_t)(value + 1u) : maximum;
    }
    return value > maximum ? maximum
         : value > minimum ? (uint16_t)(value - 1u) : minimum;
}

bool gp100_drum_adjust_setting(
    gp100_settings_record_t *settings,
    gp100_drum_control_t control,
    bool increase)
{
    uint16_t genre;
    uint16_t first_style;
    uint16_t next_first_style;
    uint16_t value;
    uint16_t maximum;
    bool updated;

    if (settings == NULL ||
        (unsigned)control > (unsigned)GP100_DRUM_CONTROL_GENRE) {
        return false;
    }
    switch (control) {
    case GP100_DRUM_CONTROL_STYLE:
        if (!read_nonnegative_setting(
                settings, GP100_SETTINGS_DRUM_GENRE, &genre) ||
            !read_nonnegative_setting(
                settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, &value) ||
            !gp100_rhythm_genre_first_style(genre, &first_style)) {
            return false;
        }
        if (genre + 1u < GP100_DRUM_GENRE_COUNT) {
            if (!gp100_rhythm_genre_first_style(
                    (uint16_t)(genre + 1u), &next_first_style)) {
                return false;
            }
            maximum = (uint16_t)(next_first_style - first_style - 1u);
        } else {
            maximum = (uint16_t)(
                GP100_DRUM_FACTORY_STYLE_COUNT - first_style - 1u);
        }
        value = clamp_step(value, 0u, maximum, increase);
        updated = write_setting(
                      settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE,
                      value) &&
                  write_setting(
                      settings, GP100_SETTINGS_DRUM_STYLE,
                      (uint16_t)(first_style + value));
        break;
    case GP100_DRUM_CONTROL_TEMPO:
        if (!read_nonnegative_setting(
                settings, GP100_SETTINGS_DRUM_TEMPO, &value)) {
            return false;
        }
        value = clamp_step(value, GP100_RHYTHM_MIN_TEMPO_BPM,
                           GP100_RHYTHM_MAX_TEMPO_BPM, increase);
        updated = write_setting(
            settings, GP100_SETTINGS_DRUM_TEMPO, value);
        break;
    case GP100_DRUM_CONTROL_VOLUME:
        if (!read_nonnegative_setting(
                settings, GP100_SETTINGS_DRUM_VOLUME, &value)) {
            return false;
        }
        value = clamp_step(value, 0u, 99u, increase);
        updated = write_setting(
            settings, GP100_SETTINGS_DRUM_VOLUME, value);
        break;
    case GP100_DRUM_CONTROL_GENRE:
        if (!read_nonnegative_setting(
                settings, GP100_SETTINGS_DRUM_GENRE, &genre)) {
            return false;
        }
        genre = clamp_step(
            genre, 0u, GP100_DRUM_GENRE_COUNT - 1u, increase);
        if (!gp100_rhythm_genre_first_style(genre, &first_style)) {
            return false;
        }
        updated = write_setting(
                      settings, GP100_SETTINGS_DRUM_GENRE, genre) &&
                  write_setting(
                      settings, GP100_SETTINGS_DRUM_STYLE,
                      first_style) &&
                  write_setting(
                      settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE,
                      0u);
        break;
    default:
        return false;
    }
    if (updated) {
        gp100_settings_update_crc(settings);
    }
    return updated;
}

bool gp100_drum_toggle_playback(bool *enabled)
{
    if (enabled == NULL) {
        return false;
    }
    *enabled = !*enabled;
    return true;
}

gp100_drum_event_result_t gp100_drum_handle_event(
    gp100_settings_record_t *settings,
    gp100_preset_record_t *preset,
    uint16_t *logical_preset,
    bool *enabled,
    bool *drum_release_guard,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_ui_mode_t target;

    if (settings == NULL || preset == NULL || logical_preset == NULL ||
        enabled == NULL || drum_release_guard == NULL || event == NULL ||
        next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_DRUM_EVENT_INVALID;
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER) {
        if (decoded.control > GP100_ENCODER_PARA ||
            !gp100_drum_adjust_setting(
                settings, (gp100_drum_control_t)decoded.control,
                decoded.action == GP100_BUTTON_PRESS)) {
            return GP100_DRUM_EVENT_INVALID;
        }
        return GP100_DRUM_EVENT_SETTINGS_CHANGED;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON ||
        (decoded.action != GP100_BUTTON_RELEASE &&
         decoded.action != GP100_BUTTON_REPEAT)) {
        return GP100_DRUM_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_FOOTSWITCH_MINUS ||
        decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS) {
        gp100_play_action_result_t action;
        int16_t mode;

        if (decoded.action == GP100_BUTTON_REPEAT &&
            (!gp100_settings_get_i16(
                 settings, GP100_SETTINGS_FOOTSWITCH_MODE, &mode) ||
             mode != GP100_FOOTSWITCH_MODE_PATCH)) {
            return GP100_DRUM_EVENT_UNHANDLED;
        }

        action = gp100_play_footswitch(
            settings, preset, logical_preset,
            decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS ? 1u : 0u);
        if (action == GP100_PLAY_ACTION_SELECT_PRESET) {
            return GP100_DRUM_EVENT_PRESET_SELECTED;
        }
        if (action == GP100_PLAY_ACTION_PRESET_CHANGED) {
            return GP100_DRUM_EVENT_PRESET_CHANGED;
        }
        if (action == GP100_PLAY_ACTION_UNCHANGED) {
            return GP100_DRUM_EVENT_UNHANDLED;
        }
        return GP100_DRUM_EVENT_INVALID;
    }
    if (decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_DRUM_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_DRUM) {
        if (*drum_release_guard) {
            *drum_release_guard = false;
            return GP100_DRUM_EVENT_RELEASE_GUARDED;
        }
        (void)gp100_drum_toggle_playback(enabled);
        return GP100_DRUM_EVENT_PLAYBACK_TOGGLED;
    }
    if (decoded.control == GP100_BUTTON_PARA) {
        (void)gp100_drum_toggle_playback(enabled);
        return GP100_DRUM_EVENT_PLAYBACK_TOGGLED;
    }
    switch ((gp100_button_id_t)decoded.control) {
    case GP100_BUTTON_EXIT:
        target = GP100_UI_MODE_PLAY;
        break;
    case GP100_BUTTON_GLOBAL:
        target = GP100_UI_MODE_GLOBAL;
        break;
    case GP100_BUTTON_EDIT:
        target = GP100_UI_MODE_EDIT;
        break;
    case GP100_BUTTON_SAVE:
        target = GP100_UI_MODE_SAVE;
        break;
    default:
        return GP100_DRUM_EVENT_UNHANDLED;
    }
    *next_mode = target;
    return GP100_DRUM_EVENT_MODE_CHANGED;
}

const char *gp100_drum_genre_name(uint16_t genre)
{
    return genre < GP100_DRUM_GENRE_COUNT ? genre_names[genre] : NULL;
}

const char *gp100_drum_style_name(uint16_t absolute_style)
{
    return absolute_style < GP100_DRUM_FACTORY_STYLE_COUNT
               ? style_names[absolute_style]
               : NULL;
}

bool gp100_drum_genre_window(uint16_t genre,
                                  uint16_t *first_genre,
                                  uint16_t *selected_row)
{
    if (genre >= GP100_DRUM_GENRE_COUNT || first_genre == NULL ||
        selected_row == NULL) {
        return false;
    }
    if (genre < 4u) {
        *first_genre = genre;
        *selected_row = 0u;
    } else {
        *first_genre = 4u;
        *selected_row = (uint16_t)(genre - 4u);
    }
    return true;
}

bool gp100_drum_screen_default_theme(
    gp100_drum_screen_theme_t *theme,
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
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->control_rgb565 = GP100_UI_CONTROL;
    theme->accent_rgb565 = GP100_UI_ACCENT;
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

static void format_unsigned(char buffer[4], uint16_t value)
{
    size_t index = 0u;

    if (value >= 100u) {
        buffer[index++] = (char)('0' + value / 100u);
        value %= 100u;
        buffer[index++] = (char)('0' + value / 10u);
    } else if (value >= 10u) {
        buffer[index++] = (char)('0' + value / 10u);
    }
    buffer[index++] = (char)('0' + value % 10u);
    buffer[index] = '\0';
}

/* GP100_UI_ASSET_REPLACEMENT: rhythm pictogram. */
static void draw_drum_icon(gp100_display_canvas_t *canvas,
                           uint16_t color, uint16_t panel)
{
    (void)gp100_display_fill_rect(canvas, 14, 30, 60u, 43u, panel);
    (void)gp100_display_draw_line(canvas, 20, 64, 67, 64, color);
    (void)gp100_display_fill_rect(canvas, 23, 41, 6u, 19u, color);
    (void)gp100_display_fill_rect(canvas, 35, 49, 6u, 11u, color);
    (void)gp100_display_fill_rect(canvas, 47, 44, 6u, 16u, color);
    (void)gp100_display_fill_rect(canvas, 59, 49, 6u, 11u, color);
}

static bool draw_genres(gp100_display_canvas_t *canvas,
                        const gp100_drum_screen_snapshot_t *snapshot,
                        const gp100_drum_screen_theme_t *theme)
{
    uint16_t first;
    uint16_t selected;
    uint16_t row;

    if (!gp100_drum_genre_window(snapshot->genre, &first, &selected)) {
        return false;
    }
    (void)gp100_display_fill_rect(canvas, 87, 14, 69u, 78u,
                                       theme->panel_rgb565);
    (void)gp100_display_fill_rect(canvas, 89, 29, 65u, 1u,
                                       theme->accent_rgb565);
    if (!draw_text(canvas, theme->regular_font, 121, 17, "Genre",
                   GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                   theme->panel_rgb565)) {
        return false;
    }
    for (row = 0u; row < 4u; ++row) {
        uint16_t genre = (uint16_t)(first + row);
        uint16_t y = (uint16_t)(30u + row * 15u);
        uint16_t foreground = theme->foreground_rgb565;
        uint16_t background = theme->panel_rgb565;

        if (row == selected) {
            foreground = theme->background_rgb565;
            background = theme->accent_rgb565;
            (void)gp100_display_fill_rect(canvas, 90, y, 62u, 15u,
                                               background);
        }
        if (!draw_text(canvas, theme->regular_font, 92, (int32_t)y + 2,
                       gp100_drum_genre_name(genre),
                       GP100_TEXT_ALIGN_LEFT, foreground, background)) {
            return false;
        }
    }
    return true;
}

static bool draw_bottom(gp100_display_canvas_t *canvas,
                        const gp100_drum_screen_snapshot_t *snapshot,
                        const gp100_drum_screen_theme_t *theme)
{
    char tempo[4];
    char volume[4];
    uint16_t tempo_width = (uint16_t)(
        ((uint32_t)(snapshot->tempo_bpm - GP100_RHYTHM_MIN_TEMPO_BPM) *
         42u) /
        (GP100_RHYTHM_MAX_TEMPO_BPM -
         GP100_RHYTHM_MIN_TEMPO_BPM));
    uint16_t volume_width = (uint16_t)((snapshot->volume * 42u) / 99u);

    format_unsigned(tempo, snapshot->tempo_bpm);
    format_unsigned(volume, snapshot->volume);
    (void)gp100_display_fill_rect(canvas, 0, 94, 160u, 34u,
                                       theme->panel_rgb565);
    (void)gp100_display_fill_rect(canvas, 2, 112, 54u, 14u,
                                       theme->control_rgb565);
    (void)gp100_display_fill_rect(canvas, 62, 110, 42u, 4u,
                                       theme->track_rgb565);
    (void)gp100_display_fill_rect(canvas, 62, 110, tempo_width, 4u,
                                       theme->accent_rgb565);
    (void)gp100_display_fill_rect(canvas, 114, 110, 42u, 4u,
                                       theme->track_rgb565);
    (void)gp100_display_fill_rect(canvas, 114, 110, volume_width, 4u,
                                       theme->accent_rgb565);

    return draw_text(canvas, theme->small_font, 29, 97, "Style",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565) &&
           draw_text(canvas, theme->small_font, 81, 97, "BPM",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565) &&
           draw_text(canvas, theme->small_font, 134, 97, "VOL",
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565) &&
           draw_text(canvas, theme->regular_font, 29, 115,
                     gp100_drum_style_name(snapshot->absolute_style),
                     GP100_TEXT_ALIGN_CENTER, theme->accent_rgb565,
                     theme->control_rgb565) &&
           draw_text(canvas, theme->regular_font, 81, 115, tempo,
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565) &&
           draw_text(canvas, theme->regular_font, 134, 115, volume,
                     GP100_TEXT_ALIGN_CENTER, theme->foreground_rgb565,
                     theme->panel_rgb565);
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_drum_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_drum_screen_snapshot_t *snapshot,
    const gp100_drum_screen_theme_t *theme)
{
    const char *enabled_text;
    uint16_t enabled_color;

    if (canvas == NULL || theme == NULL || !snapshot_is_valid(snapshot) ||
        theme->regular_font == NULL || theme->small_font == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT) {
        return false;
    }
    enabled_text = snapshot->enabled ? "ON" : "OFF";
    enabled_color = snapshot->enabled ? theme->accent_rgb565
                                      : theme->foreground_rgb565;
    (void)gp100_display_fill_rect(canvas, 0, 0, 160u, 128u,
                                       theme->background_rgb565);
    draw_drum_icon(canvas, enabled_color, theme->panel_rgb565);
    return draw_text(canvas, theme->regular_font, 4, 4, "DRUM",
                     GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                     theme->background_rgb565) &&
           draw_text(canvas, theme->regular_font, 44, 79, enabled_text,
                     GP100_TEXT_ALIGN_CENTER, enabled_color,
                     theme->background_rgb565) &&
           draw_genres(canvas, snapshot, theme) &&
           draw_bottom(canvas, snapshot, theme);
}
