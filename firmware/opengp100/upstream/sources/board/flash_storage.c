#include "flash_storage.h"

#include "board.h"

#include "audio/effects/cabinet/cabinet.h"
#include "storage/expression_calibration_record.h"
#include "storage/flash_layout.h"
#include "storage/genre_record.h"
#include "storage/preset_store.h"
#include "storage/settings_store.h"
#include "protocol/midi/sysex/vendor_sysex_user_amp.h"

#include "fsl_common.h"
#include "fsl_romapi.h"

#include <limits.h>
#include <string.h>

#define GP100_FLASH_XIP_BASE UINT32_C(0x60000000)
#define GP100_FLASH_INSTANCE 0u

#define GP100_SECTOR_FLOOR(value) \
    ((value) & ~(GP100_FLASH_SECTOR_SIZE - 1u))
#define GP100_SECTOR_CEILING(value) \
    (((value) + GP100_FLASH_SECTOR_SIZE - 1u) & \
     ~(GP100_FLASH_SECTOR_SIZE - 1u))

#define GP100_USER_AMP_START \
    GP100_SECTOR_FLOOR(GP100_USER_AMP_DESCRIPTOR_FLASH_BASE)
#define GP100_USER_AMP_END \
    GP100_SECTOR_CEILING( \
        GP100_USER_AMP_BLOB_FLASH_BASE + \
        GP100_SYSEX_USER_AMP_SLOT_COUNT * \
            GP100_SYSEX_USER_AMP_BLOB_BYTES)
#define GP100_USER_CABINET_START UINT32_C(0x006e0000)
#define GP100_USER_CABINET_END \
    GP100_SECTOR_CEILING( \
        GP100_USER_CABINET_START + GP100_CABINET_USER_COUNT * \
            GP100_CABINET_RECORD_SIZE)
#define GP100_PRESET_STORE_END (GP100_PRESET_STORE_BASE + GP100_PRESET_STORE_BYTES)
#define GP100_SETTINGS_STORE_END (GP100_SETTINGS_STORE_BASE + GP100_SETTINGS_STORE_BYTES)
#define GP100_RESET_PRESET_END \
    (GP100_RESET_PRESET_START + GP100_RESET_PRESET_SECTORS * GP100_FLASH_SECTOR_SIZE)
#define GP100_CALIBRATION_START \
    GP100_SECTOR_FLOOR(GP100_EXPRESSION_CALIBRATION_FLASH_BASE)
#define GP100_CALIBRATION_END \
    (GP100_EXPRESSION_CALIBRATION_FLASH_BASE + \
     GP100_EXPRESSION_CALIBRATION_FLASH_BYTES)
#define GP100_GENRE_START GP100_GENRE_FLASH_BASE
#define GP100_GENRE_END \
    (GP100_GENRE_FLASH_BASE + GP100_GENRE_FLASH_BYTES)

typedef struct
{
    uint32_t begin;
    uint32_t end;
} gp100_flash_range_t;

static const gp100_flash_range_t s_writableRanges[] = {
    {GP100_USER_AMP_START, GP100_USER_AMP_END},
    {GP100_USER_CABINET_START, GP100_USER_CABINET_END},
    {GP100_PRESET_STORE_BASE, GP100_PRESET_STORE_END},
    {GP100_SETTINGS_STORE_BASE, GP100_SETTINGS_STORE_END},
    {GP100_GENRE_START, GP100_GENRE_END},
    {GP100_CALIBRATION_START, GP100_CALIBRATION_END},
};

extern const uint32_t g_gp100_flexspi_config[128];

static flexspi_nor_config_t s_flashConfig __attribute__((aligned(4)));
static uint32_t s_programPage[GP100_FLASH_PAGE_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(32)));
static gp100_flash_storage_status_t s_status;

_Static_assert(sizeof(flexspi_nor_config_t) == sizeof(g_gp100_flexspi_config),
               "The RT1052 ROM config must match the 512-byte boot FCB");
_Static_assert(GP100_USER_AMP_END == UINT32_C(0x006cb000),
               "Unexpected user-amp write boundary");
_Static_assert(GP100_USER_AMP_START == GP100_USER_DATA_START,
               "The user-amp bank starts the user region");
_Static_assert(GP100_USER_CABINET_END == UINT32_C(0x006f5000),
               "Unexpected user-cabinet write boundary");
