#include "vendor_sysex_performance.h"

static const uint16_t rhythm_genre_first_styles[
    GP100_RHYTHM_GENRE_COUNT] = {
    0u, 11u, 44u, 48u, 52u, 59u, 82u, 90u
};

static bool address_kind(uint32_t address,
                         gp100_sysex_performance_kind_t *kind)
{
    switch (address) {
    case GP100_SYSEX_LOOP_RECORD_VOLUME:
        *kind = GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME;
        return true;
    case GP100_SYSEX_LOOP_PLAYBACK_VOLUME:
        *kind = GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME;
        return true;
    case GP100_SYSEX_RHYTHM_GENRE:
        *kind = GP100_SYSEX_PERFORMANCE_RHYTHM_GENRE;
        return true;
    case GP100_SYSEX_RHYTHM_STYLE:
        *kind = GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE;
        return true;
    case GP100_SYSEX_RHYTHM_TEMPO:
        *kind = GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO;
        return true;
    case GP100_SYSEX_RHYTHM_VOLUME:
        *kind = GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME;
        return true;
    default:
        return false;
    }
}

bool gp100_rhythm_style_location(
    uint16_t absolute_style,
    uint16_t *genre,
    uint16_t *style_in_genre)
{
    size_t index;

    if (absolute_style >= GP100_RHYTHM_STYLE_COUNT ||
        genre == NULL || style_in_genre == NULL) {
        return false;
    }
    for (index = GP100_RHYTHM_GENRE_COUNT; index != 0u; --index) {
        size_t candidate = index - 1u;
        if (absolute_style >= rhythm_genre_first_styles[candidate]) {
            *genre = (uint16_t)candidate;
            *style_in_genre = (uint16_t)(
                absolute_style - rhythm_genre_first_styles[candidate]);
            return true;
        }
    }
    return false;
}

bool gp100_rhythm_genre_first_style(
    uint16_t genre,
    uint16_t *absolute_style)
{
    if (genre >= GP100_RHYTHM_GENRE_COUNT ||
        absolute_style == NULL) {
        return false;
    }
    *absolute_style = rhythm_genre_first_styles[genre];
    return true;
}

static bool value_is_valid(gp100_sysex_performance_kind_t kind,
                           uint16_t value)
{
    switch (kind) {
    case GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME:
    case GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME:
    case GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME:
        return value <= 100u;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_GENRE:
        return value < GP100_RHYTHM_GENRE_COUNT;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE:
        return value < GP100_RHYTHM_STYLE_COUNT;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO:
        return value >= GP100_RHYTHM_MIN_TEMPO_BPM &&
               value <= GP100_RHYTHM_MAX_TEMPO_BPM;
    default:
        return false;
    }
}

gp100_sysex_performance_result_t
gp100_sysex_performance_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_performance_action_t *action)
{
    if (request == NULL || action == NULL) {
        return GP100_SYSEX_PERFORMANCE_INVALID;
    }
    if (request->message_class != GP100_SYSEX_RESPONSE_CLASS ||
        !address_kind(request->address, &action->kind)) {
        return GP100_SYSEX_PERFORMANCE_UNHANDLED;
    }
    if (request->payload == NULL || request->payload_length != 4u ||
        !gp100_sysex_decode_u16(request->payload, &action->value) ||
        !value_is_valid(action->kind, action->value)) {
        return GP100_SYSEX_PERFORMANCE_INVALID;
    }
    return GP100_SYSEX_PERFORMANCE_ACTION;
}

static bool set_value(gp100_settings_record_t *settings,
                      gp100_settings_field_t field,
                      uint16_t value)
{
    return gp100_settings_set_i16(settings, field, (int16_t)value);
}

bool gp100_sysex_performance_apply(
    gp100_settings_record_t *settings,
    const gp100_sysex_performance_action_t *action)
{
    uint16_t genre;
    uint16_t style;
    uint16_t style_in_genre;

    if (settings == NULL || action == NULL ||
        !value_is_valid(action->kind, action->value)) {
        return false;
    }
    switch (action->kind) {
    case GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME:
        return set_value(settings,
                         GP100_SETTINGS_LOOP_RECORD_VOLUME,
                         action->value);
    case GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME:
        return set_value(settings,
                         GP100_SETTINGS_LOOP_PLAYBACK_VOLUME,
                         action->value);
    case GP100_SYSEX_PERFORMANCE_RHYTHM_GENRE:
        genre = action->value;
        if (!gp100_rhythm_genre_first_style(genre, &style)) {
            return false;
        }
        return set_value(settings, GP100_SETTINGS_DRUM_GENRE, genre) &&
               set_value(settings, GP100_SETTINGS_DRUM_STYLE, style) &&
               set_value(settings,
                         GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0u);
    case GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE:
        style = action->value;
        if (!gp100_rhythm_style_location(
                style, &genre, &style_in_genre)) {
            return false;
        }
        return set_value(settings, GP100_SETTINGS_DRUM_STYLE, style) &&
               set_value(settings, GP100_SETTINGS_DRUM_GENRE, genre) &&
               set_value(settings,
                         GP100_SETTINGS_DRUM_STYLE_IN_GENRE,
                         style_in_genre);
    case GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO:
        return set_value(settings,
                         GP100_SETTINGS_DRUM_TEMPO,
                         action->value);
    case GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME:
        return set_value(settings,
                         GP100_SETTINGS_DRUM_VOLUME,
                         action->value);
    default:
        return false;
    }
}

