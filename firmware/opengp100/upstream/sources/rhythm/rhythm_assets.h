#ifndef GP100_RHYTHM_ASSETS_H
#define GP100_RHYTHM_ASSETS_H

#include "rhythm_player.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded V2.1 application-image locations. The standalone firmware composer
 * restores these ranges at the same logical flash offsets. */
#define GP100_RHYTHM_SAMPLE_BANK_OFFSET UINT32_C(0x00133ddc)
#define GP100_RHYTHM_SAMPLE_BANK_SIZE UINT32_C(0x000935e4)
#define GP100_RHYTHM_PATTERN_BANK_OFFSET UINT32_C(0x001c7414)
#define GP100_RHYTHM_PATTERN_BANK_SIZE UINT32_C(0x0000447e)

typedef struct gp100_rhythm_assets {
    gp100_rhythm_pattern_t
        patterns[GP100_RHYTHM_FACTORY_PATTERN_COUNT];
    gp100_rhythm_sample_t
        samples[GP100_RHYTHM_FACTORY_SAMPLE_COUNT];
} gp100_rhythm_assets_t;

/* Parse the two factory offset-table containers without copying PCM. Both
 * banks and every payload must remain alive while the resulting descriptors
 * are in use. Factory sample lengths are converted to 64-sample blocks with
 * the observed (length * 44) >> 6 equation. */
bool gp100_rhythm_assets_parse(
    gp100_rhythm_assets_t *assets,
    const uint8_t *pattern_bank,
    size_t pattern_bank_size,
    const uint8_t *sample_bank,
    size_t sample_bank_size);

#ifdef __cplusplus
}
#endif

#endif
