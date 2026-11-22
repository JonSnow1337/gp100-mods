#include "application.h"

#include "board/input/analog_acquisition.h"
#include "assets/ui/iq_boot_logo.h"
#include "audio/engine/audio_binding.h"
#include "audio/transport/audio_transport.h"
#include "board/board.h"
#include "board/input/control_input.h"
#include "display/display_transport.h"
#include "audio/codec/codec_startup.h"
#include "display/gp100_display.h"
#include "protocol/midi/midi_input.h"
#include "board/peripherals.h"
#include "board/input/tuner_capture.h"

#include "assets/ui/development_font.h"
#include "audio/effects/cabinet/cabinet.h"
#include "storage/expression_calibration_record.h"
#include "input/expression_runtime.h"
#include "storage/flash_update.h"
#include "storage/patch_chain.h"
#include "storage/preset_store.h"
#include "storage/settings_store.h"
#include "ui/firmware_renderer.h"
#include "storage/genre_record.h"
#include "ui/screens/language_screen.h"
#include "input/analog_control.h"
#include "protocol/midi/midi_runtime.h"
#include "protocol/midi/midi_stream.h"
#include "protocol/midi/sysex/vendor_sysex_live.h"
#include "protocol/midi/sysex/vendor_sysex_user_amp.h"

#include "fsl_common.h"

#include <string.h>

#define GP100_FLEXSPI_MAPPED_BASE UINT32_C(0x60000000)
#define GP100_EXPRESSION_FALLBACK_HEEL 0u
#define GP100_EXPRESSION_FALLBACK_TOE 239u
#define GP100_EXPRESSION_FALLBACK_TRIGGER 240u
#define GP100_TUNER_SIGNAL_WINDOW_MS 150u
#define GP100_TUNER_SIGNAL_MINIMUM_EDGES 6u
#define GP100_DISPLAY_RETRY_DELAY_MS 20u
#define GP100_BOOT_LOGO_FADE_STEPS 32u
#define GP100_BOOT_LOGO_FRAME_MS 40u
#define GP100_BOOT_LOGO_HOLD_MS 2000u
#define GP100_BOOT_LOGO_BLACK_MS 80u
#define GP100_BOOT_LOGO_BACKGROUND_BGR888 UINT32_C(0x00f5f1ec)
#define GP100_IQ_LOGO_INK_BGR888 UINT32_C(0x006e5a3a)

_Static_assert(GP100_IQ_LOGO_WIDTH <= GP100_DISPLAY_HEIGHT &&
    GP100_IQ_LOGO_HEIGHT <= GP100_DISPLAY_WIDTH, "IQ logo fits landscape");

/* The factory capacity occupies about 15.3 MiB. It is NOLOAD and need not be
 * cleared at startup because looper state exposes no samples until recorded. */
GP100_SDRAM_NOINIT_SECTION_ALIGN(
    static int32_t s_looperStorage[GP100_LOOPER_FACTORY_STORAGE_SAMPLES],
    32);
GP100_SDRAM_NOINIT_SECTION_ALIGN(
    static int32_t s_monoOverdubBackup[GP100_LOOPER_MONO_BACKUP_SAMPLES],
    32);

