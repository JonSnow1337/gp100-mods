#include "storage/flash_update.h"

#include <stdio.h>
#include <string.h>

#define TEST_FLASH_SIZE (GP100_FLASH_SECTOR_SIZE * 3u)

typedef struct fake_flash {
    uint8_t bytes[TEST_FLASH_SIZE];
    size_t reads;
    size_t erases;
    size_t programs;
    size_t fail_operation;
    size_t operations;
} fake_flash_t;

static bool should_fail(fake_flash_t *flash)
{
    ++flash->operations;
    return flash->fail_operation != 0u &&
           flash->operations == flash->fail_operation;
}

static bool read_flash(void *context, uint32_t address,
                       uint8_t *data, size_t size)
{
    fake_flash_t *flash = context;
    if (address > TEST_FLASH_SIZE || size > TEST_FLASH_SIZE - address ||
        should_fail(flash)) {
        return false;
    }
    memcpy(data, flash->bytes + address, size);
    ++flash->reads;
    return true;
}

static bool erase_sector(void *context, uint32_t address)
{
    fake_flash_t *flash = context;
    if ((address % GP100_FLASH_SECTOR_SIZE) != 0u ||
        address > TEST_FLASH_SIZE - GP100_FLASH_SECTOR_SIZE ||
        should_fail(flash)) {
        return false;
    }
    memset(flash->bytes + address, 0xff, GP100_FLASH_SECTOR_SIZE);
    ++flash->erases;
    return true;
}

static bool program_page(void *context, uint32_t address,
                         const uint8_t *data, size_t size)
{
    fake_flash_t *flash = context;
    size_t index;
    if (size != GP100_FLASH_PAGE_SIZE ||
        (address % GP100_FLASH_PAGE_SIZE) != 0u ||
        address > TEST_FLASH_SIZE - size || should_fail(flash)) {
        return false;
    }
    for (index = 0u; index < size; ++index) {
        /* Programming a one leaves the NOR bit at its existing value. */
        flash->bytes[address + index] &= data[index];
    }
    ++flash->programs;
    return true;
}

static void initialize_flash(fake_flash_t *flash)
{
    size_t index;
    memset(flash, 0, sizeof(*flash));
    for (index = 0u; index < sizeof(flash->bytes); ++index) {
        flash->bytes[index] = (uint8_t)(index * 17u + 3u);
    }
}

static int test_cross_sector_update(void)
{
    fake_flash_t flash;
    uint8_t original[TEST_FLASH_SIZE];
    uint8_t workspace[GP100_FLASH_SECTOR_SIZE];
    uint8_t patch[300];
    gp100_flash_update_hooks_t hooks = {
        &flash, read_flash, erase_sector, program_page
    };
    size_t index;
    uint32_t address = GP100_FLASH_SECTOR_SIZE - 100u;

    initialize_flash(&flash);
    memcpy(original, flash.bytes, sizeof(original));
    for (index = 0u; index < sizeof(patch); ++index) {
        patch[index] = (uint8_t)(0xa5u ^ index);
    }
    if (gp100_flash_update(
            &hooks, workspace, sizeof(workspace), address,
            patch, sizeof(patch)) != GP100_FLASH_UPDATE_OK ||
        flash.reads != 2u || flash.erases != 2u ||
        flash.programs != 2u * GP100_FLASH_PAGES_PER_SECTOR ||
        memcmp(flash.bytes + address, patch, sizeof(patch)) != 0 ||
        memcmp(flash.bytes, original, address) != 0 ||
        memcmp(flash.bytes + address + sizeof(patch),
               original + address + sizeof(patch),
               sizeof(original) - address - sizeof(patch)) != 0) {
        return 1;
    }
    return 0;
}

