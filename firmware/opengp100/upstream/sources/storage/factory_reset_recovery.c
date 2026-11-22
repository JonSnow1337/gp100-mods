#include "factory_reset_recovery.h"

bool gp100_factory_reset_rebuild_records(
    gp100_sysex_user_amp_record_t
        user_amps[GP100_SYSEX_USER_AMP_SLOT_COUNT],
    gp100_genre_catalog_t *genres,
    gp100_settings_record_t *settings,
    size_t language_count,
    uint32_t *settings_repairs)
{
    size_t index;
    uint32_t repairs;

    if (user_amps == NULL || genres == NULL || settings == NULL ||
        language_count == 0u) {
        return false;
    }
    for (index = 0u;
         index < GP100_SYSEX_USER_AMP_SLOT_COUNT;
         ++index) {
        if (!gp100_sysex_user_amp_make_factory_record(
                (uint8_t)index, &user_amps[index])) {
            return false;
        }
    }
    if (!gp100_genre_catalog_make_factory(genres) ||
        !gp100_make_factory_settings(settings)) {
        return false;
    }
    repairs = gp100_settings_normalize(settings, language_count);
    if (settings_repairs != NULL) {
        *settings_repairs = repairs;
    }
    return true;
}

bool gp100_factory_reset_runtime_action(
    size_t index,
    gp100_factory_reset_runtime_action_t *action)
{
    static const gp100_factory_reset_runtime_action_t actions[] = {
        GP100_FACTORY_RESET_RECALL_CURRENT_PRESET,
        GP100_FACTORY_RESET_RESTORE_OUTPUT_IF_IDLE,
        GP100_FACTORY_RESET_CLEAR_RUNTIME_FLAGS,
        GP100_FACTORY_RESET_START_EFFECTS,
        GP100_FACTORY_RESET_MARK_PRESET_ACTIVE,
    };

    if (action == NULL || index >= sizeof(actions) / sizeof(actions[0])) {
        return false;
    }
    *action = actions[index];
    return true;
}
