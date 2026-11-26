#ifndef GP100_SETTINGS_STORE_H
#define GP100_SETTINGS_STORE_H

#include "flash_layout.h"
#include "flash_update.h"
#include "settings_record.h"

#define GP100_SETTINGS_STORE_SLOT_BYTES (2u * GP100_FLASH_PAGE_SIZE)
#define GP100_SETTINGS_STORE_SLOTS_PER_SECTOR \
    (GP100_FLASH_SECTOR_SIZE / GP100_SETTINGS_STORE_SLOT_BYTES)
#define GP100_SETTINGS_STORE_BYTES \
    (GP100_SETTINGS_SECTOR_COUNT * GP100_FLASH_SECTOR_SIZE)

typedef struct gp100_settings_writer {
    uint8_t dirty_countdown;
} gp100_settings_writer_t;

typedef enum gp100_settings_writer_result {
    GP100_SETTINGS_WRITER_INVALID = -1,
    GP100_SETTINGS_WRITER_IDLE = 0,
    GP100_SETTINGS_WRITER_WAITING,
    GP100_SETTINGS_WRITER_WRITTEN,
    GP100_SETTINGS_WRITER_IO_ERROR
} gp100_settings_writer_result_t;

bool gp100_settings_store_load(const uint8_t *region, const uint8_t *legacy,
                               gp100_settings_record_t *record);
void gp100_settings_writer_mark_dirty(gp100_settings_writer_t *writer);
gp100_settings_writer_result_t gp100_settings_writer_service(
    gp100_settings_writer_t *writer, gp100_settings_record_t *record,
    const uint8_t *region, const gp100_flash_update_hooks_t *hooks);

#endif
