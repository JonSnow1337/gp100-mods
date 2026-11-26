#ifndef GP100_FLASH_UPDATE_H
#define GP100_FLASH_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_FLASH_SECTOR_SIZE 0x1000u
#define GP100_FLASH_PAGE_SIZE 0x100u
#define GP100_FLASH_PAGES_PER_SECTOR 16u

typedef struct gp100_flash_update_hooks {
    void *context;
    bool (*read)(void *context, uint32_t address,
                 uint8_t *data, size_t size);
    bool (*erase_sector)(void *context, uint32_t address);
    bool (*program_page)(void *context, uint32_t address,
                         const uint8_t *data, size_t size);
} gp100_flash_update_hooks_t;

typedef enum gp100_flash_update_result {
    GP100_FLASH_UPDATE_INVALID = -1,
    GP100_FLASH_UPDATE_OK = 0,
    GP100_FLASH_UPDATE_READ_FAILED,
    GP100_FLASH_UPDATE_ERASE_FAILED,
    GP100_FLASH_UPDATE_PROGRAM_FAILED
} gp100_flash_update_result_t;

bool gp100_flash_bytes_are_erased(const uint8_t *data, size_t size);

/*
 * Reproduce the factory sector-preserving writer: read a complete sector,
 * patch the requested range, erase, and program all sixteen pages. Requests
 * may span sector boundaries. workspace must hold one complete sector.
 */
gp100_flash_update_result_t gp100_flash_update(
    const gp100_flash_update_hooks_t *hooks,
    uint8_t *workspace,
    size_t workspace_size,
    uint32_t address,
    const uint8_t *data,
    size_t size);

/* Program an already-erased range without erasing neighboring journal data.
 * Pages are padded with 0xff; the caller owns the erased-state guarantee. */
gp100_flash_update_result_t gp100_flash_program_erased_range(
    const gp100_flash_update_hooks_t *hooks,
    uint8_t *page_workspace,
    size_t workspace_size,
    uint32_t address,
    const uint8_t *data,
    size_t size);

#ifdef __cplusplus
}
#endif

#endif
