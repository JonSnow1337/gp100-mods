#ifndef GP100_VENDOR_SYSEX_GENRE_H
#define GP100_VENDOR_SYSEX_GENRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/preset_record.h"
#include "vendor_sysex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_SYSEX_GENRE_NAME UINT32_C(0x11000006)
#define GP100_SYSEX_GENRE_NAME_CHANGED UINT32_C(0x11000008)
#define GP100_GENRE_NAME_SIZE 10u
#define GP100_SYSEX_GENRE_INDEX_BYTES 4u
#define GP100_SYSEX_GENRE_READ_BYTES \
    (GP100_SYSEX_GENRE_INDEX_BYTES + GP100_GENRE_NAME_SIZE)
#define GP100_SYSEX_GENRE_NOTIFICATION_NAME_BYTES 6u
#define GP100_SYSEX_GENRE_NOTIFICATION_BYTES \
    (GP100_SYSEX_GENRE_INDEX_BYTES + \
     GP100_SYSEX_GENRE_NOTIFICATION_NAME_BYTES)

typedef struct gp100_genre_catalog {
    uint8_t names[GP100_PRESET_GENRE_COUNT][GP100_GENRE_NAME_SIZE];
} gp100_genre_catalog_t;

typedef struct gp100_sysex_genre_update {
    uint16_t index;
    uint8_t name[GP100_GENRE_NAME_SIZE];
    size_t name_length;
} gp100_sysex_genre_update_t;

typedef enum gp100_sysex_genre_result {
    GP100_SYSEX_GENRE_INVALID = -1,
    GP100_SYSEX_GENRE_UNHANDLED = 0,
    GP100_SYSEX_GENRE_UPDATE = 1,
    GP100_SYSEX_GENRE_RESPONSE = 2
} gp100_sysex_genre_result_t;

void gp100_genre_catalog_reset(gp100_genre_catalog_t *catalog);

/* Install the English factory catalog rebuilt after an erased genre journal:
 * eleven recovered system names followed by fifty empty user slots. */
bool gp100_genre_catalog_make_factory(
    gp100_genre_catalog_t *catalog);

bool gp100_genre_name(
    const gp100_genre_catalog_t *catalog,
    uint16_t index,
    char name[GP100_GENRE_NAME_SIZE + 1u]);

gp100_sysex_genre_result_t gp100_sysex_genre_decode_update(
    const gp100_sysex_view_t *request,
    gp100_sysex_genre_update_t *update);

bool gp100_sysex_genre_apply(
    gp100_genre_catalog_t *catalog,
    gp100_preset_record_t *current_preset,
    const gp100_sysex_genre_update_t *update);

gp100_sysex_genre_result_t gp100_sysex_genre_build_state(
    const gp100_sysex_view_t *request,
    const gp100_genre_catalog_t *catalog,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool gp100_sysex_genre_build_notification(
    const gp100_genre_catalog_t *catalog,
    uint16_t index,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#ifdef __cplusplus
}
#endif

#endif