_Static_assert(GP100_USER_PRESET_BASE == GP100_RESET_PRESET_START &&
               GP100_PRESET_STORE_BASE >= GP100_SECTOR_CEILING(
                   GP100_USER_PRESET_BASE + GP100_PRESET_LEGACY_BYTES) &&
               GP100_PRESET_STORE_BASE == GP100_SECTOR_FLOOR(GP100_PRESET_STORE_BASE),
               "The new preset store must start after the legacy bank");
_Static_assert(GP100_PRESET_STORE_END <= GP100_SETTINGS_STORE_BASE &&
               GP100_SETTINGS_STORE_BASE == GP100_SECTOR_FLOOR(GP100_SETTINGS_STORE_BASE) &&
               GP100_SETTINGS_STORE_END <= GP100_RESET_PRESET_END &&
               GP100_RESET_PRESET_END <= GP100_SETTINGS_FLASH_BASE,
               "The new stores must fit in the preset reset range");
_Static_assert(GP100_GENRE_END == UINT32_C(0x007fe000),
               "Unexpected genre journal write boundary");
_Static_assert(GP100_CALIBRATION_END == UINT32_C(0x007ff000),
               "The calibration policy must stop before device identity");
_Static_assert(GP100_FACTORY_RESET_SECTOR_BYTES ==
                   GP100_FLASH_SECTOR_SIZE,
               "Factory-reset and flash-update sector sizes must agree");
bool GP100_FlashStorageAddressAllowed(uint32_t address, size_t size)
{
    uint32_t end;
    size_t index;

    if (size == 0u || size > UINT32_MAX || address < GP100_USER_DATA_START ||
        address >= BOARD_FLASH_SIZE ||
        (uint32_t)size > BOARD_FLASH_SIZE - address)
    {
        return false;
    }
    end = address + (uint32_t)size;
    for (index = 0u;
         index < sizeof(s_writableRanges) / sizeof(s_writableRanges[0]);
         ++index)
    {
        if (address >= s_writableRanges[index].begin &&
            end <= s_writableRanges[index].end)
        {
            return true;
        }
    }
    return false;
}

bool GP100_FlashStorageFactoryResetAddress(size_t step, uint32_t *address)
{
    return gp100_factory_reset_sector_address(step, address);
}

static void GP100_FlashStorageReject(uint32_t address)
{
    ++s_status.rejected_operations;
    s_status.last_address = address;
    s_status.last_rom_status = (int32_t)kStatus_InvalidArgument;
}

static void GP100_FlashStorageInvalidate(uint32_t address, size_t size)
{
    ROM_FLEXSPI_NorFlash_ClearCache(GP100_FLASH_INSTANCE);
    SCB_InvalidateDCache_by_Addr(
        (void *)(uintptr_t)(GP100_FLASH_XIP_BASE + address), (int32_t)size);
    __DSB();
    __ISB();
}

static bool GP100_FlashStorageRead(void *context,
                                  uint32_t address,
                                  uint8_t *data,
                                  size_t size)
{
    (void)context;
    if (data == NULL || !GP100_FlashStorageAddressAllowed(address, size))
    {
        GP100_FlashStorageReject(address);
        return false;
    }
    memcpy(data, (const void *)(uintptr_t)(GP100_FLASH_XIP_BASE + address),
           size);
    return true;
}

