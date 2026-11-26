#ifndef GP100_FLASH_COMMIT_H
#define GP100_FLASH_COMMIT_H

#include "flash_update.h"

/* A slot has a page-aligned payload followed by a dedicated commit page. */
bool gp100_flash_commit_read(const uint8_t *slot, size_t payload_size,
                             uint32_t tag, uint32_t *generation);
bool gp100_flash_commit_write(const gp100_flash_update_hooks_t *hooks,
                              uint32_t address, const uint8_t *payload,
                              size_t payload_size, uint32_t tag,
                              uint32_t generation);
bool gp100_flash_generation_newer(uint32_t candidate, uint32_t current);

#endif