static gp100_firmware_runtime_t s_runtime;
static gp100_application_status_t s_status;
static gp100_display_framebuffer_t s_framebuffer;
static gp100_display_hooks_t s_displayHooks;
static gp100_display_canvas_t s_canvas;
static gp100_firmware_renderer_t s_renderer;
static const gp100_bitmap_font_t *s_font;
static bool s_displayRetryPending;
static uint32_t s_displayFailedAtMs;
static gp100_analog_control_t s_analogControl;
static gp100_event_router_t s_eventRouter;
static volatile uint16_t s_expressionValue;
static volatile uint16_t s_hardwareTestTunerValue;
static gp100_midi_stream_t s_midiStream;
static gp100_midi_runtime_t s_midiRuntime;
static uint8_t s_midiMessage[GP100_MIDI_STREAM_CAPACITY];
static uint8_t s_midiResponse[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
static size_t s_midiResponseSize;
GP100_SDRAM_BSS_SECTION_ALIGN(
    static gp100_sysex_live_session_t s_sysexSession, 32);
static uint32_t s_sysexUiUpdates;
static uint16_t s_sysexSaveUserIndex;
static bool s_sysexSavePending;
static bool s_sysexGenrePending;
static bool s_factoryResetPending;
static uint8_t s_sysexCabinetUserIndex;
static bool s_sysexCabinetPending;
static bool s_sysexCabinetReloadPending;
static uint8_t s_sysexUserAmpIndex;
static uint8_t s_sysexUserAmpDescriptorIndex;
static bool s_sysexUserAmpPending;
static bool s_sysexUserAmpDescriptorPending;
static bool s_sysexUserAmpReloadPending;
static bool s_sysexUserAmpDescriptorReloadPending;
static uint32_t s_tunerTickMs;
static uint32_t s_tunerSignalWindowMs;
static uint32_t s_tunerSignalWindowSequence;
static bool s_tunerSignalPresent;
static uint16_t s_hardwareTestPedalValue;
static uint32_t s_hardwareTestTickMs;
static gp100_settings_writer_t s_settingsWriter;
static gp100_settings_record_t s_settingsToSave;
static bool s_audioRhythmEnabled;
static gp100_genre_writer_t s_genreWriter;
static gp100_expression_calibration_writer_t s_calibrationWriter;
static gp100_flash_update_hooks_t s_flashHooks;
static uint8_t s_flashWorkspace[GP100_FLASH_SECTOR_SIZE];
static uint32_t s_lastSettingsServiceMs;
static bool s_persistenceConfigured;

static bool GP100_ApplicationUpdateFlash(uint32_t address,
                                        const uint8_t *data, size_t size)
{
    if (!s_persistenceConfigured || !GP100_AudioTransportBeginFlashWrite())
    {
        return false;
    }
    const gp100_flash_update_result_t result = gp100_flash_update(
        &s_flashHooks, s_flashWorkspace, sizeof(s_flashWorkspace),
        address, data, size);
    GP100_AudioTransportEndFlashWrite();
    return result == GP100_FLASH_UPDATE_OK;
}

static const uint8_t *GP100_FlashPointer(uint32_t offset, size_t size)
{
    if (offset > BOARD_FLASH_SIZE || size > BOARD_FLASH_SIZE - offset)
    {
        return NULL;
    }
    return (const uint8_t *)(uintptr_t)(GP100_FLEXSPI_MAPPED_BASE + offset);
}

static bool GP100_LoadStoredPreset(void *context,
                                   uint16_t logicalPreset,
                                   gp100_preset_record_t *preset)
{
    (void)context;
    return gp100_preset_store_load(
        GP100_FlashPointer(GP100_PRESET_STORE_BASE, GP100_PRESET_STORE_BYTES),
        GP100_FlashPointer(GP100_USER_PRESET_BASE, GP100_PRESET_LEGACY_BYTES),
        logicalPreset, preset);
}

static bool GP100_LoadPreset(void *context,
                             uint16_t logicalPreset,
                             gp100_preset_record_t *preset)
{
    gp100_application_source_t source = kGP100_ApplicationSourceFlash;
    if (preset == NULL || logicalPreset >= GP100_PRESETS_PER_BANK)
    {
        return false;
    }
    if (!GP100_LoadStoredPreset(context, logicalPreset, preset))
    {
        if (!gp100_make_development_preset(preset, "Empty")) {
            return false;
        }
        gp100_patch_init(preset);
        source = kGP100_ApplicationSourceDevelopmentFallback;
    }
    s_status.preset_source = source;
    return true;
}

static bool GP100_ApplicationWritePreset(uint32_t address,
                                         const gp100_preset_record_t *record)
{
    if (!s_persistenceConfigured || address < GP100_USER_PRESET_BASE ||
        address - GP100_USER_PRESET_BASE >= GP100_PRESET_LEGACY_BYTES ||
        (address - GP100_USER_PRESET_BASE) % GP100_PRESET_RECORD_SIZE != 0u ||
        !GP100_AudioTransportBeginFlashWrite())
    {
        return false;
    }
    const uint16_t slot = (uint16_t)((address - GP100_USER_PRESET_BASE) /
                                     GP100_PRESET_RECORD_SIZE);
    const bool saved = gp100_preset_store_save(
        GP100_FlashPointer(GP100_PRESET_STORE_BASE, GP100_PRESET_STORE_BYTES),
        GP100_FlashPointer(GP100_USER_PRESET_BASE, GP100_PRESET_LEGACY_BYTES),
        &s_flashHooks, s_flashWorkspace, slot, record);
    GP100_AudioTransportEndFlashWrite();
    return saved;
}

static bool GP100_ApplicationSaveSysExPreset(uint16_t userIndex)
{
    gp100_preset_record_t record;
    uint32_t address;

    if (!s_persistenceConfigured ||
        userIndex >= GP100_PRESETS_PER_BANK ||
        !gp100_preset_flash_address(userIndex, &address))
    {
        return false;
    }
    record = s_runtime.preset;
    if (!gp100_patch_validate(&record))
    {
        return false;
    }
    if (!GP100_ApplicationWritePreset(address, &record))
    {
        ++s_status.persistence_errors;
        return false;
    }
    gp100_firmware_runtime_mark_preset_saved(&s_runtime, address, &record);
    ++s_status.preset_writes;
    return true;
}

static bool GP100_ApplicationSaveSysExCabinet(uint8_t userIndex,
                                              bool reload)
{
    uint32_t address;

    if (!s_persistenceConfigured ||
        userIndex >= GP100_CABINET_USER_COUNT ||
        !gp100_cabinet_flash_offset(
            GP100_CABINET_FACTORY_COUNT + userIndex, &address))
    {
        return false;
    }
    if (!GP100_ApplicationUpdateFlash(
            address, s_sysexSession.cabinet_completed_record,
            sizeof(s_sysexSession.cabinet_completed_record)))
    {
        ++s_status.persistence_errors;
        return false;
    }
    if (reload && !GP100_AudioBindingInvalidateCabinet(
                      (uint16_t)(GP100_CABINET_FACTORY_COUNT +
                                 userIndex)))
    {
        ++s_status.audio_sync_errors;
    }
    ++s_status.cabinet_writes;
    return true;
}

static bool GP100_ApplicationSaveSysExUserAmp(
    uint8_t userIndex,
    bool reload)
{
    uint8_t descriptor[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES];
    const uint8_t *blob =
        s_sysexSession.user_amp_completed_package +
        GP100_SYSEX_USER_AMP_METADATA_BYTES;
    gp100_user_amp_model_layout_t model;
    uint32_t descriptorAddress;
    uint32_t blobAddress;

    if (!s_persistenceConfigured ||
        !gp100_user_amp_model_layout(
            blob, GP100_SYSEX_USER_AMP_BLOB_BYTES, &model) ||
        !gp100_user_amp_flash_offsets(
            userIndex, &descriptorAddress, &blobAddress) ||
        !gp100_user_amp_build_descriptor_record(
            userIndex, true, s_sysexSession.user_amp_completed_package,
            descriptor))
    {
        return false;
    }
    /* Publish the presence descriptor only after the validated blob is
     * durable. */
    if (!GP100_ApplicationUpdateFlash(
            blobAddress, blob, GP100_SYSEX_USER_AMP_BLOB_BYTES) ||
        !GP100_ApplicationUpdateFlash(
            descriptorAddress, descriptor, sizeof(descriptor)))
    {
        ++s_status.persistence_errors;
        return false;
    }
    if (reload && !GP100_AudioBindingInvalidateUserAmp(userIndex))
    {
        ++s_status.audio_sync_errors;
    }
    ++s_status.user_amp_writes;
    return true;
}

static bool GP100_ApplicationSaveSysExUserAmpDescriptor(
    uint8_t userIndex,
    bool reload)
{
    uint8_t descriptor[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES];
    uint32_t descriptorAddress;
    uint32_t blobAddress;
    const gp100_sysex_user_amp_record_t *record =
        &s_sysexSession.user_amp_completed_record;

    if (!s_persistenceConfigured ||
        !gp100_user_amp_flash_offsets(
            userIndex, &descriptorAddress, &blobAddress) ||
        !gp100_user_amp_build_descriptor_record(
            userIndex, record->present, record->metadata, descriptor))
    {
        return false;
    }
    (void)blobAddress;
    if (!GP100_ApplicationUpdateFlash(
            descriptorAddress, descriptor, sizeof(descriptor)))
    {
        ++s_status.persistence_errors;
        return false;
    }
    if (reload && !GP100_AudioBindingInvalidateUserAmp(userIndex))
    {
        ++s_status.audio_sync_errors;
    }
    ++s_status.user_amp_writes;
    return true;
}

static bool GP100_LoadUserAmp(
    void *context,
    uint8_t userIndex,
    gp100_sysex_user_amp_record_t *record)
{
    const uint8_t *stored;
    uint32_t descriptorAddress;
    uint32_t blobAddress;

    (void)context;
    if (record == NULL ||
        !gp100_user_amp_flash_offsets(
            userIndex, &descriptorAddress, &blobAddress))
    {
        return false;
    }
    (void)blobAddress;
    stored = GP100_FlashPointer(
        descriptorAddress, GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES);
    if (stored != NULL && gp100_user_amp_decode_descriptor_record(
                              userIndex, stored, record))
    {
        return true;
    }
    return gp100_sysex_user_amp_make_factory_record(userIndex, record);
}

static bool GP100_LoadUserCabinet(
    void *context,
    uint8_t userIndex,
    uint8_t record[GP100_CABINET_RECORD_SIZE])
{
    const uint8_t *stored;
    uint32_t address;

    (void)context;
    if (record == NULL || userIndex >= GP100_CABINET_USER_COUNT ||
        !gp100_cabinet_flash_offset(
            GP100_CABINET_FACTORY_COUNT + userIndex, &address))
    {
        return false;
    }
    stored = GP100_FlashPointer(address, GP100_CABINET_RECORD_SIZE);
    if (stored == NULL)
    {
        return false;
    }
    memcpy(record, stored, GP100_CABINET_RECORD_SIZE);
    return true;
}

static bool GP100_ApplicationHandleSysEx(
    void *context,
    bool dataPath,
    const uint8_t *message,
    size_t messageLength)
{
    const gp100_sysex_live_hooks_t hooks = {
        .load_preset = GP100_LoadPreset,
        .load_cabinet = GP100_LoadUserCabinet,
        .load_user_amp = GP100_LoadUserAmp,
        .context = NULL,
        .session = &s_sysexSession,
        .load_stored_preset = GP100_LoadStoredPreset
    };
    gp100_sysex_live_result_t result;
    gp100_sysex_live_status_t status;
    gp100_application_source_t activePresetSource = s_status.preset_source;
    size_t responseSize;

    (void)context;
    s_sysexUiUpdates = GP100_UI_UPDATE_NONE;
    s_sysexSavePending = false;
    if (dataPath && s_midiResponseSize != 0u)
    {
        ++s_status.sysex_response_overruns;
        return false;
    }
    status = gp100_sysex_live_handle(
        &s_runtime, &hooks, message, messageLength,
        s_midiResponse, sizeof(s_midiResponse), &responseSize, &result);
    if (status < GP100_SYSEX_LIVE_APPLIED || !result.preset_loaded)
    {
        s_status.preset_source = activePresetSource;
    }
    if ((status == GP100_SYSEX_LIVE_RESPONSE ||
         status == GP100_SYSEX_LIVE_ACTION) &&
        result.cabinet_record_ready)
    {
        s_sysexCabinetUserIndex = result.cabinet_user_index;
        s_sysexCabinetPending = true;
        s_sysexCabinetReloadPending = result.cabinet_reload_requested;
    }
    if (status == GP100_SYSEX_LIVE_RESPONSE &&
        result.user_amp_package_ready)
    {
        s_sysexUserAmpIndex = result.user_amp_index;
        s_sysexUserAmpPending = true;
        s_sysexUserAmpReloadPending = result.user_amp_reload_requested;
    }
    if (status == GP100_SYSEX_LIVE_ACTION &&
        result.user_amp_descriptor_ready)
    {
        s_sysexUserAmpDescriptorIndex = result.user_amp_index;
        s_sysexUserAmpDescriptorPending = true;
        s_sysexUserAmpDescriptorReloadPending =
            result.user_amp_reload_requested;
    }
    if (status == GP100_SYSEX_LIVE_RESPONSE)
    {
        s_midiResponseSize = responseSize;
        ++s_status.sysex_reads;
        if (result.preset_changed)
        {
            s_sysexUiUpdates |= result.preset_loaded
                ? GP100_UI_UPDATE_PRESET_LOAD
                : GP100_UI_UPDATE_PRESET;
            s_sysexUiUpdates |= GP100_UI_UPDATE_REDRAW;
            if (result.preset_loaded)
            {
                ++s_status.preset_loads;
            }
        }
        if (result.settings_changed)
        {
            s_sysexUiUpdates |= GP100_UI_UPDATE_SETTINGS;
        }
        return true;
    }
    if (status == GP100_SYSEX_LIVE_APPLIED)
    {
        ++s_status.sysex_writes;
        if (result.preset_changed)
        {
            s_sysexUiUpdates |= result.preset_loaded
                ? GP100_UI_UPDATE_PRESET_LOAD
                : GP100_UI_UPDATE_PRESET;
        }
        if (result.preset_loaded)
        {
            ++s_status.preset_loads;
        }
        if (result.settings_changed)
        {
            s_sysexUiUpdates |= GP100_UI_UPDATE_SETTINGS;
        }
        if (result.genre_changed)
        {
            s_sysexGenrePending = true;
        }
        if (result.looper_changed)
        {
            s_sysexUiUpdates |= GP100_UI_UPDATE_LOOPER;
        }
        if (result.rhythm_changed)
        {
            s_sysexUiUpdates |= GP100_UI_UPDATE_DRUM;
        }
        s_sysexUiUpdates |= GP100_UI_UPDATE_REDRAW;
        return true;
    }
    if (status == GP100_SYSEX_LIVE_ACTION)
    {
        if (result.save_user_preset_requested)
        {
            s_sysexSaveUserIndex = result.save_user_preset_index;
            s_sysexSavePending = true;
            return true;
        }
        if (result.factory_reset_requested)
        {
            s_factoryResetPending = true;
            ++s_status.factory_reset_requests;
            return true;
        }
        if (result.cabinet_record_ready || result.user_amp_descriptor_ready)
        {
            return true;
        }
    }
    if (status == GP100_SYSEX_LIVE_UNHANDLED)
    {
        ++s_status.sysex_unhandled;
    }
    return false;
}

static bool GP100_LoadSettings(gp100_settings_record_t *settings,
                               uint32_t *repairs)
{
    if (gp100_settings_store_load(
            GP100_FlashPointer(GP100_SETTINGS_STORE_BASE, GP100_SETTINGS_STORE_BYTES),
            GP100_FlashPointer(GP100_SETTINGS_FLASH_BASE, GP100_SETTINGS_FLASH_BYTES),
            settings))
    {
        s_status.settings_source = kGP100_ApplicationSourceFlash;
        *repairs = gp100_settings_normalize(
            settings, GP100_LANGUAGE_COUNT);
        s_status.settings_repairs = *repairs;
        return true;
    }
    s_status.settings_source = kGP100_ApplicationSourceFactoryFallback;
    if (!gp100_make_factory_settings(settings))
    {
        return false;
    }
    *repairs = gp100_settings_normalize(
        settings, GP100_LANGUAGE_COUNT);
    s_status.settings_repairs = *repairs;
    return true;
}

static uint16_t GP100_CurrentPresetFromSettings(
    const gp100_settings_record_t *settings)
{
    int16_t storedPreset;

    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_CURRENT_PRESET, &storedPreset) ||
        storedPreset < 0 ||
        storedPreset >= (int16_t)GP100_PRESETS_PER_BANK)
    {
        return 0u;
    }
    return (uint16_t)storedPreset;
}

