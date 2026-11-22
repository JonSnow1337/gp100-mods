#include "flash_commit.h"
#include "preset_record.h"

#include <string.h>

enum {
    TAG_OFFSET = 0,
    GENERATION_OFFSET = 4,
    GENERATION_INVERSE_OFFSET = 8,
    CRC_OFFSET = 12,
    CRC_INVERSE_OFFSET = 16,
    COMMIT_BYTES = 20
};

static uint32_t read_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void write_word(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static size_t commit_offset(size_t payload_size)
{
    return (payload_size + GP100_FLASH_PAGE_SIZE - 1u) &
           ~(size_t)(GP100_FLASH_PAGE_SIZE - 1u);
}

bool gp100_flash_commit_read(const uint8_t *slot, size_t payload_size,
                             uint32_t tag, uint32_t *generation)
{
    const uint8_t *commit = slot + commit_offset(payload_size);
    const uint32_t sequence = read_word(commit + GENERATION_OFFSET);
    const uint32_t crc = read_word(commit + CRC_OFFSET);

    /* The complement pairs reject changes to these words during a torn erase. */
    if (read_word(commit + TAG_OFFSET) != tag ||
        read_word(commit + GENERATION_INVERSE_OFFSET) != ~sequence ||
        read_word(commit + CRC_INVERSE_OFFSET) != ~crc ||
        gp100_crc32(slot, payload_size) != crc) {
        return false;
    }
    *generation = sequence;
    return true;
}

bool gp100_flash_commit_write(const gp100_flash_update_hooks_t *hooks,
                              uint32_t address, const uint8_t *payload,
                              size_t payload_size, uint32_t tag,
                              uint32_t generation)
{
    uint8_t page[GP100_FLASH_PAGE_SIZE];
    uint8_t commit[COMMIT_BYTES];

    for (size_t offset = 0u; offset < payload_size; offset += sizeof(page)) {
        const size_t remaining = payload_size - offset;
        const size_t length = remaining < sizeof(page) ? remaining : sizeof(page);
        const uint32_t page_address = address + (uint32_t)offset;
        if (!gp100_flash_bytes_are_erased(payload + offset, length) &&
            gp100_flash_program_erased_range(hooks, page, sizeof(page),
                page_address, payload + offset, length) != GP100_FLASH_UPDATE_OK) {
            return false;
        }
        if (!hooks->read(hooks->context, page_address, page,
                         length) || memcmp(page, payload + offset, length) != 0) {
            return false;
        }
    }

    const uint32_t crc = gp100_crc32(payload, payload_size);
    write_word(commit + TAG_OFFSET, tag);
    write_word(commit + GENERATION_OFFSET, generation);
    write_word(commit + GENERATION_INVERSE_OFFSET, ~generation);
    write_word(commit + CRC_OFFSET, crc);
    write_word(commit + CRC_INVERSE_OFFSET, ~crc);
    address += (uint32_t)commit_offset(payload_size);
    return gp100_flash_program_erased_range(hooks, page, sizeof(page), address,
               commit, sizeof(commit)) == GP100_FLASH_UPDATE_OK &&
           hooks->read(hooks->context, address, page, sizeof(commit)) &&
           memcmp(page, commit, sizeof(commit)) == 0;
}

bool gp100_flash_generation_newer(uint32_t candidate, uint32_t current)
{
    const uint32_t distance = candidate - current;
    return distance != 0u && distance < UINT32_C(0x80000000);
}
