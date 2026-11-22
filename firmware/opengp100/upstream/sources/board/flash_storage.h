#ifndef GP100_FLASH_STORAGE_H
#define GP100_FLASH_STORAGE_H

#include "storage/factory_reset_plan.h"
#include "storage/flash_update.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GP100_FLASH_FACTORY_RESET_STEPS \
    GP100_FACTORY_RESET_SECTOR_STEPS

typedef enum
{
    kGP100_FlashFactoryResetIdle = 0,
    kGP100_FlashFactoryResetProgress,
    kGP100_FlashFactoryResetComplete,
    kGP100_FlashFactoryResetError
} gp100_flash_factory_reset_result_t;

typedef struct
{
    bool initialized;
    uint32_t erase_count;
    uint32_t program_count;
    uint32_t rejected_operations;
    uint32_t failed_operations;
    uint32_t last_address;
    int32_t last_rom_status;
    size_t factory_reset_completed_steps;
    bool factory_reset_active;
} gp100_flash_storage_status_t;

/* Initializes the RT1052 ROM FlexSPI NOR driver from the exact boot FCB and
 * returns hooks suitable for GP100_ApplicationSetFlashHooks(). */
bool GP100_FlashStorageInitialize(gp100_flash_update_hooks_t *hooks);

/* The backend deliberately admits only whole sectors that contain recovered
 * user-mutable data. This helper is public for diagnostics and policy tests. */
bool GP100_FlashStorageAddressAllowed(uint32_t address, size_t size);

/* Execute the exact recovered 167-sector V2.1.0 reset sequence one sector at
 * a time. Reset-only sectors are not added to the ordinary write allowlist. */
bool GP100_FlashStorageFactoryResetAddress(size_t step, uint32_t *address);
bool GP100_FlashStorageFactoryResetBegin(void);
gp100_flash_factory_reset_result_t GP100_FlashStorageFactoryResetService(
    size_t *completedSteps);

const gp100_flash_storage_status_t *GP100_FlashStorageStatus(void);

#endif
