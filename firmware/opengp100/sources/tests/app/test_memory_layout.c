#include "app/memory_layout.h"

#include <stdint.h>
#include <stdio.h>

static int regions_are_contiguous(const gp100_memory_region_t *regions,
                                  uint32_t count)
{
    uint32_t index;
    for (index = 1u; index < count; ++index) {
        if (regions[index - 1u].address + regions[index - 1u].size !=
            regions[index].address) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const gp100_memory_region_t *last =
        &gp100_sdram_regions[GP100_SDRAM_REGION_COUNT - 1u];

    if (!regions_are_contiguous(gp100_dtcm_low_regions,
                                GP100_DTCM_LOW_REGION_COUNT) ||
        !regions_are_contiguous(gp100_itcm_regions,
                                GP100_ITCM_REGION_COUNT)) {
        fputs("work region bank is not contiguous\n", stderr);
        return 1;
    }
    if (gp100_dtcm_low_regions[0].address != UINT32_C(0x20029f80) ||
        gp100_dtcm_low_regions[8].address +
            gp100_dtcm_low_regions[8].size != UINT32_C(0x20030000) ||
        gp100_itcm_regions[0].address != UINT32_C(0x00037f80) ||
        gp100_itcm_regions[8].address +
            gp100_itcm_regions[8].size != UINT32_C(0x00050000)) {
        fputs("DTCM/ITCM region bounds changed\n", stderr);
        return 1;
    }
    if (last->address + last->size != UINT32_C(0x82000000) ||
        gp100_sdram_regions[
            GP100_SDRAM_EFFECT_PACKAGES_INDEX].address !=
            UINT32_C(0x80009aa4) ||
        gp100_sdram_regions[GP100_SDRAM_DELAY_WORK_INDEX].address !=
            UINT32_C(0x80b00000) ||
        gp100_sdram_regions[GP100_SDRAM_REVERB_WORK_INDEX].address !=
            UINT32_C(0x80c80000) ||
        gp100_sdram_regions[GP100_SDRAM_DETUNE_WORK_INDEX].address !=
            UINT32_C(0x80dc0000) ||
        gp100_sdram_regions[GP100_SDRAM_LOOPER_INDEX].address !=
            UINT32_C(0x80e80000) ||
        GP100_SDRAM_SNAPTONE_WORK_ADDRESS != UINT32_C(0x80e20000) ||
        GP100_SDRAM_SNAPTONE_WORK_ADDRESS +
            GP100_SDRAM_SNAPTONE_WORK_OBSERVED_BYTES >
            GP100_SDRAM_SNAPTONE_SUPPORT_ADDRESS ||
        GP100_SDRAM_SNAPTONE_SUPPORT_SOURCE_ADDRESS !=
            UINT32_C(0x8024da80) ||
        GP100_SDRAM_SNAPTONE_SUPPORT_ADDRESS != UINT32_C(0x80e40000) ||
        GP100_SDRAM_SNAPTONE_SUPPORT_ADDRESS +
            GP100_SDRAM_SNAPTONE_SUPPORT_INITIALIZED_BYTES >
            gp100_sdram_regions[GP100_SDRAM_LOOPER_INDEX].address ||
        GP100_SDRAM_SNAPTONE_SUPPORT_INITIALIZED_BYTES !=
            UINT32_C(0x0001d6f8) ||
        !gp100_memory_region_contains(last, UINT32_C(0x81ffffff), 1u) ||
        gp100_memory_region_contains(last, UINT32_C(0x81ffffff), 2u) ||
        gp100_memory_region_contains(NULL, 0u, 0u)) {
        fputs("SDRAM region bounds failed\n", stderr);
        return 1;
    }
    puts("memory layout tests passed");
    return 0;
}
