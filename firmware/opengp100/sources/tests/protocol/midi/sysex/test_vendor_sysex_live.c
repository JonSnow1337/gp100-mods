#include "protocol/midi/sysex/vendor_sysex_control.h"
#include "protocol/midi/sysex/vendor_sysex_data.h"
#include "protocol/midi/sysex/vendor_sysex_effect.h"
#include "protocol/midi/sysex/vendor_sysex_global.h"
#include "protocol/midi/sysex/vendor_sysex_live.h"
#include "protocol/midi/sysex/vendor_sysex_performance.h"
#include "protocol/midi/sysex/vendor_sysex_preset.h"
#include "protocol/midi/sysex/vendor_sysex_settings.h"
#include "protocol/midi/sysex/vendor_sysex_system.h"

#include <stdio.h>
#include <string.h>

static bool initialize_runtime(
    gp100_firmware_runtime_t *runtime,
    int32_t *storage,
    size_t storage_samples)
{
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;

    return gp100_make_development_preset(&preset, "SysEx live") &&
           gp100_make_development_settings(&settings) &&
           gp100_firmware_runtime_init(
               runtime, 0u, &preset, &settings, storage, storage_samples,
               0u, 239u);
}

static void make_read(uint32_t address, uint8_t message[14])
{
    const uint8_t prefix[9] = {
        0xf0u, 0x21u, 0x25u, 0u,
        (uint8_t)'G', (uint8_t)'P', (uint8_t)'-', (uint8_t)'d',
        GP100_SYSEX_REQUEST_CLASS
    };

    memcpy(message, prefix, sizeof(prefix));
    message[9] = (uint8_t)(address >> 24u);
    message[10] = (uint8_t)(address >> 16u);
    message[11] = (uint8_t)(address >> 8u);
    message[12] = (uint8_t)address;
    message[13] = 0xf7u;
}

static bool make_write(
    uint32_t address,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *message,
    size_t message_capacity,
    size_t *message_length)
{
    return gp100_sysex_build_response(
        address, payload, payload_length,
        message, message_capacity, message_length);
}

static bool load_test_preset(
    void *context,
    uint16_t logical_preset,
    gp100_preset_record_t *preset)
{
    (void)context;
    if (logical_preset != 5u ||
        !gp100_make_development_preset(preset, "Loaded") ||
        !gp100_preset_set_tempo_bpm(preset, 200u)) {
        return false;
    }
    gp100_preset_update_crc(preset);
    return true;
}

static bool load_test_user_amp(
    void *context,
    uint8_t user_index,
    gp100_sysex_user_amp_record_t *record)
{
    (void)context;
    if (record == NULL || user_index != 4u)
        return false;
    memset(record, 0, sizeof(*record));
    record->present = true;
    memcpy(record->metadata, "Test AMP", 8u);
    return true;
}

static bool load_test_cabinet(
    void *context,
    uint8_t user_index,
    uint8_t record[GP100_CABINET_RECORD_SIZE])
{
    size_t index;

    (void)context;
    if (record == NULL || user_index != 12u)
        return false;
    for (index = 0u; index < GP100_CABINET_RECORD_SIZE; ++index)
        record[index] = (uint8_t)(index * 19u + 7u);
    return true;
}

