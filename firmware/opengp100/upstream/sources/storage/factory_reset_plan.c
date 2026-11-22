#include "factory_reset_plan.h"
#include "flash_layout.h"

#define GP100_RESET_CABINET_START UINT32_C(0x006e0000)
#define GP100_RESET_CABINET_SECTORS 32u
#define GP100_RESET_SETTINGS_START UINT32_C(0x007f0000)
#define GP100_RESET_SETTINGS_SECTORS 9u
#define GP100_RESET_GENRE_START UINT32_C(0x007f9000)
#define GP100_RESET_GENRE_SECTORS 5u
#define GP100_RESET_USER_AMP_DESCRIPTOR_START UINT32_C(0x006c0000)
#define GP100_RESET_USER_AMP_DESCRIPTOR_SECTORS 1u

#define GP100_RESET_PRESET_FIRST_STEP \
    GP100_RESET_CABINET_SECTORS
#define GP100_RESET_SETTINGS_FIRST_STEP \
    (GP100_RESET_PRESET_FIRST_STEP + GP100_RESET_PRESET_SECTORS)
#define GP100_RESET_GENRE_FIRST_STEP \
    (GP100_RESET_SETTINGS_FIRST_STEP + \
     GP100_RESET_SETTINGS_SECTORS)
#define GP100_RESET_DESCRIPTOR_FIRST_STEP \
    (GP100_RESET_GENRE_FIRST_STEP + GP100_RESET_GENRE_SECTORS)

_Static_assert(
    GP100_RESET_DESCRIPTOR_FIRST_STEP +
        GP100_RESET_USER_AMP_DESCRIPTOR_SECTORS ==
        GP100_FACTORY_RESET_SECTOR_STEPS,
    "Recovered factory-reset ranges must total 167 sectors");

bool gp100_factory_reset_sector_address(
    size_t step,
    uint32_t *address)
{
    if (address == NULL || step >= GP100_FACTORY_RESET_SECTOR_STEPS) {
        return false;
    }
    if (step < GP100_RESET_PRESET_FIRST_STEP) {
        *address = GP100_RESET_CABINET_START +
                   (uint32_t)step * GP100_FACTORY_RESET_SECTOR_BYTES;
    } else if (step < GP100_RESET_SETTINGS_FIRST_STEP) {
        *address = GP100_RESET_PRESET_START +
                   (uint32_t)(step - GP100_RESET_PRESET_FIRST_STEP) *
                       GP100_FACTORY_RESET_SECTOR_BYTES;
    } else if (step < GP100_RESET_GENRE_FIRST_STEP) {
        *address = GP100_RESET_SETTINGS_START +
                   (uint32_t)(step - GP100_RESET_SETTINGS_FIRST_STEP) *
                       GP100_FACTORY_RESET_SECTOR_BYTES;
    } else if (step < GP100_RESET_DESCRIPTOR_FIRST_STEP) {
        *address = GP100_RESET_GENRE_START +
                   (uint32_t)(step - GP100_RESET_GENRE_FIRST_STEP) *
                       GP100_FACTORY_RESET_SECTOR_BYTES;
    } else {
        *address = GP100_RESET_USER_AMP_DESCRIPTOR_START;
    }
    return true;
}
