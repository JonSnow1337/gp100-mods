#ifndef GP100_FACTORY_RESET_PLAN_H
#define GP100_FACTORY_RESET_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_FACTORY_RESET_SECTOR_BYTES UINT32_C(0x1000)
#define GP100_FACTORY_RESET_SECTOR_STEPS 167u

/*
 * Return the logical NOR offset erased by one factory-reset step. This is a
 * pure plan: callers retain ownership of flash access, retries, power-failure
 * policy, progress publication, and post-reset reload.
 */
bool gp100_factory_reset_sector_address(
    size_t step,
    uint32_t *address);

#ifdef __cplusplus
}
#endif

#endif
