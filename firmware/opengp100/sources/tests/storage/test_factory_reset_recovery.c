#include "storage/factory_reset_recovery.h"

#include <string.h>

static int test_records(void)
{
    static const char system_names[GP100_SYSTEM_GENRE_COUNT]
                                  [GP100_GENRE_NAME_SIZE] = {
        "Metal", "World", "Indie", "Country", "Rock", "Funk",
        "Pop", "Blues", "Jazz", "Bass", "Acoustic"
    };
    gp100_sysex_user_amp_record_t
        user_amps[GP100_SYSEX_USER_AMP_SLOT_COUNT];
    gp100_genre_catalog_t genres;
    gp100_settings_record_t settings;
    uint32_t repairs = UINT32_MAX;
    int16_t current_preset = -1;
    size_t index;

    memset(user_amps, 0xff, sizeof(user_amps));
    memset(&genres, 0xff, sizeof(genres));
    memset(&settings, 0xff, sizeof(settings));
    if (!gp100_factory_reset_rebuild_records(
            user_amps, &genres, &settings, 9u, &repairs) ||
        repairs != GP100_SETTINGS_REPAIR_NONE ||
        !gp100_settings_validate(&settings) ||
        !gp100_settings_get_i16(
            &settings, GP100_SETTINGS_CURRENT_PRESET,
            &current_preset) ||
        current_preset != 0) {
        return 1;
    }
    for (index = 0u;
         index < GP100_SYSEX_USER_AMP_SLOT_COUNT;
         ++index) {
        char expected[12] = "SnapTone1";

        expected[8] = (char)('1' + index);
        if (user_amps[index].present ||
            memcmp(user_amps[index].metadata, expected,
                   sizeof(expected)) != 0) {
            return 2;
        }
    }
    for (index = 0u; index < GP100_SYSTEM_GENRE_COUNT; ++index) {
        if (memcmp(genres.names[index], system_names[index],
                   GP100_GENRE_NAME_SIZE) != 0) {
            return 3;
        }
    }
    for (index = GP100_SYSTEM_GENRE_COUNT;
         index < GP100_PRESET_GENRE_COUNT;
         ++index) {
        static const uint8_t empty[GP100_GENRE_NAME_SIZE] = {0};

        if (memcmp(genres.names[index], empty, sizeof(empty)) != 0) {
            return 4;
        }
    }
    return 0;
}

static int test_runtime_plan(void)
{
    static const gp100_factory_reset_runtime_action_t expected[] = {
        GP100_FACTORY_RESET_RECALL_CURRENT_PRESET,
        GP100_FACTORY_RESET_RESTORE_OUTPUT_IF_IDLE,
        GP100_FACTORY_RESET_CLEAR_RUNTIME_FLAGS,
        GP100_FACTORY_RESET_START_EFFECTS,
        GP100_FACTORY_RESET_MARK_PRESET_ACTIVE,
    };
    size_t index;

    for (index = 0u; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        gp100_factory_reset_runtime_action_t action;

        if (!gp100_factory_reset_runtime_action(index, &action) ||
            action != expected[index]) {
            return 1;
        }
    }
    if (gp100_factory_reset_runtime_action(0u, NULL) ||
        gp100_factory_reset_runtime_action(
            GP100_FACTORY_RESET_RUNTIME_ACTION_COUNT,
            &(gp100_factory_reset_runtime_action_t){0})) {
        return 2;
    }
    return 0;
}

static int test_invalid_inputs(void)
{
    gp100_sysex_user_amp_record_t
        user_amps[GP100_SYSEX_USER_AMP_SLOT_COUNT];
    gp100_genre_catalog_t genres;
    gp100_settings_record_t settings;

    return gp100_factory_reset_rebuild_records(
               NULL, &genres, &settings, 1u, NULL) ||
           gp100_factory_reset_rebuild_records(
               user_amps, NULL, &settings, 1u, NULL) ||
           gp100_factory_reset_rebuild_records(
               user_amps, &genres, NULL, 1u, NULL) ||
           gp100_factory_reset_rebuild_records(
               user_amps, &genres, &settings, 0u, NULL);
}

int main(void)
{
    int result = test_records();

    if (result != 0) {
        return 10 + result;
    }
    result = test_runtime_plan();
    if (result != 0) {
        return 20 + result;
    }
    return test_invalid_inputs() ? 31 : 0;
}
