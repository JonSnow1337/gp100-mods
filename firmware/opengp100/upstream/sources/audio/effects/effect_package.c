#include "effect_package.h"

static const uint16_t directory_offsets[GP100_EFFECT_FAMILY_COUNT] = {
    0x008u, 0x08cu, 0x128u, 0x2c4u, 0x2d8u,
    0x2e4u, 0x300u, 0x364u, 0x3c8u
};

static const uint8_t package_counts[GP100_EFFECT_FAMILY_COUNT] = {
    16u, 19u, 51u, 2u, 1u, 3u, 12u, 12u, 10u
};

static uint32_t load_u32_le(const uint8_t input[4])
{
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) |
           ((uint32_t)input[3] << 24);
}

size_t gp100_effect_package_count(gp100_effect_family_t family)
{
    if ((unsigned)family >= GP100_EFFECT_FAMILY_COUNT) {
        return 0u;
    }
    return package_counts[family];
}

bool gp100_effect_package_descriptor(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_descriptor_t *descriptor)
{
    size_t entry_offset;
    uint32_t data_offset;
    uint32_t data_size;
    uint64_t address;
    uint64_t end;

    if (package_root == NULL || descriptor == NULL ||
        (unsigned)family >= GP100_EFFECT_FAMILY_COUNT ||
        package_index >= package_counts[family] ||
        package_root_size < GP100_EFFECT_PACKAGE_DIRECTORY_BYTES) {
        return false;
    }
    entry_offset = directory_offsets[family] + package_index * 8u;
    if (load_u32_le(package_root + directory_offsets[family] - 4u) !=
            package_counts[family] ||
        entry_offset + 8u > GP100_EFFECT_PACKAGE_DIRECTORY_BYTES) {
        return false;
    }
    data_offset = load_u32_le(package_root + entry_offset);
    data_size = load_u32_le(package_root + entry_offset + 4u);
    address = (uint64_t)root_address + 4u + data_offset;
    end = 4u + (uint64_t)data_offset + data_size;
    if (address > UINT32_MAX || end > package_root_size) {
        return false;
    }
    descriptor->address = (uint32_t)address;
    descriptor->size = data_size;
    descriptor->offset = data_offset;
    return true;
}

bool gp100_effect_package_header(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_header_t *header)
{
    gp100_effect_package_descriptor_t descriptor;
    size_t header_offset;

    if (header == NULL ||
        !gp100_effect_package_descriptor(
            package_root, package_root_size, root_address, family,
            package_index, &descriptor) ||
        descriptor.size < 16u || descriptor.address < root_address) {
        return false;
    }
    header_offset = (size_t)(descriptor.address - root_address);
    if (header_offset > package_root_size - 16u ||
        load_u32_le(package_root + header_offset + 12u) !=
            GP100_EFFECT_PACKAGE_HEADER_MAGIC) {
        return false;
    }
    header->data_address = load_u32_le(package_root + header_offset);
    header->initialize_entry = load_u32_le(package_root + header_offset + 4u);
    header->process_entry = load_u32_le(package_root + header_offset + 8u);
    return true;
}

bool gp100_effect_package_callbacks_fit(
    gp100_effect_family_t family,
    const gp100_effect_package_header_t *header)
{
    const gp100_memory_region_t *region;

    if (header == NULL || (unsigned)family >= GP100_EFFECT_FAMILY_COUNT) {
        return false;
    }
    region = &gp100_itcm_regions[family];
    return (header->initialize_entry & 1u) != 0u &&
           (header->process_entry & 1u) != 0u &&
           gp100_memory_region_contains(
               region, header->initialize_entry & ~UINT32_C(1), 2u) &&
           gp100_memory_region_contains(
               region, header->process_entry & ~UINT32_C(1), 2u);
}

bool gp100_effect_package_load_plan(
    const uint8_t *package_root,
    size_t package_root_size,
    uint32_t root_address,
    gp100_effect_family_t family,
    size_t package_index,
    gp100_effect_package_load_plan_t *plan)
{
    gp100_effect_package_load_plan_t candidate;

    if (plan == NULL || (unsigned)family >= GP100_EFFECT_FAMILY_COUNT ||
        !gp100_effect_package_descriptor(
            package_root, package_root_size, root_address, family,
            package_index, &candidate.package) ||
        !gp100_effect_package_header(
            package_root, package_root_size, root_address, family,
            package_index, &candidate.header) ||
        !gp100_effect_package_callbacks_fit(family, &candidate.header)) {
        return false;
    }
    candidate.code_region = gp100_itcm_regions[family];
    candidate.state_region = gp100_dtcm_low_regions[family];
    if (candidate.package.size > candidate.code_region.size) {
        return false;
    }
    *plan = candidate;
    return true;
}
