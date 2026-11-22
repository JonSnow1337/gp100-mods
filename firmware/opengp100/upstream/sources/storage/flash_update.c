#include "flash_update.h"

#include <string.h>

bool gp100_flash_bytes_are_erased(const uint8_t *data, size_t size)
{
    for (size_t index = 0u; index < size; ++index) {
        if (data[index] != UINT8_MAX) {
            return false;
        }
    }
    return true;
}

gp100_flash_update_result_t gp100_flash_update(
    const gp100_flash_update_hooks_t *hooks,
    uint8_t *workspace,
    size_t workspace_size,
    uint32_t address,
    const uint8_t *data,
    size_t size)
{
    if (hooks == NULL || hooks->read == NULL ||
        hooks->erase_sector == NULL || hooks->program_page == NULL ||
        workspace == NULL ||
        workspace_size < GP100_FLASH_SECTOR_SIZE ||
        data == NULL || size == 0u || size > UINT32_MAX - address) {
        return GP100_FLASH_UPDATE_INVALID;
    }
    while (size != 0u) {
        uint32_t sector_address =
            address & ~(uint32_t)(GP100_FLASH_SECTOR_SIZE - 1u);
        size_t sector_offset =
            (size_t)(address & (GP100_FLASH_SECTOR_SIZE - 1u));
        size_t chunk = GP100_FLASH_SECTOR_SIZE - sector_offset;
        if (chunk > size) {
            chunk = size;
        }
        if (!hooks->read(hooks->context, sector_address, workspace,
                         GP100_FLASH_SECTOR_SIZE)) {
            return GP100_FLASH_UPDATE_READ_FAILED;
        }
        if (memcmp(workspace + sector_offset, data, chunk) != 0) {
            if (gp100_flash_bytes_are_erased(workspace + sector_offset, chunk)) {
                gp100_flash_update_result_t result =
                    gp100_flash_program_erased_range(
                        hooks, workspace, workspace_size, address, data, chunk);
                if (result != GP100_FLASH_UPDATE_OK) {
                    return result;
                }
            } else {
                memcpy(workspace + sector_offset, data, chunk);
                if (!hooks->erase_sector(hooks->context, sector_address)) {
                    return GP100_FLASH_UPDATE_ERASE_FAILED;
                }
                for (size_t offset = 0u; offset < GP100_FLASH_SECTOR_SIZE;
                     offset += GP100_FLASH_PAGE_SIZE) {
                    const uint8_t *page = workspace + offset;

                    if (!gp100_flash_bytes_are_erased(page, GP100_FLASH_PAGE_SIZE) &&
                        !hooks->program_page(
                            hooks->context, sector_address + (uint32_t)offset,
                            page, GP100_FLASH_PAGE_SIZE)) {
                        return GP100_FLASH_UPDATE_PROGRAM_FAILED;
                    }
                }
            }
        }
        address += (uint32_t)chunk;
        data += chunk;
        size -= chunk;
    }
    return GP100_FLASH_UPDATE_OK;
}

gp100_flash_update_result_t gp100_flash_program_erased_range(
    const gp100_flash_update_hooks_t *hooks,
    uint8_t *page_workspace,
    size_t workspace_size,
    uint32_t address,
    const uint8_t *data,
    size_t size)
{
    if (hooks == NULL || hooks->program_page == NULL ||
        page_workspace == NULL ||
        workspace_size < GP100_FLASH_PAGE_SIZE ||
        data == NULL || size == 0u || size > UINT32_MAX - address) {
        return GP100_FLASH_UPDATE_INVALID;
    }
    while (size != 0u) {
        uint32_t page_address =
            address & ~(uint32_t)(GP100_FLASH_PAGE_SIZE - 1u);
        size_t page_offset =
            (size_t)(address & (GP100_FLASH_PAGE_SIZE - 1u));
        size_t chunk = GP100_FLASH_PAGE_SIZE - page_offset;

        if (chunk > size) {
            chunk = size;
        }
        memset(page_workspace, 0xff, GP100_FLASH_PAGE_SIZE);
        memcpy(page_workspace + page_offset, data, chunk);
        if (!hooks->program_page(
                hooks->context, page_address, page_workspace,
                GP100_FLASH_PAGE_SIZE)) {
            return GP100_FLASH_UPDATE_PROGRAM_FAILED;
        }
        address += (uint32_t)chunk;
        data += chunk;
        size -= chunk;
    }
    return GP100_FLASH_UPDATE_OK;
}
