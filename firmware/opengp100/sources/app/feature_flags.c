#include "feature_flags.h"

#include <string.h>

/* The app looks for this string to know the build has the live-preview switch (bit 2). */
__attribute__((used)) const char gp100_live_preview_marker[] = "GP100-LIVE-PREVIEW-1";

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t gp100_feature_flags_parse(const uint8_t *block, size_t bytes)
{
    uint32_t flags;

    if (block == NULL || bytes < GP100_FEATURE_FLAGS_BYTES || memcmp(block, "GPFL", 4) != 0) { return 0u; }
    if ((uint32_t)(block[4] | (block[5] << 8)) != 1u || (uint32_t)(block[6] | (block[7] << 8)) != GP100_FEATURE_FLAGS_BYTES) { return 0u; }
    flags = rd32(block + 8);
    return (rd32(block + 12) == ~flags) ? flags : 0u;
}

uint32_t gp100_feature_flags(void)
{
    if (*(const volatile char *)gp100_live_preview_marker != 'G') { return 0u; }   /* a real read keeps the marker in the image */
    return gp100_feature_flags_parse((const uint8_t *)(uintptr_t)(0x60000000u + GP100_FEATURE_FLAGS_FLASH_OFFSET),
                                     GP100_FEATURE_FLAGS_BYTES);
}
