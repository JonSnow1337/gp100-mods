#ifndef GP100_EFFECT_PACKAGE_H
#define GP100_EFFECT_PACKAGE_H

#include "storage/preset_record.h"
#include "app/memory_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EFFECT_PACKAGE_ROOT_ADDRESS UINT32_C(0x80009aa4)
#define GP100_EFFECT_PACKAGE_DIRECTORY_BYTES 0x418u

typedef struct gp100_effect_package_descriptor {
    uint32_t address;
    uint32_t size;
    uint32_t offset;
} gp100_effect_package_descriptor_t;

#define GP100_EFFECT_PACKAGE_HEADER_MAGIC UINT32_C(0x5555aaaa)

typedef struct gp100_effect_package_header {
    uint32_t data_address;
    uint32_t initialize_entry;
    uint32_t process_entry;
} gp100_effect_package_header_t;

typedef struct gp100_effect_package_load_plan {
    gp100_effect_package_descriptor_t package;
    gp100_effect_package_header_t header;
    gp100_memory_region_t code_region;
    gp100_memory_region_t state_region;
} gp100_effect_package_load_plan_t;

size_t gp100_effect_package_count(gp100_effect_family_t family);

bool gp100_effect_package_descriptor(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_descriptor_t *descriptor);

bool gp100_effect_package_header(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_header_t *header);

bool gp100_effect_package_callbacks_fit(
    gp100_effect_family_t family,
    const gp100_effect_package_header_t *header);

bool gp100_effect_package_load_plan(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_load_plan_t *plan);

#ifdef __cplusplus
}
#endif

#endif
