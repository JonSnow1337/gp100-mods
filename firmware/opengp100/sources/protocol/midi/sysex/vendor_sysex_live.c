#include "vendor_sysex_live.h"

#include "storage/patch_chain.h"

#include "storage/preset_normalize.h"
#include "vendor_sysex_control.h"
#include "vendor_sysex_data.h"
#include "vendor_sysex_effect.h"
#include "vendor_sysex_genre.h"
#include "vendor_sysex_global.h"
#include "vendor_sysex_performance.h"
#include "vendor_sysex_preset.h"
#include "vendor_sysex_settings.h"
#include "vendor_sysex_system.h"

#include <string.h>

void gp100_sysex_live_session_reset(
    gp100_sysex_live_session_t *session)
{
    if (session != NULL) {
        memset(session, 0, sizeof(*session));
        (void)gp100_sysex_upload_init(
            &session->preset_upload,
            session->preset_upload_data,
            sizeof(session->preset_upload_data));
        gp100_genre_catalog_reset(&session->genre_catalog);
        (void)gp100_sysex_cabinet_upload_init(
            &session->cabinet_upload,
            session->cabinet_upload_data,
            sizeof(session->cabinet_upload_data));
        (void)gp100_sysex_user_amp_upload_init(
            &session->user_amp_upload,
            session->user_amp_upload_data,
            sizeof(session->user_amp_upload_data));
    }
}

bool gp100_sysex_live_session_select_preset(
    gp100_sysex_live_session_t *session,
    uint16_t logical_preset)
{
    if (session == NULL ||
        logical_preset >= GP100_TOTAL_LOGICAL_PRESETS) {
        return false;
    }
    session->selected_logical_preset = logical_preset;
    session->selected_logical_preset_valid = true;
    return true;
}

static bool rebuild_preset_consumers(
    gp100_firmware_runtime_t *runtime,
    gp100_sysex_live_result_t *result)
{
    gp100_effect_chain_t chain;
    gp100_volume_pedal_position_t volume_position;
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];

    if (!gp100_effect_chain_from_preset(&chain, &runtime->preset) ||
        !gp100_effect_runtime_sync_preset(
            &runtime->effects, &runtime->preset, actions) ||
        !gp100_audio_output_level_set_target(
            &runtime->output,
            (uint8_t)gp100_preset_level(&runtime->preset)) ||
        !gp100_preset_volume_pedal_position(
            &runtime->preset, &volume_position) ||
        !gp100_firmware_runtime_set_volume_pedal(
            runtime, (float)runtime->volume_pedal_percent,
            volume_position) ||
        !gp100_looper_set_tempo(
            &runtime->looper, runtime->looper.sample_rate_hz,
            gp100_preset_tempo_bpm(&runtime->preset))) {
        return false;
    }
    runtime->chain = chain;
    gp100_preset_update_crc(&runtime->preset);
    runtime->preset_modified = true;
    result->preset_changed = true;
    result->audio_changed = true;
    return true;
}

static bool system_state_from_runtime(
    const gp100_firmware_runtime_t *runtime,
    gp100_sysex_system_state_t *state)
{
    int16_t language;

    if (!gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_FOOTSWITCH_MODE,
            &state->footswitch_mode) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_LANGUAGE_INDEX,
            &language) || language < 0 || language > UINT8_MAX ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_MASTER_VOLUME,
            &state->master_volume) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_INPUT_LEVEL_DB,
            &state->input_level_db) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_LEVEL_DB,
            &state->usb_record_level_db) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_MONITOR_LEVEL_DB,
            &state->usb_monitor_level_db) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT,
            &state->usb_record_source_left) ||
        !gp100_settings_get_i16(
            &runtime->settings, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT,
            &state->usb_record_source_right)) {
        return false;
    }
    state->current_preset = runtime->ui.logical_preset;
    state->language_identifier = (uint8_t)language;
    return true;
}