static int test_read_and_preset_write(void)
{
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_firmware_runtime_t before;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    uint8_t message[48];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t payload[20];
    uint32_t active_algorithm;
    uint32_t other_algorithm;
    uint32_t value_bits;
    float value = 0.5f;
    uint16_t tempo;
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    make_read(GP100_SYSEX_PRESET_TEMPO, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
        result.domain != GP100_SYSEX_DOMAIN_PRESET ||
        !gp100_sysex_parse(response, response_length, &view) ||
        !gp100_sysex_decode_u16(view.payload, &tempo) || tempo != 120u) {
        return 2;
    }

    gp100_sysex_encode_u16(173u, payload);
    if (!make_write(
            GP100_SYSEX_PRESET_TEMPO, payload, 4u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !result.preset_changed || !result.audio_changed ||
        response_length != 0u ||
        gp100_preset_tempo_bpm(&runtime.preset) != 173u ||
        runtime.looper.tempo_bpm != 173u || !runtime.preset_modified ||
        gp100_preset_validate(&runtime.preset) !=
            GP100_PRESET_VALID) {
        return 3;
    }

    before = runtime;
    if (!gp100_preset_algorithm_id(
            &runtime.preset, GP100_EFFECT_DELAY, &active_algorithm) ||
        !gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 0u, &other_algorithm)) {
        return 4;
    }
    if (other_algorithm == active_algorithm &&
        !gp100_effect_algorithm_id(
            GP100_EFFECT_DELAY, 1u, &other_algorithm)) {
        return 4;
    }
    gp100_sysex_encode_u32(other_algorithm, payload);
    gp100_sysex_encode_u16(0u, payload + 8u);
    memcpy(&value_bits, &value, sizeof(value_bits));
    gp100_sysex_encode_u32(value_bits, payload + 12u);
    if (!make_write(
            gp100_sysex_effect_address(
                GP100_EFFECT_DELAY,
                GP100_SYSEX_EFFECT_PARAMETER),
            payload, sizeof(payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_INVALID ||
        memcmp(&runtime, &before, sizeof(runtime)) != 0) {
        return 5;
    }
    return 0;
}

static int test_performance_and_effect_writes(void)
{
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_sysex_live_result_t result;
    uint8_t message[48];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t payload[4];
    bool enabled;
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    gp100_sysex_encode_u16(65u, payload);
    if (!make_write(
            GP100_SYSEX_LOOP_RECORD_VOLUME,
            payload, sizeof(payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !result.settings_changed || !result.looper_changed ||
        runtime.looper.record_level_target != 65u ||
        !runtime.settings_dirty ||
        !gp100_settings_validate(&runtime.settings)) {
        return 2;
    }

    if (!gp100_preset_effect_enabled(
            &runtime.preset, GP100_EFFECT_DELAY, &enabled)) {
        return 3;
    }
    gp100_sysex_encode_u16(enabled ? 0u : 1u, payload);
    if (!make_write(
            gp100_sysex_effect_address(
                GP100_EFFECT_DELAY, GP100_SYSEX_EFFECT_ENABLE),
            payload, sizeof(payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !gp100_preset_effect_enabled(
            &runtime.preset, GP100_EFFECT_DELAY, &enabled) ||
        enabled != runtime.effects.slots[GP100_EFFECT_DELAY].enabled) {
        return 4;
    }
    return 0;
}

static int test_settings_read_and_transactional_failure(void)
{
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_firmware_runtime_t before;
    gp100_sysex_live_result_t result;
    uint8_t message[32];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t payload[4];
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    make_read(GP100_SYSEX_SETTINGS_SNAPSHOT, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
        response_length <= GP100_SYSEX_MIN_MESSAGE_BYTES) {
        return 2;
    }

    before = runtime;
    gp100_sysex_encode_u16(251u, payload);
    if (!make_write(
            GP100_SYSEX_PRESET_TEMPO, payload, sizeof(payload),
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_INVALID ||
        memcmp(&runtime, &before, sizeof(runtime)) != 0 ||
        response_length != 0u) {
        return 3;
    }
    return 0;
}

static int test_system_and_global_settings(void)
{
    const gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .context = NULL,
        .session = NULL
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_firmware_runtime_t before;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    gp100_sysex_global_state_t global;
    uint8_t message[32];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t payload[8];
    uint16_t decoded;
    int16_t stored;
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    make_read(GP100_SYSEX_SYSTEM_MASTER_VOLUME, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        !gp100_sysex_decode_u16(view.payload, &decoded) ||
        decoded != 50u) {
        return 2;
    }
    gp100_sysex_encode_u16(73u, payload);
    if (!make_write(
            GP100_SYSEX_SYSTEM_MASTER_VOLUME, payload, 4u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !result.settings_changed || !runtime.settings_dirty ||
        !gp100_settings_get_i16(
            &runtime.settings, GP100_SETTINGS_MASTER_VOLUME, &stored) ||
        stored != 73 || !gp100_settings_validate(&runtime.settings)) {
        return 3;
    }

    gp100_sysex_encode_u32(UINT32_C(0x3f400000), payload);
    if (!make_write(
            GP100_SYSEX_GLOBAL_PARAMETER_BASE +
                GP100_GLOBAL_EQ_BAND_1_GAIN,
            payload, sizeof(payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !result.settings_changed || result.audio_changed ||
        !gp100_settings_get_global_eq(&runtime.settings, &global) ||
        global.parameters[GP100_GLOBAL_EQ_BAND_1_GAIN] !=
            UINT32_C(0x3f400000) ||
        !gp100_settings_validate(&runtime.settings)) {
        return 4;
    }

    gp100_sysex_encode_u16(0u, payload);
    gp100_sysex_encode_u16(5u, payload + 4u);
    if (!make_write(
            GP100_SYSEX_SYSTEM_PRESET, payload, sizeof(payload),
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_APPLIED ||
        !result.preset_loaded || !result.preset_changed ||
        !result.settings_changed || !runtime.settings_dirty ||
        !result.audio_changed || runtime.ui.logical_preset != 5u ||
        !gp100_settings_get_i16(
            &runtime.settings, GP100_SETTINGS_CURRENT_PRESET,
            &stored) ||
        stored != 5 ||
        gp100_preset_tempo_bpm(&runtime.preset) != 200u ||
        runtime.looper.tempo_bpm != 200u || runtime.preset_modified) {
        return 5;
    }

    before = runtime;
    if (!make_write(
            GP100_SYSEX_SYSTEM_FACTORY_RESET, NULL, 0u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_ACTION ||
        !result.factory_reset_requested ||
        result.save_user_preset_requested || response_length != 0u ||
        memcmp(&runtime, &before, sizeof(runtime)) != 0) {
        return 6;
    }
    return 0;
}

static int test_data_session_status(void)
{
    gp100_sysex_live_session_t session;
    gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .context = NULL,
        .session = &session
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_firmware_runtime_t before;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    gp100_sysex_runtime_status_t runtime_status;
    gp100_sysex_live_status_t live_status;
    uint8_t message[32];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t payload[8];
    uint8_t start_payload[2] = {1u, 1u};
    uint8_t advance_payload[5] = {1u, 1u, 0u, 0u, 0u};
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    gp100_sysex_live_session_reset(&session);
    runtime.ui.logical_preset = 100u;
    make_read(GP100_SYSEX_DATA_PRESET_STATUS, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        result.domain != GP100_SYSEX_DOMAIN_DATA ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.payload_length != 3u || view.payload[0] != 1u ||
        view.payload[1] != 1u || view.payload[2] != 1u) {
        return 2;
    }
    runtime.ui.drum_enabled = true;
    runtime.rhythm.enabled = true;
    runtime.rhythm.target_gain_q8 = 67;
    runtime.preset_modified = true;
    if (!gp100_firmware_runtime_set_volume_pedal(
            &runtime, 42.75f, GP100_VOLUME_PEDAL_PRE)) {
        return 3;
    }
    make_read(GP100_SYSEX_DATA_RUNTIME_STATUS, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        !gp100_sysex_data_decode_runtime_status(
            view.payload, view.payload_length, &runtime_status) ||
        runtime_status.rhythm_enabled != 1u ||
        runtime_status.volume_pedal_value != 42u ||
        runtime_status.rhythm_volume != 67u ||
        runtime_status.preset_modified != 1u) {
        return 4;
    }
    make_read(GP100_SYSEX_DATA_READ_CURRENT_PRESET, message);
    if (gp100_sysex_live_handle(
            &runtime, &hooks, message, 14u,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !session.preset_download_active ||
        session.preset_download_selected ||
        session.preset_download.chunk_count != 9u ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_DATA_CURRENT_PRESET_READY ||
        view.payload_length != GP100_SYSEX_PRESET_DESCRIPTOR_BYTES ||
        view.payload[0] != 1u || view.payload[1] != 1u) {
        return 5;
    }
    if (!make_write(
            GP100_SYSEX_CURRENT_START,
            start_payload, sizeof(start_payload),
            message, sizeof(message), &message_length) ||
        (live_status = gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result)) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_CHUNK ||
        view.payload_length !=
            GP100_SYSEX_TRANSFER_PREFIX_BYTES +
                GP100_SYSEX_TRANSFER_CHUNK_BYTES ||
        view.payload[2] != 0u || view.payload[3] != 0u) {
        return 6;
    }
    session.preset_download.next_chunk = 8u;
    advance_payload[4] = 1u;
    if (!make_write(
            GP100_SYSEX_CURRENT_ADVANCE,
            advance_payload, sizeof(advance_payload),
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        session.preset_download_active ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_COMPLETE ||
        view.payload[2] != 0u || view.payload[3] != 9u) {
        return 7;
    }
    if (!gp100_sysex_live_session_select_preset(&session, 5u)) {
        return 8;
    }
    make_read(GP100_SYSEX_DATA_READ_SELECTED_PRESET, message);
    if (gp100_sysex_live_handle(
            &runtime, &hooks, message, 14u,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !session.preset_download_active ||
        !session.preset_download_selected ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_DATA_SELECTED_PRESET_READY ||
        view.payload[0] != 0u || view.payload[1] != 5u) {
        return 9;
    }
    before = runtime;
    gp100_sysex_encode_u16(1u, payload);
    gp100_sysex_encode_u16(42u, payload + 4u);
    if (!make_write(
            GP100_SYSEX_DATA_SAVE_USER_PRESET,
            payload, sizeof(payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_ACTION ||
        !result.save_user_preset_requested ||
        result.save_user_preset_index != 42u ||
        memcmp(&runtime, &before, sizeof(runtime)) != 0) {
        return 10;
    }
    return 0;
}

static int test_preset_upload_recall(void)
{
    gp100_sysex_live_session_t session;
    gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .context = NULL,
        .session = &session
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_preset_record_t source;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    uint8_t encoded[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    uint8_t payload[GP100_SYSEX_TRANSFER_PREFIX_BYTES +
                    GP100_SYSEX_TRANSFER_CHUNK_BYTES];
    uint8_t message[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    const uint8_t begin[6] = {1u, 1u, 0x0cu, 0x1cu, 1u, 0x40u};
    const uint8_t finish[2] = {1u, 1u};
    size_t encoded_length;
    size_t message_length;
    size_t response_length;
    int16_t current_preset;
    uint16_t chunk;

    if (!initialize_runtime(&runtime, storage, 128u) ||
        !gp100_make_development_preset(&source, "Uploaded") ||
        !gp100_preset_set_tempo_bpm(&source, 201u) ||
        !gp100_sysex_encode_preset_transfer(
            &source, 100u, encoded, sizeof(encoded), &encoded_length)) {
        return 1;
    }
    gp100_sysex_live_session_reset(&session);
    if (!make_write(
            GP100_SYSEX_DATA_CURRENT_PRESET_READY,
            begin, sizeof(begin), message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !session.preset_upload.active ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_START) {
        return 2;
    }
    for (chunk = 0u; chunk < 9u; ++chunk) {
        const size_t chunk_size = chunk < 8u
            ? GP100_SYSEX_TRANSFER_CHUNK_BYTES : 0x1cu;
        const size_t offset =
            (size_t)chunk * GP100_SYSEX_TRANSFER_CHUNK_BYTES;

        payload[0] = 1u;
        payload[1] = 1u;
        payload[2] = (uint8_t)(chunk >> 7u);
        payload[3] = (uint8_t)(chunk & 0x7fu);
        memcpy(payload + GP100_SYSEX_TRANSFER_PREFIX_BYTES,
               encoded + offset, chunk_size);
        if (!make_write(
                GP100_SYSEX_CURRENT_CHUNK,
                payload,
                GP100_SYSEX_TRANSFER_PREFIX_BYTES + chunk_size,
                message, sizeof(message), &message_length) ||
            gp100_sysex_live_handle(
                &runtime, &hooks, message, message_length,
                response, sizeof(response), &response_length, &result) !=
                    GP100_SYSEX_LIVE_RESPONSE) {
            return 3;
        }
    }
    if (!make_write(
            GP100_SYSEX_CURRENT_COMPLETE,
            finish, sizeof(finish), message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !result.preset_loaded || !result.preset_changed ||
        !result.settings_changed || !runtime.settings_dirty ||
        !result.audio_changed || runtime.ui.logical_preset != 100u ||
        !gp100_settings_get_i16(
            &runtime.settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) ||
        current_preset != 100 ||
        gp100_preset_tempo_bpm(&runtime.preset) != 201u ||
        runtime.looper.tempo_bpm != 201u || !runtime.preset_modified ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.address != GP100_SYSEX_CURRENT_FINAL ||
        view.payload_length != 5u || view.payload[4] != 1u) {
        return 4;
    }
    return 0;
}

static int test_genre_session(void)
{
    static const uint8_t name[] = "MY GENRE";
    gp100_sysex_live_session_t session;
    gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .context = NULL,
        .session = &session
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    uint8_t payload[GP100_SYSEX_GENRE_READ_BYTES] = {0};
    uint8_t message[48];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    size_t message_length;
    size_t response_length;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    gp100_sysex_live_session_reset(&session);
    gp100_sysex_encode_u16(11u, payload);
    memcpy(payload + GP100_SYSEX_GENRE_INDEX_BYTES,
           name, sizeof(name) - 1u);
    if (!make_write(
            GP100_SYSEX_GENRE_NAME, payload,
            GP100_SYSEX_GENRE_INDEX_BYTES + sizeof(name) - 1u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_APPLIED ||
        !result.preset_changed || !result.genre_changed ||
        !runtime.preset_modified ||
        gp100_preset_genre_index(&runtime.preset) != 11u) {
        return 2;
    }
    gp100_sysex_encode_u16(10u, payload);
    if (!make_write(
            GP100_SYSEX_GENRE_NAME, payload,
            GP100_SYSEX_GENRE_INDEX_BYTES + sizeof(name) - 1u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_INVALID ||
        result.genre_changed) {
        return 3;
    }
    gp100_sysex_encode_u16(11u, payload);
    if (!make_write(
            GP100_SYSEX_GENRE_NAME, payload,
            GP100_SYSEX_GENRE_INDEX_BYTES,
            message, sizeof(message), &message_length)) {
        return 4;
    }
    message[8] = GP100_SYSEX_REQUEST_CLASS;
    if (gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.payload_length != GP100_SYSEX_GENRE_READ_BYTES ||
        memcmp(view.payload + GP100_SYSEX_GENRE_INDEX_BYTES,
               name, sizeof(name) - 1u) != 0) {
        return 5;
    }
    return 0;
}

static int test_cabinet_upload_session(void)
{
    gp100_sysex_live_session_t session;
    gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .load_cabinet = load_test_cabinet,
        .context = NULL,
        .session = &session
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    uint8_t record[GP100_CABINET_RECORD_SIZE];
    uint8_t encoded[GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES];
    uint8_t payload[GP100_SYSEX_CABINET_UPLOAD_PAYLOAD_BYTES];
    uint8_t edit_payload[4u + GP100_SYSEX_CABINET_METADATA_BYTES];
    uint8_t message[64];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t metadata[GP100_SYSEX_CABINET_METADATA_BYTES];
    uint8_t descriptor_index;
    uint8_t impulse_checksum;
    size_t encoded_length;
    size_t message_length;
    size_t response_length;
    size_t chunk;
    size_t index;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    for (index = 0u; index < sizeof(record); ++index)
        record[index] = (uint8_t)(index * 19u + 7u);
    gp100_sysex_live_session_reset(&session);
    payload[0] = 12u;
    if (!make_write(
            GP100_SYSEX_CABINET_METADATA_READ,
            payload, 1u, message, sizeof(message), &message_length)) {
        return 2;
    }
    message[8] = GP100_SYSEX_REQUEST_CLASS;
    if (gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        !gp100_sysex_cabinet_decode_info(
            view.payload, view.payload_length, &descriptor_index,
            metadata, &impulse_checksum) || descriptor_index != 12u ||
        memcmp(metadata, record, sizeof(metadata)) != 0 ||
        impulse_checksum !=
            gp100_cabinet_impulse_nibble_checksum(record)) {
        return 3;
    }
    memset(edit_payload, 0, sizeof(edit_payload));
    gp100_sysex_encode_u16(12u, edit_payload);
    memcpy(edit_payload + 4u, "Renamed IR", 11u);
    if (!make_write(
            GP100_SYSEX_CABINET_METADATA_UPDATE,
            edit_payload, sizeof(edit_payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_ACTION ||
        response_length != 0u || !result.cabinet_record_ready ||
        result.cabinet_reload_requested ||
        result.cabinet_user_index != 12u ||
        memcmp(session.cabinet_completed_record, edit_payload + 4u,
               GP100_SYSEX_CABINET_METADATA_BYTES) != 0 ||
        session.cabinet_completed_record[
            GP100_SYSEX_CABINET_METADATA_BYTES] !=
            (uint8_t)(GP100_SYSEX_CABINET_METADATA_BYTES * 19u + 7u)) {
        return 4;
    }
    gp100_sysex_encode_u16(12u, edit_payload);
    if (!make_write(
            GP100_SYSEX_CABINET_RESET,
            edit_payload, 4u, message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_ACTION ||
        !result.cabinet_record_ready ||
        !result.cabinet_reload_requested ||
        result.cabinet_user_index != 12u) {
        return 5;
    }
    for (index = 0u;
         index < GP100_SYSEX_CABINET_METADATA_BYTES; ++index) {
        if (session.cabinet_completed_record[index] != 0u)
            return 6;
    }
    if (session.cabinet_completed_record[
            GP100_SYSEX_CABINET_METADATA_BYTES] !=
        (uint8_t)(GP100_SYSEX_CABINET_METADATA_BYTES * 19u + 7u)) {
        return 7;
    }
    if (!gp100_sysex_nibble_encode(
            record, sizeof(record), encoded, sizeof(encoded),
            &encoded_length) || encoded_length != sizeof(encoded)) {
        return 8;
    }
    gp100_sysex_live_session_reset(&session);
    for (chunk = 0u;
         chunk < GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT;
         ++chunk) {
        uint8_t checksum = 0u;
        const size_t offset =
            chunk * GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;

        memset(payload, 0, sizeof(payload));
        payload[0] = 12u;
        payload[1] = (uint8_t)(chunk >> 7u);
        payload[2] = (uint8_t)(chunk & 0x7fu);
        for (index = 0u;
             index < GP100_SYSEX_CABINET_UPLOAD_CHUNK_NIBBLES;
             ++index) {
            if (offset + index < sizeof(encoded))
                payload[3u + index] = encoded[offset + index];
            else
                payload[3u + index] =
                    GP100_SYSEX_CABINET_UPLOAD_PADDING_NIBBLE;
            checksum = (uint8_t)(checksum + payload[3u + index]);
        }
        payload[31] = (uint8_t)(checksum >> 4u);
        payload[32] = (uint8_t)(checksum & 0x0fu);
        if (!make_write(
                GP100_SYSEX_CABINET_METADATA_READ,
                payload, sizeof(payload), message, sizeof(message),
                &message_length) ||
            gp100_sysex_live_handle(
                &runtime, &hooks, message, message_length,
                response, sizeof(response), &response_length, &result) !=
                    GP100_SYSEX_LIVE_RESPONSE ||
            !gp100_sysex_parse(response, response_length, &view) ||
            view.payload_length != 4u || view.payload[3] != 1u ||
            (chunk + 1u < GP100_SYSEX_CABINET_UPLOAD_CHUNK_COUNT &&
             result.cabinet_record_ready)) {
            return 9;
        }
    }
    if (!result.cabinet_record_ready || !result.cabinet_reload_requested ||
        result.cabinet_user_index != 12u ||
        memcmp(session.cabinet_completed_record,
               record, sizeof(record)) != 0) {
        return 10;
    }
    return 0;
}

static int test_user_amp_upload_session(void)
{
    gp100_sysex_live_session_t session;
    gp100_sysex_live_hooks_t hooks = {
        .load_preset = load_test_preset,
        .load_user_amp = load_test_user_amp,
        .context = NULL,
        .session = &session
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    gp100_sysex_user_amp_record_t descriptor;
    uint8_t descriptor_index;
    uint8_t package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES];
    uint8_t encoded[GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES];
    uint8_t payload[GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES];
    uint8_t edit_payload[
        4u + GP100_SYSEX_USER_AMP_NAME_BYTES];
    uint8_t message[64];
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    size_t encoded_length;
    size_t message_length;
    size_t response_length;
    size_t chunk;
    size_t index;

    if (!initialize_runtime(&runtime, storage, 128u)) return 1;
    gp100_sysex_live_session_reset(&session);
    payload[0] = 4u;
    if (!make_write(
            GP100_SYSEX_USER_AMP_DESCRIPTOR_READ,
            payload, 1u, message, sizeof(message), &message_length)) {
        return 2;
    }
    message[8] = GP100_SYSEX_REQUEST_CLASS;
    if (gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        !gp100_sysex_user_amp_decode_descriptor(
            view.payload, view.payload_length,
            &descriptor_index, &descriptor) ||
        descriptor_index != 4u || !descriptor.present ||
        memcmp(descriptor.metadata, "Test AMP", 8u) != 0) {
        return 3;
    }
    memset(edit_payload, 0, sizeof(edit_payload));
    gp100_sysex_encode_u16(4u, edit_payload);
    memcpy(edit_payload + 4u, "Renamed AMP", 12u);
    if (!make_write(
            GP100_SYSEX_USER_AMP_NAME_UPDATE,
            edit_payload, sizeof(edit_payload), message, sizeof(message),
            &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_ACTION ||
        response_length != 0u || !result.user_amp_descriptor_ready ||
        result.user_amp_reload_requested ||
        result.user_amp_index != 4u ||
        !session.user_amp_completed_record.present ||
        memcmp(session.user_amp_completed_record.metadata,
               edit_payload + 4u,
               GP100_SYSEX_USER_AMP_NAME_BYTES) != 0) {
        return 4;
    }
    payload[0] = 0u;
    payload[1] = 4u;
    if (!make_write(
            GP100_SYSEX_USER_AMP_RESET,
            payload, 2u, message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, &hooks, message, message_length,
            response, sizeof(response), &response_length, &result) !=
                GP100_SYSEX_LIVE_ACTION ||
        !result.user_amp_descriptor_ready ||
        !result.user_amp_reload_requested || result.user_amp_index != 4u ||
        session.user_amp_completed_record.present ||
        memcmp(session.user_amp_completed_record.metadata,
               "SnapTone5", 10u) != 0) {
        return 5;
    }
    for (index = 0u; index < sizeof(package); ++index)
        package[index] = (uint8_t)(index * 13u + 5u);
    if (!gp100_sysex_nibble_encode(
            package, sizeof(package), encoded, sizeof(encoded),
            &encoded_length) || encoded_length != sizeof(encoded)) {
        return 6;
    }
    gp100_sysex_live_session_reset(&session);
    for (chunk = 0u;
         chunk < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT;
         ++chunk) {
        uint8_t checksum = 0u;
        const size_t offset =
            chunk * GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;

        memset(payload, 0, sizeof(payload));
        payload[0] = 4u;
        payload[1] = (uint8_t)(chunk >> 7u);
        payload[2] = (uint8_t)(chunk & 0x7fu);
        for (index = 0u;
             index < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_NIBBLES;
             ++index) {
            if (offset + index < sizeof(encoded))
                payload[3u + index] = encoded[offset + index];
            else
                payload[3u + index] =
                    GP100_SYSEX_USER_AMP_UPLOAD_PADDING_NIBBLE;
            checksum = (uint8_t)(checksum + payload[3u + index]);
        }
        payload[31] = (uint8_t)(checksum >> 4u);
        payload[32] = (uint8_t)(checksum & 0x0fu);
        if (!make_write(
                GP100_SYSEX_USER_AMP_UPLOAD,
                payload, sizeof(payload), message, sizeof(message),
                &message_length) ||
            gp100_sysex_live_handle(
                &runtime, &hooks, message, message_length,
                response, sizeof(response), &response_length, &result) !=
                    GP100_SYSEX_LIVE_RESPONSE ||
            !gp100_sysex_parse(response, response_length, &view) ||
            view.payload_length != 4u || view.payload[3] != 1u ||
            (chunk + 1u < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT &&
             result.user_amp_package_ready)) {
            return 7;
        }
    }
    if (!result.user_amp_package_ready ||
        !result.user_amp_reload_requested || result.user_amp_index != 4u ||
        memcmp(session.user_amp_completed_package,
               package, sizeof(package)) != 0) {
        return 8;
    }
    return 0;
}

static int test_expression_controls(void)
{
    static const uint16_t raw_positions[] = {1u, 0u};
    static const struct {
        uint32_t address;
        float value;
    } ranges[] = {
        {GP100_SYSEX_CONTROL_VOLUME_MINIMUM, 88.5f},
        {GP100_SYSEX_CONTROL_VOLUME_MAXIMUM, 12.0f},
    };
    int32_t storage[128];
    gp100_firmware_runtime_t runtime;
    gp100_sysex_live_result_t result;
    gp100_sysex_view_t view;
    uint8_t message[24];
    uint8_t response[32];
    uint8_t payload[8];
    size_t message_length;
    size_t response_length;
    size_t index;
    float minimum;
    float maximum;
    gp100_expression_pedal_target_t target;
    uint16_t read_target;

    if (!initialize_runtime(&runtime, storage, 128u) ||
        !gp100_firmware_runtime_set_volume_pedal(
            &runtime, 42.0f, GP100_VOLUME_PEDAL_POST)) {
        return 1;
    }
    for (index = 0u; index < sizeof(raw_positions) / sizeof(raw_positions[0]); ++index) {
        uint16_t raw_position = raw_positions[index];
        gp100_volume_pedal_position_t expected = raw_position == 1u
            ? GP100_VOLUME_PEDAL_PRE : GP100_VOLUME_PEDAL_POST;
        uint16_t read_position;

        gp100_sysex_encode_u16(raw_position, payload);
        if (!make_write(
                GP100_SYSEX_CONTROL_VOLUME_POSITION, payload, 4u,
                message, sizeof(message), &message_length) ||
            gp100_sysex_live_handle(
                &runtime, NULL, message, message_length, response, sizeof(response),
                &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
            result.domain != GP100_SYSEX_DOMAIN_CONTROL ||
            !result.preset_changed || !result.audio_changed || result.settings_changed ||
            runtime.volume_pedal_position != expected || runtime.volume_pedal_percent != 42u ||
            runtime.preset.bytes[0x284] != (uint8_t)raw_position) {
            return 2;
        }
        make_read(GP100_SYSEX_CONTROL_VOLUME_POSITION, message);
        if (gp100_sysex_live_handle(
                &runtime, NULL, message, 14u, response, sizeof(response),
                &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
            !gp100_sysex_parse(response, response_length, &view) ||
            view.payload_length != 4u ||
            !gp100_sysex_decode_u16(view.payload, &read_position) ||
            read_position != raw_position) {
            return 3;
        }
    }
    gp100_sysex_encode_u16(2u, payload);
    if (!make_write(
            GP100_SYSEX_CONTROL_VOLUME_POSITION, payload, 4u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_INVALID ||
        runtime.volume_pedal_position != GP100_VOLUME_PEDAL_POST ||
        runtime.volume_pedal_percent != 42u) {
        return 4;
    }
    for (index = 0u; index < sizeof(ranges) / sizeof(ranges[0]); ++index) {
        uint32_t bits;
        uint32_t received;

        memcpy(&bits, &ranges[index].value, sizeof(bits));
        gp100_sysex_encode_u32(bits, payload);
        if (!make_write(ranges[index].address, payload, sizeof(payload),
                message, sizeof(message), &message_length) ||
            gp100_sysex_live_handle(
                &runtime, NULL, message, message_length, response, sizeof(response),
                &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
            result.domain != GP100_SYSEX_DOMAIN_CONTROL || !result.preset_changed) {
            return 5;
        }
        make_read(ranges[index].address, message);
        if (gp100_sysex_live_handle(
                &runtime, NULL, message, 14u, response, sizeof(response),
                &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
            !gp100_sysex_parse(response, response_length, &view) ||
            view.payload_length != 8u ||
            !gp100_sysex_decode_u32(view.payload, &received) || received != bits) {
            return 6;
        }
    }
    if (!gp100_preset_control_range(
            &runtime.preset, GP100_CONTROL_RANGE_VOLUME, &minimum, &maximum) ||
        minimum != ranges[0].value || maximum != ranges[1].value) {
        return 7;
    }
    gp100_sysex_encode_u16(1u, payload);
    if (!make_write(GP100_SYSEX_CONTROL_PEDAL_TARGET, payload, 4u,
            message, sizeof(message), &message_length) ||
        gp100_sysex_live_handle(
            &runtime, NULL, message, message_length, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_APPLIED ||
        !gp100_preset_expression_pedal_target(&runtime.preset, &target) ||
        target != GP100_EXPRESSION_PEDAL_EFFECT ||
        runtime.volume_pedal_percent != 42u) {
        return 8;
    }
    make_read(GP100_SYSEX_CONTROL_PEDAL_TARGET, message);
    if (gp100_sysex_live_handle(
            &runtime, NULL, message, 14u, response, sizeof(response),
            &response_length, &result) != GP100_SYSEX_LIVE_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &view) ||
        view.payload_length != 4u ||
        !gp100_sysex_decode_u16(view.payload, &read_target) || read_target != 1u) {
        return 9;
    }
    return 0;
}

int main(void)
{
    int result = test_read_and_preset_write();
    if (result != 0) {
        fprintf(stderr, "read/preset-write test failed: %d\n", result);
        return 10 + result;
    }
    result = test_performance_and_effect_writes();
    if (result != 0) {
        fprintf(stderr, "performance/effect test failed: %d\n", result);
        return 20 + result;
    }
    result = test_settings_read_and_transactional_failure();
    if (result != 0) {
        fprintf(stderr, "settings/transaction test failed: %d\n", result);
        return 30 + result;
    }
    result = test_system_and_global_settings();
    if (result != 0) {
        fprintf(stderr, "system/global test failed: %d\n", result);
        return 40 + result;
    }
    result = test_data_session_status();
    if (result != 0) {
        fprintf(stderr, "data-session test failed: %d\n", result);
        return 50 + result;
    }
    result = test_preset_upload_recall();
    if (result != 0) {
        fprintf(stderr, "preset-upload test failed: %d\n", result);
        return 60 + result;
    }
    result = test_genre_session();
    if (result != 0) {
        fprintf(stderr, "genre-session test failed: %d\n", result);
        return 70 + result;
    }
    result = test_cabinet_upload_session();
    if (result != 0) {
        fprintf(stderr, "cabinet-upload session test failed: %d\n", result);
        return 80 + result;
    }
    result = test_user_amp_upload_session();
    if (result != 0) {
        fprintf(stderr, "user-amp upload session test failed: %d\n", result);
        return 90 + result;
    }
    result = test_expression_controls();
    if (result != 0) {
        fprintf(stderr, "expression controls test failed: %d\n", result);
        return 100 + result;
    }
    return 0;
}
