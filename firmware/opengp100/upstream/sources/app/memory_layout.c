#include "memory_layout.h"

#include <stddef.h>

const gp100_memory_region_t
    gp100_dtcm_low_regions[GP100_DTCM_LOW_REGION_COUNT] = {
        {UINT32_C(0x20029f80), UINT32_C(0x1800)},
        {UINT32_C(0x2002b780), UINT32_C(0x0c00)},
        {UINT32_C(0x2002c380), UINT32_C(0x1000)},
        {UINT32_C(0x2002d380), UINT32_C(0x0400)},
        {UINT32_C(0x2002d780), UINT32_C(0x0080)},
        {UINT32_C(0x2002d800), UINT32_C(0x0400)},
        {UINT32_C(0x2002dc00), UINT32_C(0x1000)},
        {UINT32_C(0x2002ec00), UINT32_C(0x1000)},
        {UINT32_C(0x2002fc00), UINT32_C(0x0400)}
    };

const gp100_memory_region_t
    gp100_itcm_regions[GP100_ITCM_REGION_COUNT] = {
        {UINT32_C(0x00037f80), UINT32_C(0x3000)},
        {UINT32_C(0x0003af80), UINT32_C(0x2800)},
        {UINT32_C(0x0003d780), UINT32_C(0x5000)},
        {UINT32_C(0x00042780), UINT32_C(0x2000)},
        {UINT32_C(0x00044780), UINT32_C(0x0080)},
        {UINT32_C(0x00044800), UINT32_C(0x2000)},
        {UINT32_C(0x00046800), UINT32_C(0x2800)},
        {UINT32_C(0x00049000), UINT32_C(0x3000)},
        {UINT32_C(0x0004c000), UINT32_C(0x4000)}
    };

const gp100_memory_region_t
    gp100_sdram_regions[GP100_SDRAM_REGION_COUNT] = {
        {UINT32_C(0x80009aa4), UINT32_C(0x000ee5f1)},
        {UINT32_C(0x80b00000), UINT32_C(0x00180000)},
        {UINT32_C(0x80c80000), UINT32_C(0x00080000)},
        {UINT32_C(0x80dc0000), UINT32_C(0x00060000)},
        {UINT32_C(0x80e80000), UINT32_C(0x01180000)}
    };

bool gp100_memory_region_contains(const gp100_memory_region_t *region,
                                       uint32_t address,
                                       size_t size)
{
    uint64_t start;
    uint64_t end;

    if (region == NULL) {
        return false;
    }
    start = region->address;
    end = start + region->size;
    if ((uint64_t)address < start || (uint64_t)address > end) {
        return false;
    }
    return (uint64_t)size <= end - address;
}