static gp100_sysex_live_status_t prepare_preset_download(
    const gp100_sysex_view_t *request,
    const gp100_firmware_runtime_t *runtime,
    const gp100_sysex_live_hooks_t *hooks,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    gp100_preset_record_t loaded;
    const gp100_preset_record_t *record;
    gp100_sysex_live_session_t *session;
    uint16_t logical_preset;
    bool selected;
    bool has_requested_location;
    size_t encoded_length;

    if (request->address != GP100_SYSEX_DATA_READ_CURRENT_PRESET &&
        request->address != GP100_SYSEX_DATA_READ_SELECTED_PRESET) {
        return GP100_SYSEX_LIVE_UNHANDLED;
    }
    selected = request->address ==
               GP100_SYSEX_DATA_READ_SELECTED_PRESET;
    has_requested_location = selected && request->payload_length == 2u;
    if (request->payload_length != 0u && !has_requested_location) {
        return GP100_SYSEX_LIVE_INVALID;
    }
    if (has_requested_location &&
        (request->payload[0] >=
             GP100_TOTAL_LOGICAL_PRESETS / GP100_PRESETS_PER_BANK ||
         request->payload[1] >= GP100_PRESETS_PER_BANK)) {
        return GP100_SYSEX_LIVE_INVALID;
    }
    if (hooks == NULL || hooks->session == NULL) {
        return GP100_SYSEX_LIVE_UNHANDLED;
    }
    session = hooks->session;
    if (has_requested_location) {
        logical_preset = (uint16_t)(
            request->payload[0] * GP100_PRESETS_PER_BANK +
            request->payload[1]);
    } else {
        logical_preset = selected && session->selected_logical_preset_valid
            ? session->selected_logical_preset
            : runtime->ui.logical_preset;
    }
    if (!has_requested_location && logical_preset == runtime->ui.logical_preset) {
        record = &runtime->preset;
    } else {
        if (hooks->load_preset == NULL ||
            !hooks->load_preset(hooks->context, logical_preset, &loaded) ||
            gp100_preset_validate(&loaded) != GP100_PRESET_VALID) {
            return GP100_SYSEX_LIVE_INVALID;
        }
        (void)gp100_preset_normalize(&loaded, &runtime->catalog);
        record = &loaded;
    }
    if (!gp100_sysex_build_preset_descriptor(
            request->address, logical_preset,
            response, response_capacity, response_length) ||
        !gp100_sysex_encode_preset_transfer(
            record, logical_preset,
            session->preset_download_data,
            sizeof(session->preset_download_data), &encoded_length) ||
        !gp100_sysex_transfer_init(
            &session->preset_download,
            session->preset_download_data, encoded_length)) {
        return GP100_SYSEX_LIVE_INVALID;
    }
    session->preset_download_active = true;
    session->preset_download_selected = selected;
    return GP100_SYSEX_LIVE_RESPONSE;
}

