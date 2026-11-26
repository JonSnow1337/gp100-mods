#include "protocol/midi/sysex/vendor_sysex_performance.h"

#include <stdio.h>
#include <string.h>

static bool read_field(const gp100_settings_record_t *settings,
                       gp100_settings_field_t field,
                       uint16_t expected)
{
    int16_t value;
    return gp100_settings_get_i16(settings, field, &value) &&
           value >= 0 && (uint16_t)value == expected;
}

static int test_rhythm_mapping(void)
{
    uint16_t genre;
    uint16_t relative;
    uint16_t absolute;

    if (!gp100_rhythm_style_location(43u, &genre, &relative) ||
        genre != 1u || relative != 32u ||
        !gp100_rhythm_style_location(44u, &genre, &relative) ||
        genre != 2u || relative != 0u ||
        !gp100_rhythm_style_location(99u, &genre, &relative) ||
        genre != 7u || relative != 9u ||
        !gp100_rhythm_genre_first_style(6u, &absolute) ||
        absolute != 82u ||
        gp100_rhythm_style_location(100u, &genre, &relative) ||
        gp100_rhythm_genre_first_style(8u, &absolute)) {
        return 1;
    }
    return 0;
}

static int apply_value(gp100_settings_record_t *settings,
                       uint32_t address,
                       uint16_t value)
{
    uint8_t payload[4];
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS, address,
        payload, sizeof(payload)
    };
    gp100_sysex_performance_action_t action;

    gp100_sysex_encode_u16(value, payload);
    return gp100_sysex_performance_decode_update(&command, &action) ==
               GP100_SYSEX_PERFORMANCE_ACTION &&
           gp100_sysex_performance_apply(settings, &action)
               ? 0
               : 1;
}

static int test_updates(void)
{
    gp100_settings_record_t settings = {{0}};

    if (apply_value(&settings, GP100_SYSEX_LOOP_RECORD_VOLUME, 100u) ||
        apply_value(&settings, GP100_SYSEX_LOOP_PLAYBACK_VOLUME, 73u) ||
        apply_value(&settings, GP100_SYSEX_RHYTHM_GENRE, 5u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_GENRE, 5u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_STYLE, 59u) ||
        !read_field(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 0u) ||
        apply_value(&settings, GP100_SYSEX_RHYTHM_STYLE, 88u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_GENRE, 6u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_STYLE, 88u) ||
        !read_field(
            &settings, GP100_SETTINGS_DRUM_STYLE_IN_GENRE, 6u) ||
        apply_value(&settings, GP100_SYSEX_RHYTHM_TEMPO, 250u) ||
        apply_value(&settings, GP100_SYSEX_RHYTHM_VOLUME, 91u) ||
        !read_field(
            &settings, GP100_SETTINGS_LOOP_RECORD_VOLUME, 100u) ||
        !read_field(
            &settings, GP100_SETTINGS_LOOP_PLAYBACK_VOLUME, 73u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_TEMPO, 250u) ||
        !read_field(&settings, GP100_SETTINGS_DRUM_VOLUME, 91u)) {
        return 1;
    }
    return 0;
}

static int test_reads_and_bounds(void)
{
    gp100_settings_record_t settings = {{0}};
    gp100_sysex_view_t query = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_RHYTHM_TEMPO, NULL, 0u
    };
    gp100_sysex_view_t response_view;
    gp100_sysex_performance_action_t action;
    uint8_t payload[4];
    uint8_t response[32];
    uint16_t value;
    size_t response_length;

    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, 10) ||
        gp100_sysex_performance_build_state(
            &query, &settings, response, sizeof(response), &response_length) !=
            GP100_SYSEX_PERFORMANCE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        !gp100_sysex_decode_u16(response_view.payload, &value) ||
        value != 10u) {
        return 1;
    }
    query.address = GP100_SYSEX_RHYTHM_VOLUME;
    if (!gp100_settings_set_i16(&settings, GP100_SETTINGS_DRUM_VOLUME, 73) ||
        gp100_sysex_performance_build_state(
            &query, &settings, response, sizeof(response), &response_length) !=
        GP100_SYSEX_PERFORMANCE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.address != query.address || response_view.payload_length != 4u ||
        !gp100_sysex_decode_u16(response_view.payload, &value) || value != 73u) {
        return 2;
    }
    gp100_sysex_encode_u16(251u, payload);
    query.message_class = GP100_SYSEX_RESPONSE_CLASS;
    query.address = GP100_SYSEX_RHYTHM_TEMPO;
    query.payload = payload;
    query.payload_length = sizeof(payload);
    if (gp100_sysex_performance_decode_update(&query, &action) !=
        GP100_SYSEX_PERFORMANCE_INVALID) {
        return 3;
    }
    return 0;
}

static int test_live_updates(void)
{
    int32_t storage[64];
    gp100_settings_record_t settings;
    gp100_looper_engine_t looper;
    gp100_rhythm_settings_t rhythm;
    gp100_sysex_performance_live_result_t result;
    gp100_sysex_performance_action_t action;

    memset(&settings, 0, sizeof(settings));
    if (!gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_STYLE, 0) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_TEMPO, 120) ||
        !gp100_settings_set_i16(
            &settings, GP100_SETTINGS_DRUM_VOLUME, 80) ||
        !gp100_looper_init(
            &looper, storage, sizeof(storage) / sizeof(storage[0]),
            GP100_LOOPER_PRE_EFFECTS, 80u, 80u) ||
        !gp100_rhythm_settings_from_record(
            &settings, true, &rhythm)) {
        return 1;
    }
    action.kind = GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE;
    action.value = 88u;
    if (!gp100_sysex_performance_apply_live(
            &settings, &looper, &rhythm, true, &action, &result) ||
        !result.rhythm_changed || result.looper_changed ||
        rhythm.pattern_index != 88u || !rhythm.enabled ||
        !gp100_settings_validate(&settings)) {
        return 2;
    }
    action.kind = GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME;
    action.value = 100u;
    if (!gp100_sysex_performance_apply_live(
            &settings, &looper, &rhythm, true, &action, &result) ||
        !result.looper_changed || result.rhythm_changed ||
        looper.record_level_target != 100u ||
        !read_field(
            &settings, GP100_SETTINGS_LOOP_RECORD_VOLUME, 100u) ||
        !gp100_settings_validate(&settings)) {
        return 3;
    }
    action.kind = GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO;
    action.value = 251u;
    if (gp100_sysex_performance_apply_live(
            &settings, &looper, &rhythm, true, &action, &result) ||
        rhythm.tempo_bpm != 120u) {
        return 4;
    }
    return 0;
}

int main(void)
{
    int result = test_rhythm_mapping();
    if (result != 0) {
        fprintf(stderr, "rhythm mapping test failed: %d\n", result);
        return 10 + result;
    }
    result = test_updates();
    if (result != 0) {
        fprintf(stderr, "performance update test failed: %d\n", result);
        return 20 + result;
    }
    result = test_reads_and_bounds();
    if (result != 0) {
        fprintf(stderr, "performance read test failed: %d\n", result);
        return 30 + result;
    }
    result = test_live_updates();
    if (result != 0) {
        fprintf(stderr, "performance live test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