static bool rhythm_equal(const gp100_rhythm_settings_t *left,
                         const gp100_rhythm_settings_t *right)
{
    return left->pattern_index == right->pattern_index &&
           left->enabled == right->enabled &&
           left->tempo_bpm == right->tempo_bpm &&
           left->target_gain_q8 == right->target_gain_q8;
}

bool gp100_sysex_performance_apply_live(
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_rhythm_settings_t *rhythm,
    bool rhythm_enabled,
    const gp100_sysex_performance_action_t *action,
    gp100_sysex_performance_live_result_t *result)
{
    gp100_settings_record_t next_settings;
    gp100_looper_engine_t next_looper;
    gp100_rhythm_settings_t next_rhythm;
    gp100_sysex_performance_live_result_t next_result = {false, false};

    if (settings == NULL || looper == NULL || rhythm == NULL ||
        action == NULL || result == NULL) {
        return false;
    }
    next_settings = *settings;
    next_looper = *looper;
    next_rhythm = *rhythm;
    if (!gp100_sysex_performance_apply(&next_settings, action)) {
        return false;
    }
    if (action->kind == GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME ||
        action->kind == GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME) {
        uint8_t record_level = next_looper.record_level_target;
        uint8_t playback_level = next_looper.playback_level_target;
        uint8_t live_level = (uint8_t)action->value;

        if (action->kind ==
                GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME) {
            record_level = live_level;
        } else {
            playback_level = live_level;
        }
        next_result.looper_changed =
            record_level != looper->record_level_target ||
            playback_level != looper->playback_level_target;
        if (!gp100_looper_set_levels(
                &next_looper, record_level, playback_level)) {
            return false;
        }
    } else {
        if (!gp100_rhythm_settings_from_record(
                &next_settings, rhythm_enabled, &next_rhythm)) {
            return false;
        }
        next_result.rhythm_changed = !rhythm_equal(rhythm, &next_rhythm);
    }
    gp100_settings_update_crc(&next_settings);
    *settings = next_settings;
    *looper = next_looper;
    *rhythm = next_rhythm;
    *result = next_result;
    return true;
}

static bool kind_field(gp100_sysex_performance_kind_t kind,
                       gp100_settings_field_t *field)
{
    switch (kind) {
    case GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME:
        *field = GP100_SETTINGS_LOOP_RECORD_VOLUME;
        return true;
    case GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME:
        *field = GP100_SETTINGS_LOOP_PLAYBACK_VOLUME;
        return true;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_GENRE:
        *field = GP100_SETTINGS_DRUM_GENRE;
        return true;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE:
        *field = GP100_SETTINGS_DRUM_STYLE;
        return true;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO:
        *field = GP100_SETTINGS_DRUM_TEMPO;
        return true;
    case GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME:
        *field = GP100_SETTINGS_DRUM_VOLUME;
        return true;
    default:
        return false;
    }
}

gp100_sysex_performance_result_t
gp100_sysex_performance_build_state(
    const gp100_sysex_view_t *request,
    const gp100_settings_record_t *settings,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    gp100_sysex_performance_kind_t kind;
    gp100_settings_field_t field;
    uint8_t payload[4];
    int16_t value;

    if (request == NULL || settings == NULL || response_length == NULL) {
        return GP100_SYSEX_PERFORMANCE_INVALID;
    }
    if (request->message_class != GP100_SYSEX_REQUEST_CLASS ||
        !address_kind(request->address, &kind)) {
        return GP100_SYSEX_PERFORMANCE_UNHANDLED;
    }
    if (request->payload_length != 0u || !kind_field(kind, &field) ||
        !gp100_settings_get_i16(settings, field, &value) ||
        value < 0 || !value_is_valid(kind, (uint16_t)value)) {
        return GP100_SYSEX_PERFORMANCE_INVALID;
    }
    gp100_sysex_encode_u16((uint16_t)value, payload);
    if (!gp100_sysex_build_response(
            request->address, payload, sizeof(payload),
            response, response_capacity, response_length)) {
        return GP100_SYSEX_PERFORMANCE_INVALID;
    }
    return GP100_SYSEX_PERFORMANCE_RESPONSE;
}