static void GP100_LoadGenreCatalog(gp100_genre_catalog_t *catalog,
                                   size_t *nextSlot)
{
    const uint8_t *region = GP100_FlashPointer(
        GP100_GENRE_FLASH_BASE, GP100_GENRE_FLASH_BYTES);
    if (region != NULL && gp100_genre_record_find_latest(
                              region, GP100_GENRE_FLASH_BYTES,
                              catalog, NULL, nextSlot))
    {
        s_status.genre_source = kGP100_ApplicationSourceFlash;
        return;
    }
    (void)gp100_genre_catalog_make_factory(catalog);
    *nextSlot = 0u;
    s_status.genre_source = kGP100_ApplicationSourceFactoryFallback;
}

static void GP100_LoadExpressionCalibration(
    gp100_expression_calibration_values_t *calibration,
    size_t *nextSlot)
{
    const uint8_t *region = GP100_FlashPointer(
        GP100_EXPRESSION_CALIBRATION_FLASH_BASE,
        GP100_EXPRESSION_CALIBRATION_FLASH_BYTES);

    calibration->heel = GP100_EXPRESSION_FALLBACK_HEEL;
    calibration->toe = GP100_EXPRESSION_FALLBACK_TOE;
    calibration->trigger = GP100_EXPRESSION_FALLBACK_TRIGGER;
    s_status.expression_calibration_source =
        kGP100_ApplicationSourceDevelopmentFallback;
    *nextSlot = 0u;
    if (region != NULL &&
        gp100_expression_calibration_find_latest(
            region, GP100_EXPRESSION_CALIBRATION_FLASH_BYTES,
            calibration, NULL, nextSlot) &&
        gp100_expression_calibration_values_are_usable(calibration))
    {
        s_status.expression_calibration_source =
            kGP100_ApplicationSourceFlash;
    }
    else
    {
        calibration->heel = GP100_EXPRESSION_FALLBACK_HEEL;
        calibration->toe = GP100_EXPRESSION_FALLBACK_TOE;
        calibration->trigger = GP100_EXPRESSION_FALLBACK_TRIGGER;
    }
    s_status.expression_calibration_heel = calibration->heel;
    s_status.expression_calibration_toe = calibration->toe;
    s_status.expression_activation_threshold = calibration->trigger;
}