static int test_failures(void)
{
    fake_flash_t flash;
    uint8_t workspace[GP100_FLASH_SECTOR_SIZE];
    uint8_t patch[8] = {0};
    gp100_flash_update_hooks_t hooks = {
        &flash, read_flash, erase_sector, program_page
    };

    initialize_flash(&flash);
    flash.fail_operation = 1u;
    if (gp100_flash_update(
            &hooks, workspace, sizeof(workspace), 16u,
            patch, sizeof(patch)) != GP100_FLASH_UPDATE_READ_FAILED) {
        return 1;
    }
    initialize_flash(&flash);
    flash.fail_operation = 2u;
    if (gp100_flash_update(
            &hooks, workspace, sizeof(workspace), 16u,
            patch, sizeof(patch)) != GP100_FLASH_UPDATE_ERASE_FAILED) {
        return 2;
    }
    initialize_flash(&flash);
    flash.fail_operation = 3u;
    if (gp100_flash_update(
            &hooks, workspace, sizeof(workspace), 16u,
            patch, sizeof(patch)) != GP100_FLASH_UPDATE_PROGRAM_FAILED) {
        return 3;
    }
    if (gp100_flash_update(
            &hooks, workspace, sizeof(workspace), 16u, patch, 0u) !=
            GP100_FLASH_UPDATE_INVALID) {
        return 4;
    }
    return 0;
}

static int test_erased_range_program(void)
{
    fake_flash_t flash;
    uint8_t workspace[GP100_FLASH_PAGE_SIZE];
    uint8_t patch[300];
    gp100_flash_update_hooks_t hooks = {
        &flash, read_flash, erase_sector, program_page
    };
    uint32_t address = GP100_FLASH_PAGE_SIZE - 20u;
    size_t index;

    memset(&flash, 0, sizeof(flash));
    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    for (index = 0u; index < sizeof(patch); ++index) {
        patch[index] = (uint8_t)(index ^ 0x69u);
    }
    if (gp100_flash_program_erased_range(
            &hooks, workspace, sizeof(workspace), address,
            patch, sizeof(patch)) != GP100_FLASH_UPDATE_OK ||
        flash.programs != 3u || flash.reads != 0u || flash.erases != 0u ||
        memcmp(flash.bytes + address, patch, sizeof(patch)) != 0 ||
        flash.bytes[address - 1u] != 0xffu ||
        flash.bytes[address + sizeof(patch)] != 0xffu) {
        return 1;
    }
    return 0;
}

static int test_append_preserves_existing_page(void)
{
    fake_flash_t flash = {0};
    uint8_t workspace[GP100_FLASH_SECTOR_SIZE];
    const uint8_t first[] = {0x12u, 0x34u, 0x56u};
    const uint8_t second[] = {0xadu, 0xbcu, 0xdeu};
    const gp100_flash_update_hooks_t hooks = {
        &flash, read_flash, erase_sector, program_page
    };

    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    memcpy(flash.bytes, first, sizeof(first));
    if (gp100_flash_update(&hooks, workspace, sizeof(workspace), sizeof(first),
                           second, sizeof(second)) != GP100_FLASH_UPDATE_OK ||
        flash.erases != 0u || flash.programs != 1u ||
        memcmp(flash.bytes, first, sizeof(first)) != 0 ||
        memcmp(flash.bytes + sizeof(first), second, sizeof(second)) != 0) {
        return 1;
    }
    if (gp100_flash_update(&hooks, workspace, sizeof(workspace), sizeof(first),
                           second, sizeof(second)) != GP100_FLASH_UPDATE_OK ||
        flash.erases != 0u || flash.programs != 1u) {
        return 2;
    }
    if (gp100_flash_update(&hooks, workspace, sizeof(workspace), sizeof(first),
                           first, sizeof(first)) != GP100_FLASH_UPDATE_OK ||
        flash.erases != 1u || flash.programs != 2u ||
        memcmp(flash.bytes, first, sizeof(first)) != 0 ||
        memcmp(flash.bytes + sizeof(first), first, sizeof(first)) != 0) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_cross_sector_update();

    if (result != 0) {
        fprintf(stderr, "cross-sector flash update failed: %d\n", result);
        return result;
    }
    result = test_failures();
    if (result != 0) {
        fprintf(stderr, "flash failure propagation failed: %d\n", result);
        return 10 + result;
    }
    result = test_erased_range_program();
    if (result != 0) {
        fprintf(stderr, "erased-range program failed: %d\n", result);
        return 20 + result;
    }
    result = test_append_preserves_existing_page();
    if (result != 0) {
        fprintf(stderr, "same-page append failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