static bool GP100_FlashStorageErasePhysical(uint32_t address)
{
    uint32_t interruptMask;
    status_t result;

    if ((address & (GP100_FLASH_SECTOR_SIZE - 1u)) != 0u ||
        address < GP100_USER_DATA_START ||
        address > BOARD_FLASH_SIZE - GP100_FLASH_SECTOR_SIZE)
    {
        GP100_FlashStorageReject(address);
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    /* The ROM issues IP commands using NOR offsets. Keep interrupts masked
     * while FlexSPI is busy so no handler can fetch from the XIP window. */
    result = ROM_FLEXSPI_NorFlash_EraseSector(
        GP100_FLASH_INSTANCE, &s_flashConfig, address);
    if (result == kStatus_Success)
    {
        GP100_FlashStorageInvalidate(address, GP100_FLASH_SECTOR_SIZE);
    }
    EnableGlobalIRQ(interruptMask);

    s_status.last_address = address;
    s_status.last_rom_status = (int32_t)result;
    if (result != kStatus_Success)
    {
        ++s_status.failed_operations;
        return false;
    }
    ++s_status.erase_count;
    return true;
}

static bool GP100_FlashStorageEraseSector(void *context, uint32_t address)
{
    (void)context;
    if (!GP100_FlashStorageAddressAllowed(
            address, GP100_FLASH_SECTOR_SIZE))
    {
        GP100_FlashStorageReject(address);
        return false;
    }
    return GP100_FlashStorageErasePhysical(address);
}

static bool GP100_FlashStorageProgramPage(void *context,
                                         uint32_t address,
                                         const uint8_t *data,
                                         size_t size)
{
    uint32_t interruptMask;
    status_t result;

    (void)context;
    if (data == NULL || size != GP100_FLASH_PAGE_SIZE ||
        (address & (GP100_FLASH_PAGE_SIZE - 1u)) != 0u ||
        !GP100_FlashStorageAddressAllowed(address, size))
    {
        GP100_FlashStorageReject(address);
        return false;
    }
    memcpy(s_programPage, data, sizeof(s_programPage));
    interruptMask = DisableGlobalIRQ();
    /* See the erase path: the ROM call returns only after the IP command and
     * flash busy interval have completed. */
    result = ROM_FLEXSPI_NorFlash_ProgramPage(
        GP100_FLASH_INSTANCE, &s_flashConfig, address, s_programPage);
    if (result == kStatus_Success)
    {
        GP100_FlashStorageInvalidate(address, size);
    }
    EnableGlobalIRQ(interruptMask);

    s_status.last_address = address;
    s_status.last_rom_status = (int32_t)result;
    if (result != kStatus_Success)
    {
        ++s_status.failed_operations;
        return false;
    }
    ++s_status.program_count;
    return true;
}

bool GP100_FlashStorageInitialize(gp100_flash_update_hooks_t *hooks)
{
    uint32_t interruptMask;
    status_t result;

    if (hooks == NULL)
    {
        return false;
    }
    memset(&s_status, 0, sizeof(s_status));
    memset(hooks, 0, sizeof(*hooks));
    memcpy(&s_flashConfig, g_gp100_flexspi_config, sizeof(s_flashConfig));
    if (s_flashConfig.memConfig.tag != FLEXSPI_CFG_BLK_TAG ||
        s_flashConfig.pageSize != GP100_FLASH_PAGE_SIZE ||
        s_flashConfig.sectorSize != GP100_FLASH_SECTOR_SIZE)
    {
        s_status.last_rom_status = (int32_t)kStatus_InvalidArgument;
        return false;
    }

    /* The SPI NOR's Read Status (0x05) reports write-in-progress at bit 0,
     * with 1 meaning busy. Use that convention in the runtime ROM config. */
    s_flashConfig.memConfig.busyOffset = 0u;
    s_flashConfig.memConfig.busyBitPolarity = 0u;

    interruptMask = DisableGlobalIRQ();
    result = ROM_FLEXSPI_NorFlash_Init(GP100_FLASH_INSTANCE, &s_flashConfig);
    EnableGlobalIRQ(interruptMask);
    s_status.last_rom_status = (int32_t)result;
    if (result != kStatus_Success)
    {
        ++s_status.failed_operations;
        return false;
    }

    hooks->context = NULL;
    hooks->read = GP100_FlashStorageRead;
    hooks->erase_sector = GP100_FlashStorageEraseSector;
    hooks->program_page = GP100_FlashStorageProgramPage;
    s_status.initialized = true;
    return true;
}

bool GP100_FlashStorageFactoryResetBegin(void)
{
    if (!s_status.initialized || s_status.factory_reset_active)
    {
        return false;
    }
    s_status.factory_reset_completed_steps = 0u;
    s_status.factory_reset_active = true;
    return true;
}

gp100_flash_factory_reset_result_t GP100_FlashStorageFactoryResetService(
    size_t *completedSteps)
{
    uint32_t address;

    if (completedSteps != NULL)
    {
        *completedSteps = s_status.factory_reset_completed_steps;
    }
    if (!s_status.factory_reset_active)
    {
        return kGP100_FlashFactoryResetIdle;
    }
    if (!GP100_FlashStorageFactoryResetAddress(
            s_status.factory_reset_completed_steps, &address) ||
        !GP100_FlashStorageErasePhysical(address))
    {
        s_status.factory_reset_active = false;
        return kGP100_FlashFactoryResetError;
    }
    ++s_status.factory_reset_completed_steps;
    if (completedSteps != NULL)
    {
        *completedSteps = s_status.factory_reset_completed_steps;
    }
    if (s_status.factory_reset_completed_steps ==
        GP100_FLASH_FACTORY_RESET_STEPS)
    {
        s_status.factory_reset_active = false;
        return kGP100_FlashFactoryResetComplete;
    }
    return kGP100_FlashFactoryResetProgress;
}

const gp100_flash_storage_status_t *GP100_FlashStorageStatus(void)
{
    return &s_status;
}