bool GP100_ApplicationInitialize(void)
{
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    gp100_expression_calibration_values_t calibration;
    uint32_t settingsRepairs;
    uint16_t initialPreset;
    size_t calibrationNextSlot;
    size_t genreNextSlot;

    memset(&s_runtime, 0, sizeof(s_runtime));
    s_settingsWriter = (gp100_settings_writer_t){0};
    memset(&s_status, 0, sizeof(s_status));
    s_displayRetryPending = false;
    s_status.persistence_configured = s_persistenceConfigured;
    memset(&s_eventRouter, 0, sizeof(s_eventRouter));
    gp100_analog_control_init(&s_analogControl);
    s_expressionValue = 0u;
    s_hardwareTestTunerValue = 0u;
    gp100_sysex_live_session_reset(&s_sysexSession);
    GP100_LoadGenreCatalog(&s_sysexSession.genre_catalog, &genreNextSlot);
    GP100_LoadExpressionCalibration(&calibration, &calibrationNextSlot);
    s_eventRouter.expression_toe = calibration.toe;
    s_eventRouter.expression_activation_threshold = calibration.trigger;
    if (!GP100_LoadSettings(&settings, &settingsRepairs))
    {
        return false;
    }
    initialPreset = GP100_CurrentPresetFromSettings(&settings);
    if (!GP100_LoadPreset(NULL, initialPreset, &preset) ||
        !gp100_genre_writer_init(&s_genreWriter, genreNextSlot) ||
        !gp100_expression_calibration_writer_init(
            &s_calibrationWriter, calibrationNextSlot) ||
        !gp100_firmware_runtime_init(
            &s_runtime, initialPreset, &preset, &settings,
            s_looperStorage,
            sizeof(s_looperStorage) / sizeof(s_looperStorage[0]),
            calibration.heel,
            calibration.toe) ||
        !gp100_looper_set_mono_overdub_backup(
            &s_runtime.looper, s_monoOverdubBackup, GP100_LOOPER_MONO_BACKUP_SAMPLES) ||
        !GP100_AudioBindingInitialize(&s_runtime))
    {
        return false;
    }
    if (settingsRepairs != GP100_SETTINGS_REPAIR_NONE)
    {
        s_runtime.settings_dirty = true;
        gp100_settings_writer_mark_dirty(&s_settingsWriter);
    }
    s_settingsToSave = s_runtime.settings;
    s_audioRhythmEnabled = s_runtime.rhythm.enabled;
    memset(&s_midiRuntime, 0, sizeof(s_midiRuntime));
    gp100_midi_stream_reset(&s_midiStream);
    s_midiResponseSize = 0u;
    s_sysexUiUpdates = GP100_UI_UPDATE_NONE;
    s_sysexSavePending = false;
    s_sysexGenrePending = false;
    s_factoryResetPending = false;
    s_sysexCabinetPending = false;
    s_sysexCabinetReloadPending = false;
    s_sysexUserAmpPending = false;
    s_sysexUserAmpDescriptorPending = false;
    s_sysexUserAmpReloadPending = false;
    s_sysexUserAmpDescriptorReloadPending = false;
    s_tunerTickMs = g_gp100_board_state.milliseconds;
    s_tunerSignalWindowMs = s_tunerTickMs;
    s_tunerSignalWindowSequence = 0u;
    s_tunerSignalPresent = false;
    s_hardwareTestPedalValue = 0u;
    s_hardwareTestTickMs = g_gp100_board_state.milliseconds;
    s_lastSettingsServiceMs = g_gp100_board_state.milliseconds;
    s_midiRuntime.controls.ui = &s_runtime.ui;
    s_midiRuntime.controls.preset = &s_runtime.preset;
    s_midiRuntime.controls.settings = &s_runtime.settings;
    s_midiRuntime.controls.looper = &s_runtime.looper;
    s_midiRuntime.controls.effects = &s_runtime.effects;
    s_midiRuntime.controls.rhythm = &s_runtime.rhythm;
    s_midiRuntime.catalog = &s_runtime.catalog;
    s_midiRuntime.chain = &s_runtime.chain;
    s_midiRuntime.output = &s_runtime.output;
    s_midiRuntime.load_preset = GP100_LoadPreset;
    s_midiRuntime.handle_sysex = GP100_ApplicationHandleSysEx;
    s_midiRuntime.context = NULL;
    gp100_midi_runtime_reset(&s_midiRuntime);
    s_status.initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitApplication;
    return true;
}

bool GP100_ApplicationSelectStartupMode(uint32_t gpio1Snapshot)
{
    if (!s_status.initialized)
    {
        return false;
    }
    return gp100_ui_runtime_set_mode(
        &s_runtime.ui, gp100_ui_startup_mode(gpio1Snapshot),
        &s_runtime.preset, &s_runtime.settings);
}

static void GP100_LoadSaveDestinationName(char *name)
{
    gp100_preset_record_t preset;

    if (name == NULL)
    {
        return;
    }
    memcpy(name, "Empty", sizeof("Empty"));
    if (GP100_LoadStoredPreset(NULL, s_runtime.ui.save.destination, &preset))
    {
        gp100_preset_name(&preset, name);
    }
}

static gp100_display_flush_result_t GP100_ApplicationFlushDisplay(void)
{
    const gp100_display_flush_result_t result = gp100_display_flush(
        &s_framebuffer, &s_displayHooks);

    if (result == GP100_DISPLAY_FLUSHED)
    {
        ++s_status.display_frames;
    }
    else if (result != GP100_DISPLAY_UNCHANGED)
    {
        ++s_status.display_errors;
    }
    return result;
}

bool GP100_ApplicationRender(void)
{
    gp100_firmware_render_context_t context;
    char destinationName[GP100_PRESET_NAME_SIZE + 1u];

    if (!s_status.initialized || !s_status.display_initialized)
    {
        return false;
    }
    s_displayRetryPending = false;
    memcpy(destinationName, "", sizeof(""));
    if (s_runtime.ui.mode == GP100_UI_MODE_SAVE)
    {
        GP100_LoadSaveDestinationName(destinationName);
    }
    memset(&context, 0, sizeof(context));
    context.firmware_version = "development";
    context.save_destination_name = destinationName;
    context.hardware_info = "GP-100";
    context.hardware_variant = "reference board";
    context.pedal_value = s_expressionValue;
    context.tuner_value = s_hardwareTestTunerValue;
    if (!gp100_firmware_renderer_render(
            &s_renderer, &s_canvas, &s_runtime, &context))
    {
        ++s_status.display_errors;
        return false;
    }
    const gp100_display_flush_result_t result =
        GP100_ApplicationFlushDisplay();
    if (result == GP100_DISPLAY_FLUSH_TRANSFER_FAILED)
    {
        s_displayFailedAtMs = g_gp100_board_state.milliseconds;
        s_displayRetryPending = true;
    }
    return result == GP100_DISPLAY_FLUSHED ||
           result == GP100_DISPLAY_UNCHANGED;
}

static void GP100_WaitForBootLogoTime(uint32_t startedAt, uint32_t elapsedMs)
{
    while ((uint32_t)(g_gp100_board_state.milliseconds - startedAt) < elapsedMs)
    {
        __WFI();
    }
}

static uint16_t GP100_BootLogoColor(uint32_t inkWeight, uint32_t brightness)
{
    const uint32_t fullWeight = GP100_BOOT_LOGO_FADE_STEPS * 15u;
    uint32_t color = 0u;

    for (uint32_t shift = 0u; shift < 24u; shift += 8u)
    {
        const uint32_t background = (GP100_BOOT_LOGO_BACKGROUND_BGR888 >> shift) & 0xffu;
        const uint32_t ink = (GP100_IQ_LOGO_INK_BGR888 >> shift) & 0xffu;
        const uint32_t component =
            (background * (fullWeight - inkWeight) + ink * inkWeight +
             fullWeight / 2u) / fullWeight;
        const uint32_t scaled =
            (component * brightness + GP100_BOOT_LOGO_FADE_STEPS / 2u) /
            GP100_BOOT_LOGO_FADE_STEPS;
        color |= scaled << shift;
    }
    return gp100_display_bgr888_to_rgb565(color);
}

static bool GP100_ApplicationFadeBootLogo(bool fadeOut)
{
    const int32_t x = (s_canvas.width - GP100_IQ_LOGO_WIDTH) / 2u;
    const int32_t y = (s_canvas.height - GP100_IQ_LOGO_HEIGHT) / 2u;
    uint16_t palette[16];
    gp100_display_flush_result_t result;
    uint32_t step = 0u;

    const uint32_t startedAt = g_gp100_board_state.milliseconds;
    for (;;)
    {
        GP100_WaitForBootLogoTime(startedAt, step * GP100_BOOT_LOGO_FRAME_MS);
        /* Use elapsed time so a slow panel transfer skips stale fade steps. */
        const uint32_t elapsed = g_gp100_board_state.milliseconds - startedAt;
        step = elapsed / GP100_BOOT_LOGO_FRAME_MS;
        if (step > GP100_BOOT_LOGO_FADE_STEPS)
        {
            step = GP100_BOOT_LOGO_FADE_STEPS;
        }
        const uint32_t inkStep = fadeOut ? GP100_BOOT_LOGO_FADE_STEPS : step;
        const uint32_t brightness = fadeOut ? GP100_BOOT_LOGO_FADE_STEPS - step
                                           : GP100_BOOT_LOGO_FADE_STEPS;
        for (uint32_t index = 0u; index < 16u; ++index)
        {
            palette[index] = GP100_BootLogoColor(inkStep * index, brightness);
        }
        if (!gp100_display_fill_rect(
                &s_canvas, 0, 0, s_canvas.width, s_canvas.height, palette[0]) ||
            !gp100_display_blit_indexed(
                &s_canvas, x, y, GP100_IQ_LOGO_WIDTH, GP100_IQ_LOGO_HEIGHT,
                gp100_iq_logo_coverage, GP100_IQ_LOGO_WIDTH / 2u, 0u, 4u, palette, 16u, false))
        {
            return false;
        }
        /* Each newer fade frame can retry an interrupted panel upload. */
        result = gp100_display_flush(&s_framebuffer, &s_displayHooks);
        if (step == GP100_BOOT_LOGO_FADE_STEPS)
        {
            break;
        }
        ++step;
    }
    if (result == GP100_DISPLAY_FLUSH_TRANSFER_FAILED)
    {
        GP100_WaitForBootLogoTime(g_gp100_board_state.milliseconds,
                                 GP100_DISPLAY_RETRY_DELAY_MS);
        result = gp100_display_flush(&s_framebuffer, &s_displayHooks);
    }
    return result >= GP100_DISPLAY_UNCHANGED;
}

