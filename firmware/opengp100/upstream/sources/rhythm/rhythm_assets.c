#include "rhythm_assets.h"

#include <stdalign.h>
#include <string.h>

#define FACTORY_SAMPLE_LENGTH_NUMERATOR UINT32_C(44)
#define FACTORY_SAMPLE_LENGTH_SHIFT 6u

static uint32_t load_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static bool range_fits(size_t offset, size_t length, size_t size)
{
    return offset <= size && length <= size - offset;
}

bool gp100_rhythm_assets_parse(
    gp100_rhythm_assets_t *assets,
    const uint8_t *pattern_bank,
    size_t pattern_bank_size,
    const uint8_t *sample_bank,
    size_t sample_bank_size)
{
    const size_t pattern_header_size =
        sizeof(uint32_t) *
        (1u + GP100_RHYTHM_FACTORY_PATTERN_COUNT);
    const size_t sample_header_size =
        sizeof(uint32_t) *
        (1u + 2u * GP100_RHYTHM_FACTORY_SAMPLE_COUNT);
    gp100_rhythm_assets_t next;
    size_t index;

    if (assets == NULL || pattern_bank == NULL || sample_bank == NULL ||
        pattern_bank_size < pattern_header_size ||
        sample_bank_size < sample_header_size ||
        load_u32_le(pattern_bank) !=
            GP100_RHYTHM_FACTORY_PATTERN_COUNT ||
        load_u32_le(sample_bank) !=
            GP100_RHYTHM_FACTORY_SAMPLE_COUNT) {
        return false;
    }
    memset(&next, 0, sizeof(next));

    for (index = 0u;
         index < GP100_RHYTHM_FACTORY_PATTERN_COUNT;
         ++index) {
        size_t offset = load_u32_le(
            pattern_bank + sizeof(uint32_t) * (1u + index));
        size_t bytes = sizeof(uint16_t) *
            GP100_RHYTHM_PATTERN_EVENTS;

        if (offset < pattern_header_size ||
            !range_fits(offset, bytes, pattern_bank_size) ||
            ((uintptr_t)(pattern_bank + offset) % alignof(uint16_t)) != 0u) {
            return false;
        }
        next.patterns[index].events =
            (const uint16_t *)(const void *)(pattern_bank + offset);
        next.patterns[index].event_count =
            GP100_RHYTHM_PATTERN_EVENTS;
    }

    for (index = 0u;
         index < GP100_RHYTHM_FACTORY_SAMPLE_COUNT;
         ++index) {
        size_t offset = load_u32_le(
            sample_bank + sizeof(uint32_t) * (1u + index));
        uint32_t raw_length = load_u32_le(
            sample_bank + sizeof(uint32_t) *
                (1u + GP100_RHYTHM_FACTORY_SAMPLE_COUNT + index));
        uint64_t blocks_wide =
            ((uint64_t)raw_length * FACTORY_SAMPLE_LENGTH_NUMERATOR) >>
            FACTORY_SAMPLE_LENGTH_SHIFT;
        uint32_t blocks;
        size_t bytes;

        if (blocks_wide > UINT16_MAX) {
            return false;
        }
        blocks = (uint32_t)blocks_wide;
        bytes = (size_t)blocks * GP100_RHYTHM_BLOCK_SAMPLES *
            sizeof(int16_t);
        if (offset < sample_header_size ||
            !range_fits(offset, bytes, sample_bank_size) ||
            ((uintptr_t)(sample_bank + offset) % alignof(int16_t)) != 0u) {
            return false;
        }
        next.samples[index].pcm =
            (const int16_t *)(const void *)(sample_bank + offset);
        next.samples[index].block_count = (uint16_t)blocks;
    }

    *assets = next;
    return true;
}
