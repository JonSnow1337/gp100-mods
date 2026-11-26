#ifndef GP100_VENDOR_SYSEX_LIVE_H
#define GP100_VENDOR_SYSEX_LIVE_H

#include "app/firmware_runtime.h"
#include "vendor_sysex.h"
#include "vendor_sysex_cabinet.h"
#include "vendor_sysex_data.h"
#include "vendor_sysex_genre.h"
#include "vendor_sysex_transfer.h"
#include "vendor_sysex_user_amp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_sysex_live_status {
    GP100_SYSEX_LIVE_INVALID = -1,
    GP100_SYSEX_LIVE_UNHANDLED = 0,
    GP100_SYSEX_LIVE_APPLIED = 1,
    GP100_SYSEX_LIVE_RESPONSE = 2,
    GP100_SYSEX_LIVE_ACTION = 3
} gp100_sysex_live_status_t;

typedef struct gp100_sysex_live_result {
    gp100_sysex_domain_t domain;
    bool preset_changed;
    bool settings_changed;
    bool looper_changed;
    bool rhythm_changed;
    bool audio_changed;
    bool genre_changed;
    bool preset_loaded;
    bool save_user_preset_requested;
    bool factory_reset_requested;
    bool cabinet_record_ready;
    bool cabinet_reload_requested;
    uint8_t cabinet_user_index;
    bool user_amp_package_ready;
    bool user_amp_descriptor_ready;
    bool user_amp_reload_requested;
    uint8_t user_amp_index;
    uint16_t save_user_preset_index;
} gp100_sysex_live_result_t;

typedef struct gp100_sysex_live_session {
    uint8_t preset_download_data[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    gp100_sysex_transfer_t preset_download;
    uint8_t preset_upload_data[GP100_SYSEX_PRESET_TRANSFER_BYTES];
    gp100_sysex_upload_t preset_upload;
    gp100_genre_catalog_t genre_catalog;
    uint8_t cabinet_upload_data[GP100_SYSEX_CABINET_UPLOAD_ENCODED_BYTES];
    uint8_t cabinet_completed_record[GP100_CABINET_RECORD_SIZE];
    gp100_sysex_cabinet_upload_t cabinet_upload;
    uint8_t user_amp_upload_data[GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES];
    uint8_t user_amp_completed_package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES];
    gp100_sysex_user_amp_record_t user_amp_completed_record;
    gp100_sysex_user_amp_upload_t user_amp_upload;
    uint16_t selected_logical_preset;
    bool selected_logical_preset_valid;
    bool preset_download_active;
    bool preset_download_selected;
} gp100_sysex_live_session_t;

typedef bool (*gp100_user_amp_loader_t)(
    void *context,
    uint8_t user_index,
    gp100_sysex_user_amp_record_t *record);

typedef bool (*gp100_cabinet_record_loader_t)(
    void *context,
    uint8_t user_index,
    uint8_t record[GP100_CABINET_RECORD_SIZE]);

typedef struct gp100_sysex_live_hooks {
    gp100_preset_loader_t load_preset;
    gp100_cabinet_record_loader_t load_cabinet;
    gp100_user_amp_loader_t load_user_amp;
    void *context;
    gp100_sysex_live_session_t *session;
    /* Author and notes reads use the exact stored slot. */
    gp100_preset_loader_t load_stored_preset;
} gp100_sysex_live_hooks_t;

void gp100_sysex_live_session_reset(
    gp100_sysex_live_session_t *session);
bool gp100_sysex_live_session_select_preset(
    gp100_sysex_live_session_t *session,
    uint16_t logical_preset);

/*
 * Apply or answer the live protocol pages whose complete state is owned by a
 * firmware runtime. The user-slot save command returns a caller-owned ACTION.
 * A caller-owned session serves current/selected preset transfers without
 * blocking between acknowledged chunks. A valid upload transactionally
 * becomes the live preset; persistence still requires the explicit Save
 * command. Save and factory-reset requests are returned as typed actions so
 * destructive storage policy remains caller-owned. User-genre names live in
 * the caller-owned session. Full cabinet uploads are assembled there and
 * returned as a validated record-ready result. Metadata edits and resets load
 * and mutate the same session record, but persistence remains deferred.
 * User-AMP packages use the same contract; name edits and resets return
 * a separate descriptor-ready result so the opaque blob is not overwritten.
 * Mutations publish only on success.
 */
gp100_sysex_live_status_t gp100_sysex_live_handle(
    gp100_firmware_runtime_t *runtime,
    const gp100_sysex_live_hooks_t *hooks,
    const uint8_t *message,
    size_t message_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    gp100_sysex_live_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