static void GP100_ApplicationShowBootLogo(void)
{
    if (!GP100_ApplicationFadeBootLogo(false))
    {
        return;
    }
    GP100_WaitForBootLogoTime(g_gp100_board_state.milliseconds, GP100_BOOT_LOGO_HOLD_MS);
    if (GP100_ApplicationFadeBootLogo(true))
    {
        GP100_WaitForBootLogoTime(g_gp100_board_state.milliseconds, GP100_BOOT_LOGO_BLACK_MS);
    }
}

bool GP100_ApplicationInitializeDisplay(void)
{
    if (!s_status.initialized ||
        GP100_DisplayInitialize() != kStatus_Success)
    {
        return false;
    }
    GP100_DisplayTransportGetFramebuffer(&s_framebuffer);
    GP100_DisplayTransportGetHooks(&s_displayHooks);
    /* GP100_UI_ASSET_REPLACEMENT: default font providers for the panel. */
    s_font = gp100_development_font_5x7();
    if (s_font == NULL ||
        !gp100_display_canvas_init_rotated(
            &s_canvas, s_framebuffer.render_pixels,
            GP100_DISPLAY_WIDTH, GP100_DISPLAY_HEIGHT,
            GP100_DISPLAY_WIDTH,
            GP100_DISPLAY_ROTATION_COUNTER_CLOCKWISE) ||
        !gp100_firmware_renderer_init_with_fonts(
            &s_renderer, gp100_development_font_10x14(), s_font, s_font))
    {
        GP100_DisplaySetEnabled(false);
        return false;
    }
    GP100_ApplicationShowBootLogo();
    s_status.display_initialized = true;
    if (!GP100_ApplicationRender())
    {
        s_displayRetryPending = false;
        s_status.display_initialized = false;
        GP100_DisplaySetEnabled(false);
        return false;
    }
    return true;
}

bool GP100_ApplicationStartControls(void)
{
    if (!s_status.initialized || !GP100_ControlInputStart())
    {
        return false;
    }
    s_status.controls_running = true;
    return true;
}

void GP100_ApplicationStopControls(void)
{
    GP100_ControlInputStop();
    s_status.controls_running = false;
}

static void GP100_ApplicationAnalogPair(
    const uint16_t samples[GP100_ADC_CHANNEL_COUNT],
    uint32_t completedPairs,
    void *userData)
{
    (void)userData;
    s_hardwareTestTunerValue =
        samples[GP100_ANALOG_HARDWARE_TEST_TUNER];
    s_expressionValue = gp100_analog_control_process(
        &s_analogControl, samples);
    s_status.analog_pairs = completedPairs;
}

bool GP100_ApplicationStartAnalog(void)
{
    if (!s_status.initialized)
    {
        return false;
    }
    if (s_status.analog_running)
    {
        return true;
    }
    gp100_analog_control_init(&s_analogControl);
    s_expressionValue = 0u;
    s_hardwareTestTunerValue = 0u;
    s_status.analog_pairs = 0u;
    s_eventRouter.previous_expression_value = 0u;
    s_eventRouter.expression_activation_pending = false;
    /* A pedal already above the toe stop when we boot must release first. */
    s_eventRouter.expression_activation_latched = true;
    GP100_AnalogSetPairCallback(GP100_ApplicationAnalogPair, NULL);
    if (!GP100_AnalogStart())
    {
        GP100_AnalogSetPairCallback(NULL, NULL);
        return false;
    }
    s_status.analog_running = true;
    return true;
}

void GP100_ApplicationStopAnalog(void)
{
    GP100_AnalogStop();
    GP100_AnalogSetPairCallback(NULL, NULL);
    s_status.analog_running = false;
}

bool GP100_ApplicationStartMidi(void)
{
    if (!s_status.initialized)
    {
        return false;
    }
    if (s_status.midi_running)
    {
        return true;
    }
    gp100_midi_stream_reset(&s_midiStream);
    gp100_midi_runtime_reset(&s_midiRuntime);
    if (!GP100_MidiInputStart())
    {
        return false;
    }
    s_status.midi_running = true;
    return true;
}

void GP100_ApplicationStopMidi(void)
{
    GP100_MidiInputStop();
    s_status.midi_running = false;
}

