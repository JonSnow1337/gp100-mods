#ifndef GP100_GENRE_RECORD_H
#define GP100_GENRE_RECORD_H

#include "protocol/midi/sysex/vendor_sysex_genre.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_GENRE_FLASH_BASE UINT32_C(0x007f9000)
#define GP100_GENRE_FLASH_BYTES 0x5000u
#define GP100_GENRE_SECTOR_COUNT 5u
#define GP100_GENRE_PAYLOAD_SIZE 0x262u
#define GP100_GENRE_RECORD_SIZE 0x266u
#define GP100_GENRE_CRC_OFFSET 0x262u
#define GP100_GENRE_SLOT_COUNT 33u

typedef struct gp100_genre_record {
    uint8_t bytes[GP100_GENRE_RECORD_SIZE];
} gp100_genre_record_t;

typedef struct gp100_genre_writer {
    size_t next_slot;
} gp100_genre_writer_t;

typedef struct gp100_genre_writer_hooks {
    void *context;
    bool (*erase_sector)(void *context, uint32_t address);
    bool (*write)(void *context, uint32_t address,
                  const uint8_t *data, size_t size);
} gp100_genre_writer_hooks_t;

typedef enum gp100_genre_writer_result {
    GP100_GENRE_WRITER_INVALID = -1,
    GP100_GENRE_WRITER_WRITTEN = 0,
    GP100_GENRE_WRITER_IO_ERROR
} gp100_genre_writer_result_t;

uint32_t gp100_genre_record_stored_crc(
    const gp100_genre_record_t *record);
bool gp100_genre_record_validate(
    const gp100_genre_record_t *record);
bool gp100_genre_record_encode(
    const gp100_genre_catalog_t *catalog,
    gp100_genre_record_t *record);
bool gp100_genre_record_decode(
    const gp100_genre_record_t *record,
    gp100_genre_catalog_t *catalog);

bool gp100_genre_record_slot_address(size_t slot, uint32_t *address);

/* Recover the newest valid record exactly as factory loader 0x19d48 does. */
bool gp100_genre_record_find_latest(
    const uint8_t *flash_region,
    size_t flash_region_size,
    gp100_genre_catalog_t *catalog,
    size_t *record_slot,
    size_t *next_slot);

bool gp100_genre_writer_init(
    gp100_genre_writer_t *writer,
    size_t next_slot);

/* Append one record. A full journal is erased as one five-sector generation. */
gp100_genre_writer_result_t gp100_genre_writer_append(
    gp100_genre_writer_t *writer,
    const gp100_genre_catalog_t *catalog,
    const gp100_genre_writer_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