static gp100_sysex_live_status_t handle_read(
    const gp100_sysex_view_t *request,
    gp100_sysex_domain_t domain,
    const gp100_firmware_runtime_t *runtime,
    const gp100_sysex_live_hooks_t *hooks,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    int domain_result;

    switch (domain) {
    case GP100_SYSEX_DOMAIN_CONTROL:
        domain_result = gp100_sysex_control_build_state(
            request, &runtime->preset, response, response_capacity,
            response_length);
        return domain_result == GP100_SYSEX_CONTROL_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_CONTROL_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    case GP100_SYSEX_DOMAIN_PERFORMANCE:
        domain_result = gp100_sysex_performance_build_state(
            request, &runtime->settings, response, response_capacity,
            response_length);
        return domain_result == GP100_SYSEX_PERFORMANCE_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_PERFORMANCE_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    case GP100_SYSEX_DOMAIN_SYSTEM: {
        gp100_sysex_system_state_t system;
        if (!system_state_from_runtime(runtime, &system))
            return GP100_SYSEX_LIVE_INVALID;
        domain_result = gp100_sysex_system_build_state(
            request, &system, response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_SYSTEM_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_SYSTEM_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    case GP100_SYSEX_DOMAIN_GLOBAL: {
        gp100_sysex_global_state_t global;
        if (!gp100_settings_get_global_eq(&runtime->settings, &global))
            return GP100_SYSEX_LIVE_INVALID;
        domain_result = gp100_sysex_global_build_state(
            request, &global, response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_GLOBAL_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_GLOBAL_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    case GP100_SYSEX_DOMAIN_EFFECT:
        domain_result = gp100_sysex_effect_build_state(
            request, &runtime->effects, response, response_capacity,
            response_length);
        return domain_result == GP100_SYSEX_EFFECT_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_EFFECT_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    case GP100_SYSEX_DOMAIN_PRESET: {
        gp100_preset_record_t stored;
        const gp100_preset_record_t *record = &runtime->preset;

        if (request->address == GP100_SYSEX_PRESET_AUTHOR ||
            request->address == GP100_SYSEX_PRESET_NOTES) {
            uint8_t logical_preset;
            if (request->payload_length != GP100_SYSEX_PRESET_LOCATION_BYTES)
                return GP100_SYSEX_LIVE_INVALID;
            logical_preset = request->payload[1];
            if (logical_preset >= GP100_TOTAL_LOGICAL_PRESETS)
                return GP100_SYSEX_LIVE_INVALID;
            if (logical_preset != runtime->ui.logical_preset) {
                gp100_preset_loader_t loader = NULL;
                if (hooks != NULL) {
                    loader = hooks->load_stored_preset != NULL
                        ? hooks->load_stored_preset : hooks->load_preset;
                }
                if (loader == NULL ||
                    !loader(hooks->context, logical_preset, &stored) ||
                    gp100_preset_validate(&stored) != GP100_PRESET_VALID) {
                    memset(&stored, 0, sizeof(stored));
                }
                record = &stored;
            }
        }
        domain_result = gp100_sysex_preset_build_state(
            request, record, response, response_capacity,
            response_length);
        return domain_result == GP100_SYSEX_PRESET_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_PRESET_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    case GP100_SYSEX_DOMAIN_GENRE:
        if (hooks == NULL || hooks->session == NULL)
            return GP100_SYSEX_LIVE_UNHANDLED;
        domain_result = gp100_sysex_genre_build_state(
            request, &hooks->session->genre_catalog,
            response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_GENRE_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_GENRE_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    case GP100_SYSEX_DOMAIN_DATA:
    {
        gp100_sysex_runtime_status_t live_status;
        gp100_sysex_live_status_t transfer_status = gp100_patch_is_native(&runtime->preset)
            ? GP100_SYSEX_LIVE_UNHANDLED : prepare_preset_download(
                request, runtime, hooks, response, response_capacity,
                response_length);

        if (transfer_status != GP100_SYSEX_LIVE_UNHANDLED)
            return transfer_status;

        live_status.rhythm_enabled = runtime->ui.drum_enabled ? 1u : 0u;
        live_status.volume_pedal_value = runtime->volume_pedal_percent;
        live_status.rhythm_volume = (uint16_t)runtime->rhythm.target_gain_q8;
        live_status.preset_modified = runtime->preset_modified ? 1u : 0u;
        domain_result = gp100_sysex_data_build_runtime_status(
            request, &live_status, response, response_capacity,
            response_length);
        if (domain_result == GP100_SYSEX_DATA_RESPONSE)
            return GP100_SYSEX_LIVE_RESPONSE;
        if (domain_result == GP100_SYSEX_DATA_INVALID)
            return GP100_SYSEX_LIVE_INVALID;
        domain_result = gp100_sysex_data_handle_control(
            request, runtime->ui.logical_preset,
            response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_DATA_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_DATA_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    case GP100_SYSEX_DOMAIN_SETTINGS:
        domain_result = gp100_sysex_settings_build_snapshot(
            request, &runtime->settings, response, response_capacity,
            response_length);
        return domain_result == GP100_SYSEX_SETTINGS_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_SETTINGS_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    case GP100_SYSEX_DOMAIN_USER_AMP: {
        gp100_sysex_user_amp_record_t record;

        if (hooks == NULL || hooks->load_user_amp == NULL) {
            return GP100_SYSEX_LIVE_UNHANDLED;
        }
        if (request->payload == NULL || request->payload_length != 1u ||
            request->payload[0] >= GP100_SYSEX_USER_AMP_SLOT_COUNT)
            return GP100_SYSEX_LIVE_INVALID;
        if (!hooks->load_user_amp(
                hooks->context, request->payload[0], &record)) {
            return GP100_SYSEX_LIVE_INVALID;
        }
        domain_result = gp100_sysex_user_amp_build_descriptor(
            request, &record, response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_USER_AMP_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_USER_AMP_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    case GP100_SYSEX_DOMAIN_CABINET: {
        uint8_t *record;

        if (hooks == NULL || hooks->session == NULL ||
            hooks->load_cabinet == NULL)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (request->payload == NULL || request->payload_length != 1u ||
            request->payload[0] >= GP100_CABINET_USER_COUNT)
            return GP100_SYSEX_LIVE_INVALID;
        record = hooks->session->cabinet_completed_record;
        if (!hooks->load_cabinet(
                hooks->context, request->payload[0], record))
            return GP100_SYSEX_LIVE_INVALID;
        domain_result = gp100_sysex_cabinet_build_info(
            request, record, response, response_capacity, response_length);
        return domain_result == GP100_SYSEX_CABINET_RESPONSE
            ? GP100_SYSEX_LIVE_RESPONSE
            : (domain_result == GP100_SYSEX_CABINET_INVALID
                ? GP100_SYSEX_LIVE_INVALID
                : GP100_SYSEX_LIVE_UNHANDLED);
    }
    default:
        return GP100_SYSEX_LIVE_UNHANDLED;
    }
}

static bool system_action_field(
    gp100_sysex_system_action_kind_t kind,
    gp100_settings_field_t *field)
{
    switch (kind) {
    case GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE:
        *field = GP100_SETTINGS_FOOTSWITCH_MODE;
        return true;
    case GP100_SYSEX_SYSTEM_SET_LANGUAGE:
        *field = GP100_SETTINGS_LANGUAGE_INDEX;
        return true;
    case GP100_SYSEX_SYSTEM_SET_MASTER_VOLUME:
        *field = GP100_SETTINGS_MASTER_VOLUME;
        return true;
    case GP100_SYSEX_SYSTEM_SET_INPUT_LEVEL:
        *field = GP100_SETTINGS_INPUT_LEVEL_DB;
        return true;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_LEVEL:
        *field = GP100_SETTINGS_USB_RECORD_LEVEL_DB;
        return true;
    case GP100_SYSEX_SYSTEM_SET_USB_MONITOR_LEVEL:
        *field = GP100_SETTINGS_USB_MONITOR_LEVEL_DB;
        return true;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_LEFT:
        *field = GP100_SETTINGS_USB_RECORD_SOURCE_LEFT;
        return true;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_RIGHT:
        *field = GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT;
        return true;
    default:
        return false;
    }
}

static int16_t system_state_value(
    const gp100_sysex_system_state_t *state,
    gp100_sysex_system_action_kind_t kind)
{
    switch (kind) {
    case GP100_SYSEX_SYSTEM_SET_FOOTSWITCH_MODE:
        return state->footswitch_mode;
    case GP100_SYSEX_SYSTEM_SET_LANGUAGE:
        return (int16_t)state->language_identifier;
    case GP100_SYSEX_SYSTEM_SET_MASTER_VOLUME:
        return state->master_volume;
    case GP100_SYSEX_SYSTEM_SET_INPUT_LEVEL:
        return state->input_level_db;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_LEVEL:
        return state->usb_record_level_db;
    case GP100_SYSEX_SYSTEM_SET_USB_MONITOR_LEVEL:
        return state->usb_monitor_level_db;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_LEFT:
        return state->usb_record_source_left;
    case GP100_SYSEX_SYSTEM_SET_USB_RECORD_SOURCE_RIGHT:
        return state->usb_record_source_right;
    default:
        return 0;
    }
}

static bool apply_effect_action(
    gp100_preset_record_t *preset,
    const gp100_sysex_effect_action_t *action)
{
    uint32_t active_algorithm_id;
    uint16_t changed_slots;

    switch (action->kind) {
    case GP100_SYSEX_EFFECT_ENABLE:
        return gp100_preset_set_effect_enabled(
            preset, action->family, action->enabled);
    case GP100_SYSEX_EFFECT_ALGORITHM:
        return gp100_preset_select_algorithm(
            preset, action->family, action->algorithm_index);
    case GP100_SYSEX_EFFECT_PARAMETER:
        return gp100_preset_algorithm_id(
                   preset, action->family, &active_algorithm_id) &&
               active_algorithm_id == action->algorithm_id &&
               gp100_preset_set_parameter(
                   preset, action->family, action->parameter_index,
                   action->value, &changed_slots);
    default:
        return false;
    }
}

static bool download_address_matches(bool selected, uint32_t address)
{
    if (selected) {
        return address == GP100_SYSEX_SELECTED_START ||
               address == GP100_SYSEX_SELECTED_ADVANCE;
    }
    return address == GP100_SYSEX_CURRENT_START ||
           address == GP100_SYSEX_CURRENT_ADVANCE;
}

static gp100_sysex_live_status_t handle_write(
    const gp100_sysex_view_t *request,
    gp100_sysex_domain_t domain,
    gp100_firmware_runtime_t *runtime,
    const gp100_sysex_live_hooks_t *hooks,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    gp100_sysex_live_result_t *result)
{
    gp100_firmware_runtime_t next = *runtime;

    if (hooks != NULL && hooks->session != NULL) {
        gp100_preset_record_t uploaded;
        uint16_t uploaded_index;
        gp100_sysex_transfer_result_t upload_status = gp100_patch_is_native(&runtime->preset)
            ? GP100_SYSEX_TRANSFER_UNHANDLED : gp100_sysex_transfer_handle_upload(
                &hooks->session->preset_upload, request,
                &uploaded, &uploaded_index,
                response, response_capacity, response_length);

        if (upload_status == GP100_SYSEX_TRANSFER_DONE) {
            next.preset = uploaded;
            next.ui.logical_preset = uploaded_index;
            if (!rebuild_preset_consumers(&next, result) ||
                !gp100_settings_set_i16(
                    &next.settings, GP100_SETTINGS_CURRENT_PRESET,
                    (int16_t)uploaded_index))
                return GP100_SYSEX_LIVE_INVALID;
            next.settings_dirty = true;
            result->preset_loaded = true;
            result->settings_changed = true;
            *runtime = next;
            return GP100_SYSEX_LIVE_RESPONSE;
        }
        if (upload_status == GP100_SYSEX_TRANSFER_RESPONSE ||
            upload_status == GP100_SYSEX_TRANSFER_REJECTED) {
            return GP100_SYSEX_LIVE_RESPONSE;
        }
        if (upload_status == GP100_SYSEX_TRANSFER_INVALID)
            return GP100_SYSEX_LIVE_INVALID;

        if (domain == GP100_SYSEX_DOMAIN_CABINET) {
            uint8_t user_index;
            gp100_sysex_cabinet_result_t cabinet_status =
                gp100_sysex_cabinet_handle_upload(
                    &hooks->session->cabinet_upload, request,
                    hooks->session->cabinet_completed_record, &user_index,
                    response, response_capacity, response_length);

            if (cabinet_status == GP100_SYSEX_CABINET_DONE) {
                result->cabinet_record_ready = true;
                result->cabinet_reload_requested = true;
                result->cabinet_user_index = user_index;
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            if (cabinet_status == GP100_SYSEX_CABINET_RESPONSE ||
                cabinet_status == GP100_SYSEX_CABINET_REJECTED) {
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            if (cabinet_status == GP100_SYSEX_CABINET_INVALID)
                return GP100_SYSEX_LIVE_INVALID;
        }
        if (domain == GP100_SYSEX_DOMAIN_USER_AMP) {
            uint8_t user_index;
            gp100_sysex_user_amp_result_t amp_status =
                gp100_sysex_user_amp_handle_upload(
                    &hooks->session->user_amp_upload, request,
                    hooks->session->user_amp_completed_package, &user_index,
                    response, response_capacity, response_length);

            if (amp_status == GP100_SYSEX_USER_AMP_DONE) {
                result->user_amp_package_ready = true;
                result->user_amp_reload_requested = true;
                result->user_amp_index = user_index;
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            if (amp_status == GP100_SYSEX_USER_AMP_RESPONSE ||
                amp_status == GP100_SYSEX_USER_AMP_REJECTED) {
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            if (amp_status == GP100_SYSEX_USER_AMP_INVALID)
                return GP100_SYSEX_LIVE_INVALID;
        }
    }

    if (domain == GP100_SYSEX_DOMAIN_CABINET) {
        gp100_sysex_cabinet_metadata_action_t metadata;
        gp100_sysex_cabinet_reset_action_t reset;
        gp100_sysex_cabinet_result_t cabinet_status;
        uint8_t *record;

        if (hooks == NULL || hooks->session == NULL ||
            hooks->load_cabinet == NULL) {
            return GP100_SYSEX_LIVE_UNHANDLED;
        }
        record = hooks->session->cabinet_completed_record;
        cabinet_status = gp100_sysex_cabinet_decode_metadata_update(
            request, &metadata);
        if (cabinet_status == GP100_SYSEX_CABINET_ACTION) {
            if (!hooks->load_cabinet(
                    hooks->context, (uint8_t)metadata.user_index, record) ||
                !gp100_sysex_cabinet_apply_metadata(
                    record, &metadata)) {
                return GP100_SYSEX_LIVE_INVALID;
            }
            result->cabinet_user_index = (uint8_t)metadata.user_index;
            result->cabinet_record_ready = true;
            return GP100_SYSEX_LIVE_ACTION;
        }
        if (cabinet_status == GP100_SYSEX_CABINET_INVALID)
            return GP100_SYSEX_LIVE_INVALID;

        cabinet_status = gp100_sysex_cabinet_decode_reset(
            request, &reset);
        if (cabinet_status == GP100_SYSEX_CABINET_ACTION) {
            if (!hooks->load_cabinet(
                    hooks->context, (uint8_t)reset.user_index, record) ||
                !gp100_sysex_cabinet_apply_reset(record, &reset)) {
                return GP100_SYSEX_LIVE_INVALID;
            }
            result->cabinet_user_index = (uint8_t)reset.user_index;
            result->cabinet_record_ready = true;
            result->cabinet_reload_requested = true;
            return GP100_SYSEX_LIVE_ACTION;
        }
        if (cabinet_status == GP100_SYSEX_CABINET_INVALID)
            return GP100_SYSEX_LIVE_INVALID;
    }

    if (domain == GP100_SYSEX_DOMAIN_USER_AMP) {
        gp100_sysex_user_amp_name_action_t name;
        gp100_sysex_user_amp_reset_action_t reset;
        gp100_sysex_user_amp_record_t record;
        gp100_sysex_user_amp_result_t amp_status;

        if (hooks == NULL || hooks->session == NULL)
            return GP100_SYSEX_LIVE_UNHANDLED;
        amp_status = gp100_sysex_user_amp_decode_name_update(
            request, &name);
        if (amp_status == GP100_SYSEX_USER_AMP_ACTION) {
            if (hooks->load_user_amp == NULL ||
                !hooks->load_user_amp(
                    hooks->context, name.user_index, &record) ||
                !gp100_sysex_user_amp_apply_name(&record, &name)) {
                return GP100_SYSEX_LIVE_INVALID;
            }
            hooks->session->user_amp_completed_record = record;
            result->user_amp_index = name.user_index;
            result->user_amp_descriptor_ready = true;
            return GP100_SYSEX_LIVE_ACTION;
        }
        if (amp_status == GP100_SYSEX_USER_AMP_INVALID)
            return GP100_SYSEX_LIVE_INVALID;

        amp_status = gp100_sysex_user_amp_decode_reset(request, &reset);
        if (amp_status == GP100_SYSEX_USER_AMP_ACTION) {
            if (!gp100_sysex_user_amp_apply_reset(&record, &reset))
                return GP100_SYSEX_LIVE_INVALID;
            hooks->session->user_amp_completed_record = record;
            result->user_amp_index = reset.user_index;
            result->user_amp_descriptor_ready = true;
            result->user_amp_reload_requested = true;
            return GP100_SYSEX_LIVE_ACTION;
        }
        if (amp_status == GP100_SYSEX_USER_AMP_INVALID)
            return GP100_SYSEX_LIVE_INVALID;
    }

    if (domain == GP100_SYSEX_DOMAIN_CONTROL) {
        gp100_sysex_control_action_t action;
        gp100_sysex_control_result_t status =
            gp100_sysex_control_decode_update(request, &action);
        if (status == GP100_SYSEX_CONTROL_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_CONTROL_ACTION ||
            !gp100_sysex_control_apply(&next.preset, &action) ||
            !rebuild_preset_consumers(&next, result))
            return GP100_SYSEX_LIVE_INVALID;
    } else if (domain == GP100_SYSEX_DOMAIN_PERFORMANCE) {
        gp100_sysex_performance_action_t action;
        gp100_sysex_performance_live_result_t live;
        gp100_sysex_performance_result_t status =
            gp100_sysex_performance_decode_update(request, &action);
        if (status == GP100_SYSEX_PERFORMANCE_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_PERFORMANCE_ACTION ||
            !gp100_sysex_performance_apply_live(
                &next.settings, &next.looper, &next.rhythm,
                next.ui.drum_enabled, &action, &live))
            return GP100_SYSEX_LIVE_INVALID;
        next.settings_dirty = true;
        result->settings_changed = true;
        result->looper_changed = live.looper_changed;
        result->rhythm_changed = live.rhythm_changed;
        result->audio_changed = live.looper_changed || live.rhythm_changed;
    } else if (domain == GP100_SYSEX_DOMAIN_SYSTEM) {
        gp100_sysex_system_action_t action;
        gp100_sysex_system_state_t system;
        gp100_language_catalog_t languages;
        gp100_settings_field_t field;
        bool changed;
        gp100_sysex_system_result_t status =
            gp100_sysex_system_decode_update(request, &action);
        if (status == GP100_SYSEX_SYSTEM_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_SYSTEM_UPDATE)
            return GP100_SYSEX_LIVE_INVALID;
        if (action.kind == GP100_SYSEX_SYSTEM_REQUEST_FACTORY_RESET) {
            result->factory_reset_requested = true;
            return GP100_SYSEX_LIVE_ACTION;
        }
        if (action.kind == GP100_SYSEX_SYSTEM_RECALL_PRESET) {
            gp100_preset_recall_result_t recall;
            gp100_volume_pedal_position_t volume_position;
            if (hooks == NULL || hooks->load_preset == NULL)
                return GP100_SYSEX_LIVE_UNHANDLED;
            if (!gp100_preset_runtime_load_and_recall(
                    action.logical_preset, hooks->load_preset,
                    hooks->context, &next.catalog, &next.settings, &next.preset,
                    &next.ui, &next.chain, &next.effects, &next.output,
                    &recall) ||
                !gp100_looper_set_tempo(
                    &next.looper, next.looper.sample_rate_hz,
                    gp100_preset_tempo_bpm(&next.preset)) ||
                !gp100_preset_volume_pedal_position(
                    &next.preset, &volume_position) ||
                !gp100_firmware_runtime_set_volume_pedal(
                    &next, (float)next.volume_pedal_percent,
                    volume_position) ||
                !gp100_settings_set_i16(
                    &next.settings, GP100_SETTINGS_CURRENT_PRESET,
                    (int16_t)action.logical_preset))
                return GP100_SYSEX_LIVE_INVALID;
            next.preset_modified = false;
            next.settings_dirty = true;
            result->preset_changed = true;
            result->preset_loaded = true;
            result->settings_changed = true;
            result->audio_changed = true;
            *runtime = next;
            return GP100_SYSEX_LIVE_APPLIED;
        }
        gp100_language_catalog_from_mask(0u, &languages);
        if (!system_state_from_runtime(&next, &system) ||
            !system_action_field(action.kind, &field) ||
            !gp100_sysex_system_apply(
                &system, &languages, &action, &changed))
            return GP100_SYSEX_LIVE_INVALID;
        if (changed) {
            if (!gp100_settings_set_i16(
                    &next.settings, field,
                    system_state_value(&system, action.kind)))
                return GP100_SYSEX_LIVE_INVALID;
            gp100_settings_update_crc(&next.settings);
            next.settings_dirty = true;
            result->settings_changed = true;
        }
    } else if (domain == GP100_SYSEX_DOMAIN_GLOBAL) {
        gp100_sysex_global_state_t global;
        gp100_sysex_global_update_t update;
        uint32_t previous_bits;
        gp100_sysex_global_result_t status =
            gp100_sysex_global_decode_update(request, &update);
        if (status == GP100_SYSEX_GLOBAL_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_GLOBAL_UPDATE ||
            !gp100_settings_get_global_eq(&next.settings, &global))
            return GP100_SYSEX_LIVE_INVALID;
        previous_bits = global.parameters[(size_t)update.parameter];
        if (!gp100_sysex_global_apply_update(
                &global, &update, NULL))
            return GP100_SYSEX_LIVE_INVALID;
        if (previous_bits != global.parameters[(size_t)update.parameter]) {
            if (!gp100_settings_set_global_eq(
                    &next.settings, &global))
                return GP100_SYSEX_LIVE_INVALID;
            gp100_settings_update_crc(&next.settings);
            next.settings_dirty = true;
            result->settings_changed = true;
        }
    } else if (domain == GP100_SYSEX_DOMAIN_EFFECT) {
        gp100_sysex_effect_action_t action;
        gp100_sysex_effect_result_t status =
            gp100_sysex_effect_decode_command(request, &action);
        if (status == GP100_SYSEX_EFFECT_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_EFFECT_ACTION ||
            !apply_effect_action(&next.preset, &action) ||
            !rebuild_preset_consumers(&next, result))
            return GP100_SYSEX_LIVE_INVALID;
        if (next.ui.tap_tempo_enabled &&
            gp100_effect_runtime_tap_sync_algorithm_changed(&runtime->preset, &next.preset)) {
            next.ui.tap_tempo_enabled = false;
        }
    } else if (domain == GP100_SYSEX_DOMAIN_PRESET) {
        gp100_sysex_preset_action_t action;
        gp100_sysex_preset_result_t status =
            gp100_sysex_preset_decode_update(request, &action);
        if (status == GP100_SYSEX_PRESET_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_PRESET_ACTION ||
            !gp100_sysex_preset_apply(&next.preset, &action) ||
            !rebuild_preset_consumers(&next, result))
            return GP100_SYSEX_LIVE_INVALID;
    } else if (domain == GP100_SYSEX_DOMAIN_GENRE) {
        gp100_genre_catalog_t next_catalog;
        gp100_sysex_genre_update_t update;
        gp100_sysex_genre_result_t status;

        if (hooks == NULL || hooks->session == NULL)
            return GP100_SYSEX_LIVE_UNHANDLED;
        status = gp100_sysex_genre_decode_update(request, &update);
        if (status == GP100_SYSEX_GENRE_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        next_catalog = hooks->session->genre_catalog;
        if (status != GP100_SYSEX_GENRE_UPDATE ||
            !gp100_sysex_genre_apply(
                &next_catalog, &next.preset, &update) ||
            !rebuild_preset_consumers(&next, result))
            return GP100_SYSEX_LIVE_INVALID;
        hooks->session->genre_catalog = next_catalog;
        result->genre_changed = true;
    } else if (domain == GP100_SYSEX_DOMAIN_TRANSFER) {
        if (hooks != NULL && hooks->session != NULL &&
            hooks->session->preset_download.data != NULL &&
            download_address_matches(
                hooks->session->preset_download_selected,
                request->address)) {
            gp100_sysex_transfer_result_t transfer_status =
                gp100_sysex_transfer_handle_download(
                    &hooks->session->preset_download, request,
                    response, response_capacity, response_length);

            if (transfer_status == GP100_SYSEX_TRANSFER_DONE) {
                hooks->session->preset_download_active = false;
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            if (transfer_status == GP100_SYSEX_TRANSFER_RESPONSE) {
                return GP100_SYSEX_LIVE_RESPONSE;
            }
            return transfer_status == GP100_SYSEX_TRANSFER_UNHANDLED
                ? GP100_SYSEX_LIVE_UNHANDLED
                : GP100_SYSEX_LIVE_INVALID;
        }
        return GP100_SYSEX_LIVE_UNHANDLED;
    } else if (domain == GP100_SYSEX_DOMAIN_DATA) {
        gp100_sysex_save_preset_action_t action;
        gp100_sysex_data_result_t status;

        status =
            gp100_sysex_data_decode_save_preset(request, &action);
        if (status == GP100_SYSEX_DATA_UNHANDLED)
            return GP100_SYSEX_LIVE_UNHANDLED;
        if (status != GP100_SYSEX_DATA_ACTION)
            return GP100_SYSEX_LIVE_INVALID;
        result->save_user_preset_requested = true;
        result->save_user_preset_index = action.user_index;
        return GP100_SYSEX_LIVE_ACTION;
    } else {
        return GP100_SYSEX_LIVE_UNHANDLED;
    }
    *runtime = next;
    return GP100_SYSEX_LIVE_APPLIED;
}

gp100_sysex_live_status_t gp100_sysex_live_handle(
    gp100_firmware_runtime_t *runtime,
    const gp100_sysex_live_hooks_t *hooks,
    const uint8_t *message,
    size_t message_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    gp100_sysex_live_result_t *result)
{
    gp100_sysex_view_t request;
    gp100_sysex_domain_t domain;
    gp100_sysex_live_status_t status;

    if (runtime == NULL || message == NULL || response_length == NULL ||
        result == NULL || (response_capacity != 0u && response == NULL)) {
        return GP100_SYSEX_LIVE_INVALID;
    }
    *response_length = 0u;
    memset(result, 0, sizeof(*result));
    if (!gp100_sysex_route(
            message, message_length, &request, &domain)) {
        return GP100_SYSEX_LIVE_INVALID;
    }
    result->domain = domain;
    if (domain == GP100_SYSEX_DOMAIN_UNKNOWN ||
        (gp100_patch_is_native(&runtime->preset) &&
         (domain == GP100_SYSEX_DOMAIN_EFFECT || domain == GP100_SYSEX_DOMAIN_PRESET ||
          domain == GP100_SYSEX_DOMAIN_CONTROL))) {
        return GP100_SYSEX_LIVE_UNHANDLED;
    }
    if (request.message_class == GP100_SYSEX_REQUEST_CLASS) {
        status = handle_read(
            &request, domain, runtime, hooks, response, response_capacity,
            response_length);
    } else if (request.message_class == GP100_SYSEX_RESPONSE_CLASS) {
        status = handle_write(
            &request, domain, runtime, hooks,
            response, response_capacity, response_length, result);
    } else {
        status = GP100_SYSEX_LIVE_UNHANDLED;
    }
    if (status != GP100_SYSEX_LIVE_RESPONSE) {
        *response_length = 0u;
    }
    return status;
}