bool GP100_ApplicationTakeMidiResponse(
    uint8_t *destination,
    size_t capacity,
    size_t *responseSize)
{
    uint32_t interruptMask;
    size_t size;

    if (responseSize == NULL)
    {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    size = s_midiResponseSize;
    *responseSize = size;
    if (size == 0u || destination == NULL || capacity < size)
    {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    memcpy(destination, s_midiResponse, size);
    s_midiResponseSize = 0u;
    EnableGlobalIRQ(interruptMask);
    return true;
}

bool GP100_ApplicationTakeFactoryResetRequest(void)
{
    uint32_t interruptMask = DisableGlobalIRQ();
    bool pending = s_factoryResetPending;

    s_factoryResetPending = false;
    EnableGlobalIRQ(interruptMask);
    return pending;
}

bool GP100_ApplicationFactoryResetBegin(size_t totalSteps)
{
    gp100_factory_reset_state_t *reset =
        &s_runtime.ui.global.factory_reset;
    bool resetRequested;
    bool exitRequested;

    if (!s_status.initialized || totalSteps == 0u)
    {
        return false;
    }
    if (reset->phase != GP100_FACTORY_RESET_REQUESTED)
    {
        gp100_factory_reset_init(reset);
        if (!gp100_factory_reset_select(reset, true) ||
            !gp100_factory_reset_activate(
                reset, &resetRequested, &exitRequested) ||
            !resetRequested || exitRequested)
        {
            return false;
        }
    }
    if (!gp100_factory_reset_begin(reset, totalSteps))
    {
        return false;
    }
    s_runtime.ui.mode = GP100_UI_MODE_GLOBAL;
    s_runtime.ui.global.view = GP100_GLOBAL_VIEW_FACTORY_RESET;
    s_runtime.ui.global.menu_item = GP100_GLOBAL_FACTORY_RESET;
    if (s_status.audio_running)
    {
        GP100_ApplicationStopAudio();
    }
    (void)GP100_ApplicationRender();
    return true;
}

bool GP100_ApplicationFactoryResetUpdate(size_t completedSteps)
{
    if (!s_status.initialized ||
        !gp100_factory_reset_update(
            &s_runtime.ui.global.factory_reset, completedSteps))
    {
        return false;
    }
    (void)GP100_ApplicationRender();
    return true;
}

static bool GP100_ApplicationApplyCurrentExpression(void)
{
    gp100_expression_apply_result_t expression;

    if (!s_status.analog_running || s_status.analog_pairs == 0u)
    {
        return true;
    }
    return gp100_expression_runtime_apply_adc(
               &s_runtime.preset, &s_runtime.effects, s_expressionValue,
               s_runtime.calibration_heel, s_runtime.calibration_toe, &expression) &&
           (!expression.volume_changed ||
            gp100_firmware_runtime_set_volume_pedal(
                &s_runtime, expression.volume_percent, expression.volume_position));
}

bool GP100_ApplicationStartAudio(void)
{
    if (!s_status.initialized ||
        (!s_status.audio_running && !GP100_ApplicationApplyCurrentExpression()) ||
        !GP100_AudioBindingSync(&s_runtime))
    {
        return false;
    }
#if defined(GP100_AUTOSTART_AUDIO)
    if (!s_status.audio_running &&
        (((g_gp100_board_state.peripheral_errors & kGP100_ErrorAudioClock) != 0u) ||
         !GP100_CodecStartup()))
    {
        return false;
    }
#endif
    if (!GP100_AudioBindingStart())
    {
        return false;
    }
    s_status.audio_running = true;
    return true;
}

void GP100_ApplicationStopAudio(void)
{
    GP100_AudioBindingStop();
    s_status.audio_running = false;
}

void GP100_ApplicationSetAudioEffectOps(
    const gp100_effect_chain_ops_t *ops)
{
    GP100_AudioBindingSetEffectOps(ops);
}

bool GP100_ApplicationStartTunerCapture(void)
{
    if (!s_status.initialized)
    {
        return false;
    }
    if (s_status.tuner_running)
    {
        return true;
    }
    if (!GP100_TunerCaptureStart())
    {
        return false;
    }
    s_tunerTickMs = g_gp100_board_state.milliseconds;
    s_tunerSignalWindowMs = s_tunerTickMs;
    s_tunerSignalWindowSequence = 0u;
    s_tunerSignalPresent = false;
    s_status.tuner_running = true;
    return true;
}

void GP100_ApplicationStopTunerCapture(void)
{
    GP100_TunerCaptureStop();
    s_status.tuner_running = false;
}

static bool GP100_ApplicationEraseSector(void *context, uint32_t address)
{
    (void)context;
    if (!s_persistenceConfigured || !GP100_AudioTransportBeginFlashWrite())
    {
        return false;
    }
    const bool erased = s_flashHooks.erase_sector(s_flashHooks.context, address);
    GP100_AudioTransportEndFlashWrite();
    return erased;
}

static bool GP100_ApplicationAppendFlash(void *context,
                                        uint32_t address,
                                        const uint8_t *data,
                                        size_t size)
{
    (void)context;
    return GP100_ApplicationUpdateFlash(address, data, size);
}

static bool GP100_ApplicationProgramErased(void *context,
                                           uint32_t address,
                                           const uint8_t *data,
                                           size_t size)
{
    (void)context;
    if (!s_persistenceConfigured || !GP100_AudioTransportBeginFlashWrite())
    {
        return false;
    }
    const gp100_flash_update_result_t result = gp100_flash_program_erased_range(
        &s_flashHooks, s_flashWorkspace, GP100_FLASH_PAGE_SIZE, address, data, size);
    GP100_AudioTransportEndFlashWrite();
    return result == GP100_FLASH_UPDATE_OK;
}

void GP100_ApplicationSetFlashHooks(
    const gp100_flash_update_hooks_t *hooks)
{
    if (hooks == NULL || hooks->read == NULL ||
        hooks->erase_sector == NULL || hooks->program_page == NULL)
    {
        memset(&s_flashHooks, 0, sizeof(s_flashHooks));
        s_persistenceConfigured = false;
        s_status.persistence_configured = false;
        return;
    }
    s_flashHooks = *hooks;
    s_persistenceConfigured = true;
    s_status.persistence_configured = true;
}

static bool GP100_ApplicationSavePreset(
    const gp100_firmware_runtime_result_t *result)
{
    if (!s_persistenceConfigured ||
        !result->live.save_record_ready)
    {
        return false;
    }
    if (!GP100_ApplicationWritePreset(result->live.save_flash_address,
                                      &result->live.save_record))
    {
        ++s_status.persistence_errors;
        return false;
    }
    gp100_firmware_runtime_mark_preset_saved(
        &s_runtime, result->live.save_flash_address, &result->live.save_record);
    ++s_status.preset_writes;
    return true;
}

static bool GP100_ApplicationSaveCalibration(void)
{
    gp100_expression_calibration_values_t values;
    gp100_expression_calibration_writer_hooks_t hooks = {
        NULL, GP100_ApplicationEraseSector, GP100_ApplicationAppendFlash
    };

    values.heel = s_runtime.ui.global.expression_calibration.heel;
    values.toe = s_runtime.ui.global.expression_calibration.toe;
    values.trigger = s_runtime.ui.global.expression_calibration.trigger;
    if (!gp100_expression_calibration_values_are_usable(&values))
    {
        ++s_status.persistence_errors;
        return false;
    }
    s_runtime.calibration_heel = values.heel;
    s_runtime.calibration_toe = values.toe;
    s_eventRouter.expression_toe = values.toe;
    s_eventRouter.expression_activation_threshold = values.trigger;
    s_eventRouter.previous_expression_value = UINT16_MAX;
    s_eventRouter.expression_activation_pending = false;
    s_eventRouter.expression_activation_latched = s_expressionValue > values.toe;
    s_status.expression_calibration_heel = values.heel;
    s_status.expression_calibration_toe = values.toe;
    s_status.expression_activation_threshold = values.trigger;
    if (!s_persistenceConfigured)
    {
        return false;
    }
    if (gp100_expression_calibration_writer_append(
            &s_calibrationWriter, &values, &hooks) !=
        GP100_EXPRESSION_CALIBRATION_WRITER_WRITTEN)
    {
        ++s_status.persistence_errors;
        return false;
    }
    s_status.expression_calibration_source = kGP100_ApplicationSourceFlash;
    ++s_status.calibration_writes;
    return true;
}

static void GP100_ApplicationQueueSettings(void)
{
    gp100_settings_record_t next = s_runtime.settings;

    /* The normal journal can save global changes without saving drum edits. */
    (void)gp100_settings_copy_drums(&next, &s_settingsToSave);
    if (memcmp(next.bytes, s_settingsToSave.bytes, GP100_SETTINGS_PAYLOAD_SIZE) != 0)
    {
        s_settingsToSave = next;
        gp100_settings_writer_mark_dirty(&s_settingsWriter);
    }
}

static gp100_settings_writer_result_t GP100_ApplicationWriteSettings(
    gp100_settings_writer_t *writer, gp100_settings_record_t *settings)
{
    const bool writing = writer->dirty_countdown == 1u;

    if (!s_persistenceConfigured ||
        (writing && !GP100_AudioTransportBeginFlashWrite()))
    {
        return GP100_SETTINGS_WRITER_IDLE;
    }
    const gp100_settings_writer_result_t result = gp100_settings_writer_service(
        writer, settings,
        GP100_FlashPointer(GP100_SETTINGS_STORE_BASE, GP100_SETTINGS_STORE_BYTES),
        &s_flashHooks);
    if (writing)
    {
        GP100_AudioTransportEndFlashWrite();
    }
    if (result == GP100_SETTINGS_WRITER_WRITTEN)
    {
        gp100_settings_update_crc(&s_runtime.settings);
        if (memcmp(settings->bytes, s_runtime.settings.bytes,
                   GP100_SETTINGS_PAYLOAD_SIZE) == 0)
        {
            gp100_firmware_runtime_mark_settings_saved(&s_runtime);
        }
        ++s_status.settings_writes;
    }
    else if (result == GP100_SETTINGS_WRITER_IO_ERROR ||
             result == GP100_SETTINGS_WRITER_INVALID)
    {
        ++s_status.persistence_errors;
    }
    return result;
}

static void GP100_ApplicationSaveChangedDrumSettings(void)
{
    gp100_settings_record_t next = s_settingsToSave;
    gp100_settings_writer_t writer = { .dirty_countdown = 1u };

    if (gp100_settings_copy_drums(&next, &s_runtime.settings) &&
        GP100_ApplicationWriteSettings(&writer, &next) == GP100_SETTINGS_WRITER_WRITTEN)
    {
        s_settingsToSave = next;
        s_settingsWriter = (gp100_settings_writer_t){0};
    }
}

static bool GP100_ApplicationSyncAudio(bool presetRecalled, bool presetEdited)
{
    if ((presetRecalled || (presetEdited && gp100_patch_is_native(&s_runtime.preset))) &&
        !GP100_ApplicationApplyCurrentExpression())
    {
        return false;
    }
    const bool rhythmChanged = s_audioRhythmEnabled != s_runtime.rhythm.enabled;
    if (rhythmChanged && s_runtime.rhythm.enabled)
    {
        GP100_ApplicationSaveChangedDrumSettings();
    }
    const bool synced = presetRecalled ? GP100_AudioBindingRecallPreset(&s_runtime)
                                       : GP100_AudioBindingSync(&s_runtime);
    if (synced)
    {
        s_audioRhythmEnabled = s_runtime.rhythm.enabled;
        if (rhythmChanged && !s_audioRhythmEnabled)
        {
            GP100_ApplicationSaveChangedDrumSettings();
        }
    }
    return synced;
}

static bool GP100_ApplicationHandleEvent(const gp100_event_t *event)
{
    gp100_firmware_runtime_result_t result;
    uint32_t interruptMask;
    bool handled;

    /* The audio ISR advances the same Looper object. Keep its control-state
     * transitions atomic with respect to one 64-frame processing block. */
    interruptMask = DisableGlobalIRQ();
    handled = gp100_firmware_runtime_handle_event(
        &s_runtime, event, GP100_LoadPreset, NULL, &result);
    EnableGlobalIRQ(interruptMask);
    if (!handled)
    {
        ++s_status.event_errors;
        return false;
    }
    ++s_status.events_handled;
    s_status.last_ui_updates = result.live.ui_updates;
    if (result.live.preset_loaded)
    {
        ++s_status.preset_loads;
    }
    if (result.settings_changed)
    {
        GP100_ApplicationQueueSettings();
    }
    if (!GP100_ApplicationSyncAudio(result.live.preset_loaded,
            (result.live.ui_updates & GP100_UI_UPDATE_PRESET) != 0u &&
            result.live.expression_status != GP100_EXPRESSION_EVENT_TARGET_TOGGLED))
    {
        ++s_status.audio_sync_errors;
    }
    if ((result.live.ui_updates & GP100_UI_UPDATE_FACTORY_RESET) != 0u)
    {
        s_factoryResetPending = true;
        ++s_status.factory_reset_requests;
    }
    if ((result.live.ui_updates & GP100_UI_UPDATE_SAVE_COMMIT) != 0u)
    {
        (void)GP100_ApplicationSavePreset(&result);
    }
    if ((result.live.ui_updates &
         GP100_UI_UPDATE_CALIBRATION_COMMIT) != 0u)
    {
        (void)GP100_ApplicationSaveCalibration();
    }
    if ((result.live.ui_updates & GP100_UI_UPDATE_REDRAW) != 0u &&
        s_status.display_initialized)
    {
        (void)GP100_ApplicationRender();
    }
    return true;
}

static void GP100_ApplicationApplyMidiResult(
    const gp100_midi_runtime_result_t *result)
{
    gp100_volume_pedal_position_t volumePosition;
    uint32_t updates = GP100_UI_UPDATE_NONE;

    if (result->action == GP100_MIDI_ACTION_CONTROL_CHANGE)
    {
        updates = result->control_change.ui_updates;
        if (result->control_change.volume_pedal_changed &&
            !gp100_firmware_runtime_set_volume_pedal(
                &s_runtime,
                result->control_change.volume_pedal_percent,
                result->control_change.volume_pedal_position))
        {
            ++s_status.audio_sync_errors;
        }
        if ((updates & GP100_UI_UPDATE_SETTINGS) != 0u)
        {
            s_runtime.settings_dirty = true;
        }
        if ((updates & GP100_UI_UPDATE_PRESET) != 0u)
        {
            s_runtime.preset_modified = true;
        }
        if ((updates & GP100_UI_UPDATE_PRESET_LOAD) != 0u)
        {
            s_runtime.preset_modified = false;
            ++s_status.preset_loads;
        }
    }
    else if (result->action == GP100_MIDI_ACTION_PRESET_RECALL)
    {
        updates = GP100_UI_UPDATE_PRESET_LOAD |
                  GP100_UI_UPDATE_REDRAW;
        s_runtime.preset_modified = false;
        ++s_status.preset_loads;
    }
    else if (result->action == GP100_MIDI_ACTION_SYSEX_DATA ||
             result->action == GP100_MIDI_ACTION_SYSEX_COMMAND)
    {
        updates = s_sysexUiUpdates;
    }
    if (result->settings_changed)
    {
        s_runtime.settings_dirty = true;
    }
    if ((updates & (GP100_UI_UPDATE_PRESET |
                    GP100_UI_UPDATE_PRESET_LOAD)) != 0u &&
        (!gp100_preset_volume_pedal_position(
             &s_runtime.preset, &volumePosition) ||
         !gp100_firmware_runtime_set_volume_pedal(
             &s_runtime, (float)s_runtime.volume_pedal_percent,
             volumePosition)))
    {
        ++s_status.audio_sync_errors;
    }
    s_status.last_ui_updates = updates;
    if ((updates & GP100_UI_UPDATE_SETTINGS) != 0u ||
        result->settings_changed)
    {
        GP100_ApplicationQueueSettings();
    }
    if (!GP100_ApplicationSyncAudio((updates & GP100_UI_UPDATE_PRESET_LOAD) != 0u,
                                    (updates & GP100_UI_UPDATE_PRESET) != 0u))
    {
        ++s_status.audio_sync_errors;
    }
    if ((updates & GP100_UI_UPDATE_REDRAW) != 0u &&
        s_status.display_initialized)
    {
        (void)GP100_ApplicationRender();
    }
}

static bool GP100_ApplicationServiceMidi(void)
{
    gp100_midi_runtime_result_t result;
    gp100_midi_poll_result_t pollResult;
    size_t messageLength;
    uint8_t byte;
    bool sawInput = false;

    while (GP100_MidiInputPop(&byte))
    {
        sawInput = true;
        ++s_status.midi_bytes;
        if (!gp100_midi_stream_push(&s_midiStream, &byte, 1u))
        {
            ++s_status.midi_errors;
            gp100_midi_stream_reset(&s_midiStream);
        }
    }
    do
    {
        pollResult = gp100_midi_stream_poll(
            &s_midiStream, s_midiMessage, sizeof(s_midiMessage),
            &messageLength);
        if (pollResult == GP100_MIDI_POLL_MESSAGE)
        {
            uint32_t interruptMask;
            bool handled;

            ++s_status.midi_messages;
            interruptMask = DisableGlobalIRQ();
            handled = gp100_midi_runtime_handle_message(
                &s_midiRuntime, s_midiMessage, messageLength, &result);
            EnableGlobalIRQ(interruptMask);
            if (handled && s_sysexSavePending)
            {
                handled = GP100_ApplicationSaveSysExPreset(
                    s_sysexSaveUserIndex);
                s_sysexSavePending = false;
                if (handled)
                {
                    ++s_status.sysex_writes;
                }
            }
            if (handled && s_sysexCabinetPending)
            {
                handled = GP100_ApplicationSaveSysExCabinet(
                    s_sysexCabinetUserIndex,
                    s_sysexCabinetReloadPending);
                s_sysexCabinetPending = false;
                s_sysexCabinetReloadPending = false;
                if (handled)
                {
                    ++s_status.sysex_writes;
                }
            }
            if (handled && s_sysexUserAmpPending)
            {
                handled = GP100_ApplicationSaveSysExUserAmp(
                    s_sysexUserAmpIndex,
                    s_sysexUserAmpReloadPending);
                s_sysexUserAmpPending = false;
                s_sysexUserAmpReloadPending = false;
                if (handled)
                {
                    ++s_status.sysex_writes;
                }
            }
            if (handled && s_sysexUserAmpDescriptorPending)
            {
                handled = GP100_ApplicationSaveSysExUserAmpDescriptor(
                    s_sysexUserAmpDescriptorIndex,
                    s_sysexUserAmpDescriptorReloadPending);
                s_sysexUserAmpDescriptorPending = false;
                s_sysexUserAmpDescriptorReloadPending = false;
                if (handled)
                {
                    ++s_status.sysex_writes;
                }
            }
            if (!handled)
            {
                ++s_status.midi_errors;
            }
            else
            {
                GP100_ApplicationApplyMidiResult(&result);
            }
        }
        else if (pollResult == GP100_MIDI_POLL_DISCARDED ||
                 pollResult == GP100_MIDI_POLL_OUTPUT_TOO_SMALL)
        {
            ++s_status.midi_errors;
        }
    } while (pollResult == GP100_MIDI_POLL_MESSAGE ||
             pollResult == GP100_MIDI_POLL_DISCARDED);
    s_status.midi_dropped = GP100_MidiInputDropped();
    s_status.midi_uart_errors = GP100_MidiInputErrors();
    return sawInput;
}

static bool GP100_ApplicationServiceTuner(void)
{
    uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT];
    gp100_tuner_model_t *model = &s_runtime.ui.tuner;
    gp100_tuner_reading_t previous;
    const uint32_t now = g_gp100_board_state.milliseconds;
    const uint32_t elapsed = now - s_tunerTickMs;
    uint32_t sequence;
    bool ready;

    if (!s_status.tuner_running || elapsed == 0u)
    {
        return false;
    }
    previous = model->reading;
    s_tunerTickMs = now;
    ready = GP100_TunerCaptureReadIntervals(intervals, &sequence);
    if (ready && s_tunerSignalPresent)
    {
        uint32_t updates = elapsed < GP100_TUNER_HISTORY_LENGTH
            ? elapsed : GP100_TUNER_HISTORY_LENGTH;

        /* During a blocking LCD transfer, use the newest available window for
         * the elapsed service ticks. A complete history caps the catch-up. */
        s_status.tuner_windows += updates;
        while (updates != 0u)
        {
            if (!gp100_tuner_update_factory_intervals(model, intervals, NULL))
            {
                s_status.tuner_rejected_windows += updates;
                break;
            }
            ++s_status.tuner_readings;
            --updates;
        }
    }
    if (now - s_tunerSignalWindowMs >= GP100_TUNER_SIGNAL_WINDOW_MS)
    {
        bool signalPresent = sequence - s_tunerSignalWindowSequence >=
                             GP100_TUNER_SIGNAL_MINIMUM_EDGES;

        if (!signalPresent && s_tunerSignalPresent)
        {
            memset(model->detune_half_cents_history, 0,
                   sizeof(model->detune_half_cents_history));
        }
        s_tunerSignalPresent = signalPresent;
        s_tunerSignalWindowSequence = sequence;
        s_tunerSignalWindowMs = now;
    }
    if (s_status.display_initialized && s_runtime.ui.mode == GP100_UI_MODE_TUNER &&
        (previous.valid != model->reading.valid ||
         previous.note_letter != model->reading.note_letter ||
         previous.sharp != model->reading.sharp ||
         previous.octave != model->reading.octave ||
         previous.needle_step != model->reading.needle_step))
    {
        (void)GP100_ApplicationRender();
    }
    return true;
}

static bool GP100_ApplicationServiceSettingsWriter(void)
{
    const uint32_t milliseconds = g_gp100_board_state.milliseconds;

    if (!s_persistenceConfigured ||
        milliseconds == s_lastSettingsServiceMs)
    {
        return false;
    }
    s_lastSettingsServiceMs = milliseconds;
    return GP100_ApplicationWriteSettings(&s_settingsWriter, &s_settingsToSave) !=
           GP100_SETTINGS_WRITER_IDLE;
}

static bool GP100_ApplicationServiceGenreWriter(void)
{
    const gp100_genre_writer_hooks_t hooks = {
        NULL, GP100_ApplicationEraseSector, GP100_ApplicationProgramErased
    };
    gp100_genre_writer_result_t result;

    if (!s_sysexGenrePending || !s_persistenceConfigured)
    {
        return false;
    }
    result = gp100_genre_writer_append(
        &s_genreWriter, &s_sysexSession.genre_catalog, &hooks);
    if (result == GP100_GENRE_WRITER_WRITTEN)
    {
        s_sysexGenrePending = false;
        ++s_status.genre_writes;
        return true;
    }
    ++s_status.persistence_errors;
    return true;
}

bool GP100_ApplicationService(void)
{
    gp100_event_t event;
    gp100_expression_route_result_t routeResult;
    bool sawEvent = false;

    if (!s_status.initialized)
    {
        return false;
    }
    while (GP100_ControlInputPop(&event))
    {
        sawEvent = true;
        (void)GP100_ApplicationHandleEvent(&event);
    }
    if (s_status.analog_running &&
        s_runtime.ui.mode == GP100_UI_MODE_HARDWARE_TEST)
    {
        bool changed = s_expressionValue != s_hardwareTestPedalValue;

        (void)gp100_hardware_test_observe_pedal(
            &s_runtime.ui.hardware_test, s_expressionValue);
        if (changed)
        {
            s_hardwareTestPedalValue = s_expressionValue;
            sawEvent = true;
            if (s_status.display_initialized)
            {
                (void)GP100_ApplicationRender();
            }
        }
    }
    else if (s_status.analog_running)
    {
        routeResult = gp100_event_router_sample_expression(
            &s_eventRouter, s_expressionValue,
            g_gp100_board_state.milliseconds, &event);
        if (routeResult > GP100_EXPRESSION_ROUTE_UNCHANGED)
        {
            sawEvent = true;
            ++s_status.expression_events;
            (void)GP100_ApplicationHandleEvent(&event);
        }
    }
    if (s_runtime.ui.mode == GP100_UI_MODE_HARDWARE_TEST &&
        s_hardwareTestTickMs != g_gp100_board_state.milliseconds)
    {
        s_hardwareTestTickMs = g_gp100_board_state.milliseconds;
        if (gp100_hardware_test_tick(&s_runtime.ui.hardware_test))
        {
            sawEvent = true;
            if (s_status.display_initialized)
            {
                (void)GP100_ApplicationRender();
            }
        }
    }
    if (s_status.midi_running && GP100_ApplicationServiceMidi())
    {
        sawEvent = true;
    }
    if (GP100_AudioBindingService())
    {
        sawEvent = true;
    }
    if (GP100_ApplicationServiceTuner())
    {
        sawEvent = true;
    }
    if (GP100_ApplicationServiceSettingsWriter())
    {
        sawEvent = true;
    }
    if (GP100_ApplicationServiceGenreWriter())
    {
        sawEvent = true;
    }
    if (s_displayRetryPending &&
        (uint32_t)(g_gp100_board_state.milliseconds - s_displayFailedAtMs) >=
            GP100_DISPLAY_RETRY_DELAY_MS)
    {
        /* Retry a transport failure once. A newer render takes precedence. */
        s_displayRetryPending = false;
        (void)GP100_ApplicationFlushDisplay();
        sawEvent = true;
    }
    return sawEvent;
}

gp100_firmware_runtime_t *GP100_ApplicationRuntime(void)
{
    return s_status.initialized ? &s_runtime : NULL;
}

const gp100_application_status_t *GP100_ApplicationStatus(void)
{
    gp100_audio_binding_status_t audioStatus;

    GP100_AudioBindingGetStatus(&audioStatus);
    s_status.audio_running = audioStatus.running;
    s_status.audio_blocks = audioStatus.processed_blocks;
    s_status.audio_errors = audioStatus.processing_errors;
    s_status.audio_clipped_samples = audioStatus.clipped_samples;
    s_status.rhythm_audio_blocks = audioStatus.rhythm_blocks;
    s_status.rhythm_audio_errors = audioStatus.rhythm_errors;
    s_status.rhythm_dropped_triggers =
        audioStatus.rhythm_dropped_triggers;
    s_status.looper_audio_blocks = audioStatus.looper_blocks;
    s_status.looper_audio_errors = audioStatus.looper_errors;
    s_status.looper_wraps = audioStatus.looper_wraps;
    s_status.looper_capacity_stops = audioStatus.looper_capacity_stops;
    return &s_status;
}
