#include "audio/effects/effect_package.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void store_u32_le(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

static uint32_t load_u32_le(const uint8_t input[4])
{
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) |
           ((uint32_t)input[3] << 24);
}

static int package_contains_aligned_address(
    const uint8_t *package_root,
    size_t package_root_size,
    const gp100_effect_package_descriptor_t *descriptor,
    const gp100_memory_region_t *region,
    int require_region_base)
{
    size_t offset;
    size_t index;

    if (descriptor->address < GP100_EFFECT_PACKAGE_ROOT_ADDRESS) {
        return 0;
    }
    offset = (size_t)(descriptor->address -
                      GP100_EFFECT_PACKAGE_ROOT_ADDRESS);
    if (offset > package_root_size ||
        descriptor->size > package_root_size - offset) {
        return 0;
    }
    for (index = 0u; index + 4u <= descriptor->size; index += 4u) {
        uint32_t address = load_u32_le(package_root + offset + index);
        if ((require_region_base && address == region->address) ||
            (!require_region_base &&
             gp100_memory_region_contains(region, address, 1u))) {
            return 1;
        }
    }
    return 0;
}

static int test_rejected_inputs(uint8_t *package_root, size_t package_root_size)
{
    const gp100_effect_family_t invalid_family =
        (gp100_effect_family_t)GP100_EFFECT_FAMILY_COUNT;
    gp100_effect_package_descriptor_t descriptor;
    gp100_effect_package_header_t header = {
        UINT32_C(0x000478fa), UINT32_C(0x00046891), UINT32_C(0x00046c71)
    };
    gp100_effect_package_load_plan_t plan;
    uint8_t oversized_root[0x1000];

    if (gp100_effect_package_count(invalid_family) != 0u ||
        gp100_effect_package_descriptor(
            NULL, package_root_size,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &descriptor) ||
        gp100_effect_package_descriptor(
            package_root, GP100_EFFECT_PACKAGE_DIRECTORY_BYTES - 1u,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &descriptor) ||
        gp100_effect_package_descriptor(
            package_root, package_root_size,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            invalid_family, 0u, &descriptor) ||
        gp100_effect_package_descriptor(
            package_root, package_root_size,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, NULL) ||
        gp100_effect_package_header(
            package_root, package_root_size,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, NULL) ||
        gp100_effect_package_callbacks_fit(invalid_family, &header) ||
        gp100_effect_package_callbacks_fit(
            GP100_EFFECT_MODULATION, NULL) ||
        gp100_effect_package_load_plan(
            package_root, package_root_size,
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, NULL)) {
        return 1;
    }

    store_u32_le(package_root + 0x300u, UINT32_MAX);
    if (gp100_effect_package_descriptor(
            package_root, package_root_size, UINT32_MAX,
            GP100_EFFECT_MODULATION, 0u, &descriptor)) {
        return 2;
    }
    store_u32_le(package_root + 0x300u, 0x500u);

    header.process_entry &= ~UINT32_C(1);
    if (gp100_effect_package_callbacks_fit(
            GP100_EFFECT_MODULATION, &header)) {
        return 3;
    }
    header.process_entry |= UINT32_C(1);

    memset(oversized_root, 0, sizeof(oversized_root));
    store_u32_le(oversized_root + 0x2d4u, 1u);
    store_u32_le(oversized_root + 0x2d8u, 0x500u);
    store_u32_le(
        oversized_root + 0x2dcu,
        gp100_itcm_regions[GP100_EFFECT_CABINET].size + 1u);
    store_u32_le(oversized_root + 0x504u, UINT32_C(0x000447aa));
    store_u32_le(oversized_root + 0x508u, UINT32_C(0x000447a3));
    store_u32_le(oversized_root + 0x50cu, UINT32_C(0x000447a7));
    store_u32_le(oversized_root + 0x510u,
                 GP100_EFFECT_PACKAGE_HEADER_MAGIC);
    if (gp100_effect_package_load_plan(
            oversized_root, sizeof(oversized_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_CABINET, 0u, &plan)) {
        return 4;
    }
    return 0;
}

static int test_shipped_directory(const char *path)
{
    static const uint32_t user_amp_addresses[5] = {
        UINT32_C(0x800a7cb4), UINT32_C(0x800ab1b0),
        UINT32_C(0x800ae6ac), UINT32_C(0x800b1ba8),
        UINT32_C(0x800b50a4)
    };
    const long root_file_offset = 0x9aa4L;
    FILE *file;
    uint8_t *root;
    long file_size;
    size_t root_size;
    gp100_effect_package_descriptor_t descriptor;
    gp100_effect_package_header_t header;
    gp100_effect_package_load_plan_t plan;
    size_t family_index;
    size_t package_index;
    size_t delay_work_packages = 0u;
    size_t reverb_work_packages = 0u;
    int detune_work_base_found = 0;
    size_t user_amp_reference_offset = 0u;
    int result = 0;

    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 ||
        (file_size = ftell(file)) <= root_file_offset ||
        fseek(file, root_file_offset, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return 1;
    }
    root_size = (size_t)(file_size - root_file_offset);
    root = (uint8_t *)malloc(root_size);
    if (root == NULL || fread(root, 1u, root_size, file) != root_size) {
        free(root);
        fclose(file);
        return 2;
    }
    fclose(file);
    if (!gp100_effect_package_descriptor(
            root, root_size, GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &descriptor) ||
        descriptor.address != UINT32_C(0x800bec8c) ||
        descriptor.size != UINT32_C(0x119c)) {
        result = 3;
    } else if (!gp100_effect_package_header(
                   root, root_size,
                   GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                   GP100_EFFECT_MODULATION, 0u, &header) ||
               header.data_address != UINT32_C(0x000478fa) ||
               header.initialize_entry != UINT32_C(0x00046891) ||
               header.process_entry != UINT32_C(0x00046c71) ||
               !gp100_effect_package_callbacks_fit(
                   GP100_EFFECT_MODULATION, &header)) {
        result = 4;
    } else if (!gp100_effect_package_descriptor(
                   root, root_size,
                   GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                   GP100_EFFECT_CABINET, 0u, &descriptor) ||
               descriptor.address != UINT32_C(0x800ba4ec) ||
               descriptor.size != UINT32_C(0x34)) {
        result = 5;
    } else if (!gp100_effect_package_header(
                   root, root_size,
                   GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                   GP100_EFFECT_CABINET, 0u, &header) ||
               header.data_address != UINT32_C(0x000447aa) ||
               header.initialize_entry != UINT32_C(0x000447a3) ||
               header.process_entry != UINT32_C(0x000447a7) ||
               !gp100_effect_package_callbacks_fit(
                   GP100_EFFECT_CABINET, &header)) {
        result = 6;
    }
    for (family_index = 0u;
         result == 0 && family_index < GP100_EFFECT_FAMILY_COUNT;
         ++family_index) {
        gp100_effect_family_t family =
            (gp100_effect_family_t)family_index;
        for (package_index = 0u;
             package_index < gp100_effect_package_count(family);
             ++package_index) {
            if (!gp100_effect_package_descriptor(
                    root, root_size,
                    GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                    family, package_index, &descriptor) ||
                !gp100_effect_package_header(
                    root, root_size,
                    GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                    family, package_index, &header) ||
                !gp100_effect_package_callbacks_fit(family, &header) ||
                !gp100_effect_package_load_plan(
                    root, root_size,
                    GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                    family, package_index, &plan) ||
                plan.code_region.address !=
                    gp100_itcm_regions[family_index].address ||
                plan.state_region.address !=
                    gp100_dtcm_low_regions[family_index].address) {
                result = 7;
                break;
            }
            if (family == GP100_EFFECT_DELAY &&
                package_contains_aligned_address(
                    root, root_size, &descriptor,
                    &gp100_sdram_regions[
                        GP100_SDRAM_DELAY_WORK_INDEX],
                    0)) {
                ++delay_work_packages;
            }
            if (family == GP100_EFFECT_REVERB &&
                package_contains_aligned_address(
                    root, root_size, &descriptor,
                    &gp100_sdram_regions[
                        GP100_SDRAM_REVERB_WORK_INDEX],
                    0)) {
                ++reverb_work_packages;
            }
            if (family == GP100_EFFECT_MODULATION &&
                package_index == 3u) {
                detune_work_base_found = package_contains_aligned_address(
                    root, root_size, &descriptor,
                    &gp100_sdram_regions[
                        GP100_SDRAM_DETUNE_WORK_INDEX],
                    1);
            }
        }
    }
    if (result == 0 &&
        (delay_work_packages !=
             gp100_effect_package_count(GP100_EFFECT_DELAY) ||
         reverb_work_packages !=
             gp100_effect_package_count(GP100_EFFECT_REVERB) ||
         !detune_work_base_found)) {
        result = 8;
    }
    for (package_index = 46u; result == 0 && package_index < 51u;
         ++package_index) {
        size_t package_offset;

        if (!gp100_effect_package_descriptor(
                root, root_size, GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                GP100_EFFECT_AMPLIFIER, package_index, &descriptor) ||
            !gp100_effect_package_header(
                root, root_size, GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
                GP100_EFFECT_AMPLIFIER, package_index, &header) ||
            descriptor.address != user_amp_addresses[package_index - 46u] ||
            descriptor.size != UINT32_C(0x34fc) ||
            header.data_address != UINT32_C(0x80e40000) ||
            header.initialize_entry != UINT32_C(0x0003e271) ||
            header.process_entry != UINT32_C(0x0003e3d9)) {
            result = 9;
            break;
        }
        package_offset = (size_t)(descriptor.address -
                                  GP100_EFFECT_PACKAGE_ROOT_ADDRESS);
        if (package_index == 46u) {
            user_amp_reference_offset = package_offset;
        } else if (memcmp(root + user_amp_reference_offset,
                          root + package_offset,
                          descriptor.size) != 0) {
            result = 10;
        }
    }
    free(root);
    return result;
}

int main(int argc, char **argv)
{
    static uint8_t package_root[0x1000];
    gp100_effect_package_descriptor_t descriptor;
    gp100_effect_package_header_t header;
    gp100_effect_package_load_plan_t plan;

    memset(package_root, 0, sizeof(package_root));
    store_u32_le(package_root + 0x2fcu, 12u);
    store_u32_le(package_root + 0x300u, 0x500u);
    store_u32_le(package_root + 0x304u, 0x120u);
    store_u32_le(package_root + 0x504u, 0x478fau);
    store_u32_le(package_root + 0x508u, 0x46891u);
    store_u32_le(package_root + 0x50cu, 0x46c71u);
    store_u32_le(package_root + 0x510u,
                 GP100_EFFECT_PACKAGE_HEADER_MAGIC);
    if (test_rejected_inputs(package_root, sizeof(package_root)) != 0) {
        fputs("effect package rejected-input validation failed\n", stderr);
        return 1;
    }
    if (gp100_effect_package_count(GP100_EFFECT_PRE_FX) != 16u ||
        gp100_effect_package_count(GP100_EFFECT_CABINET) != 1u ||
        gp100_effect_package_count(GP100_EFFECT_MODULATION) != 12u ||
        !gp100_effect_package_descriptor(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &descriptor) ||
        descriptor.offset != 0x500u || descriptor.size != 0x120u ||
        descriptor.address != UINT32_C(0x80009fa8) ||
        !gp100_effect_package_header(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &header) ||
        !gp100_effect_package_callbacks_fit(
            GP100_EFFECT_MODULATION, &header) ||
        !gp100_effect_package_load_plan(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &plan) ||
        plan.code_region.address != UINT32_C(0x00046800) ||
        plan.state_region.address != UINT32_C(0x2002dc00)) {
        fputs("effect package descriptor failed\n", stderr);
        return 2;
    }
    if (gp100_effect_package_descriptor(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 12u, &descriptor)) {
        fputs("effect package bounds failed\n", stderr);
        return 3;
    }
    store_u32_le(package_root + 0x2fcu, 11u);
    if (gp100_effect_package_descriptor(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &descriptor)) {
        fputs("effect package count validation failed\n", stderr);
        return 4;
    }
    store_u32_le(package_root + 0x2fcu, 12u);
    header.process_entry = UINT32_C(0x00049001);
    if (gp100_effect_package_callbacks_fit(
            GP100_EFFECT_MODULATION, &header)) {
        fputs("effect package callback region validation failed\n", stderr);
        return 5;
    }
    store_u32_le(package_root + 0x510u, 0u);
    if (gp100_effect_package_header(
            package_root, sizeof(package_root),
            GP100_EFFECT_PACKAGE_ROOT_ADDRESS,
            GP100_EFFECT_MODULATION, 0u, &header)) {
        fputs("effect package header magic validation failed\n", stderr);
        return 6;
    }
    if (argc != 2 || test_shipped_directory(argv[1]) != 0) {
        fputs("shipped effect package directory failed\n", stderr);
        return 7;
    }
    return 0;
}
