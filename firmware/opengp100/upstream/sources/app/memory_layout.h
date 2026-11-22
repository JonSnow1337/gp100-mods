#ifndef GP100_MEMORY_LAYOUT_H
#define GP100_MEMORY_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_DTCM_LOW_REGION_COUNT 9u
#define GP100_ITCM_REGION_COUNT 9u
#define GP100_SDRAM_REGION_COUNT 5u
#define GP100_SDRAM_EFFECT_PACKAGES_INDEX 0u
#define GP100_SDRAM_DELAY_WORK_INDEX 1u
#define GP100_SDRAM_REVERB_WORK_INDEX 2u
#define GP100_SDRAM_DETUNE_WORK_INDEX 3u
#define GP100_SDRAM_LOOPER_INDEX 4u
#define GP100_SDRAM_SNAPTONE_WORK_ADDRESS UINT32_C(0x80e20000)
#define GP100_SDRAM_SNAPTONE_WORK_OBSERVED_BYTES UINT32_C(0x000126c8)
#define GP100_SDRAM_SNAPTONE_SUPPORT_SOURCE_ADDRESS \
    UINT32_C(0x8024da80)
#define GP100_SDRAM_SNAPTONE_SUPPORT_ADDRESS UINT32_C(0x80e40000)
#define GP100_SDRAM_SNAPTONE_SUPPORT_INITIALIZED_BYTES \
    UINT32_C(0x0001d6f8)

typedef struct gp100_memory_region {
    uint32_t address;
    uint32_t size;
} gp100_memory_region_t;

extern const gp100_memory_region_t
    gp100_dtcm_low_regions[GP100_DTCM_LOW_REGION_COUNT];
extern const gp100_memory_region_t
    gp100_itcm_regions[GP100_ITCM_REGION_COUNT];
extern const gp100_memory_region_t
    gp100_sdram_regions[GP100_SDRAM_REGION_COUNT];

bool gp100_memory_region_contains(const gp100_memory_region_t *region,
                                       uint32_t address,
                                       size_t size);

#ifdef __cplusplus
}
#endif

#endif
