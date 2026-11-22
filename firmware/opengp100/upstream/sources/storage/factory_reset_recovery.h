#ifndef GP100_FACTORY_RESET_RECOVERY_H
#define GP100_FACTORY_RESET_RECOVERY_H

#include "app/firmware_runtime.h"
#include "genre_record.h"
#include "protocol/midi/sysex/vendor_sysex_user_amp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Portable runtime operations that follow record reconstruction in factory
 * routine 0x00005af8. The board/application layer owns their synchronization. */
typedef enum gp100_factory_reset_runtime_action {
    GP100_FACTORY_RESET_RECALL_CURRENT_PRESET = 0,
    GP100_FACTORY_RESET_RESTORE_OUTPUT_IF_IDLE,
    GP100_FACTORY_RESET_CLEAR_RUNTIME_FLAGS,
    GP100_FACTORY_RESET_START_EFFECTS,
    GP100_FACTORY_RESET_MARK_PRESET_ACTIVE,
    GP100_FACTORY_RESET_RUNTIME_ACTION_COUNT
} gp100_factory_reset_runtime_action_t;

/* Rebuild all portable records whose sectors the factory reset erased.
 * user_amps must provide exactly the five runtime descriptor slots. */
bool gp100_factory_reset_rebuild_records(
    gp100_sysex_user_amp_record_t
        user_amps[GP100_SYSEX_USER_AMP_SLOT_COUNT],
    gp100_genre_catalog_t *genres,
    gp100_settings_record_t *settings,
    size_t language_count,
    uint32_t *settings_repairs);

/* Return the ordered in-place runtime publication plan. */
bool gp100_factory_reset_runtime_action(
    size_t index,
    gp100_factory_reset_runtime_action_t *action);

#ifdef __cplusplus
}
#endif

#endif
