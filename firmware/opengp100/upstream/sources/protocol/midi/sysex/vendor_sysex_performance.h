#ifndef GP100_VENDOR_SYSEX_PERFORMANCE_H
#define GP100_VENDOR_SYSEX_PERFORMANCE_H

#include "storage/settings_record.h"
#include "looper/looper_engine.h"
#include "rhythm/rhythm_engine.h"
#include "vendor_sysex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_RHYTHM_GENRE_COUNT 8u
#define GP100_RHYTHM_STYLE_COUNT GP100_RHYTHM_FACTORY_PATTERN_COUNT

#define GP100_SYSEX_LOOP_RECORD_VOLUME UINT32_C(0x00010103)
#define GP100_SYSEX_LOOP_PLAYBACK_VOLUME UINT32_C(0x00010104)
#define GP100_SYSEX_RHYTHM_GENRE UINT32_C(0x00010200)
#define GP100_SYSEX_RHYTHM_STYLE UINT32_C(0x00010201)
#define GP100_SYSEX_RHYTHM_TEMPO UINT32_C(0x00010203)
#define GP100_SYSEX_RHYTHM_VOLUME UINT32_C(0x00010204)

typedef enum gp100_sysex_performance_kind {
    GP100_SYSEX_PERFORMANCE_LOOP_RECORD_VOLUME = 1,
    GP100_SYSEX_PERFORMANCE_LOOP_PLAYBACK_VOLUME,
    GP100_SYSEX_PERFORMANCE_RHYTHM_GENRE,
    GP100_SYSEX_PERFORMANCE_RHYTHM_STYLE,
    GP100_SYSEX_PERFORMANCE_RHYTHM_TEMPO,
    GP100_SYSEX_PERFORMANCE_RHYTHM_VOLUME
} gp100_sysex_performance_kind_t;

typedef enum gp100_sysex_performance_result {
    GP100_SYSEX_PERFORMANCE_INVALID = -1,
    GP100_SYSEX_PERFORMANCE_UNHANDLED = 0,
    GP100_SYSEX_PERFORMANCE_ACTION = 1,
    GP100_SYSEX_PERFORMANCE_RESPONSE = 2
} gp100_sysex_performance_result_t;

typedef struct gp100_sysex_performance_action {
    gp100_sysex_performance_kind_t kind;
    uint16_t value;
} gp100_sysex_performance_action_t;

typedef struct gp100_sysex_performance_live_result {
    bool looper_changed;
    bool rhythm_changed;
} gp100_sysex_performance_live_result_t;

bool gp100_rhythm_style_location(
    uint16_t absolute_style,
    uint16_t *genre,
    uint16_t *style_in_genre);

bool gp100_rhythm_genre_first_style(
    uint16_t genre,
    uint16_t *absolute_style);

gp100_sysex_performance_result_t
gp100_sysex_performance_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_performance_action_t *action);

bool gp100_sysex_performance_apply(
    gp100_settings_record_t *settings,
    const gp100_sysex_performance_action_t *action);

/* Apply a decoded update atomically to persistence and its live engine. */
bool gp100_sysex_performance_apply_live(
    gp100_settings_record_t *settings,
    gp100_looper_engine_t *looper,
    gp100_rhythm_settings_t *rhythm,
    bool rhythm_enabled,
    const gp100_sysex_performance_action_t *action,
    gp100_sysex_performance_live_result_t *result);

gp100_sysex_performance_result_t
gp100_sysex_performance_build_state(
    const gp100_sysex_view_t *request,
    const gp100_settings_record_t *settings,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
