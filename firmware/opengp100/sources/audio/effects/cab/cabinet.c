#include "cabinet.h"

#include <limits.h>
#include <string.h>

#define GP100_CABINET_FACTORY_FLASH_OFFSET 0x00680000u
#define GP100_CABINET_USER_FLASH_OFFSET 0x006e0000u
#define GP100_CABINET_Q23_SCALE (1.0f / 8388608.0f)
#define GP100_CABINET_LEVEL_FILTER 0.00022676f
#define GP100_CABINET_LEVEL_STEP 0.002f
#define GP100_CABINET_OUTPUT_SCALE 0.27f

static int32_t read_i32_le(const uint8_t *bytes)
{
    uint32_t raw = (uint32_t)bytes[0] |
                   ((uint32_t)bytes[1] << 8u) |
                   ((uint32_t)bytes[2] << 16u) |
                   ((uint32_t)bytes[3] << 24u);

    if (raw <= (uint32_t)INT32_MAX) {
        return (int32_t)raw;
    }
    return INT32_MIN + (int32_t)(raw - 0x80000000u);
}

static bool level_is_valid(float level_parameter)
{
    return level_parameter >= 0.0f && level_parameter <= 99.0f;
}

static void update_level(gp100_cabinet_t *cabinet,
                         float level_parameter)
{
    float target = level_parameter * 0.01f;
    float next = cabinet->level +
                 (target - cabinet->level) * GP100_CABINET_LEVEL_FILTER;

    if (next != target) {
        if (target <= next) {
            next -= GP100_CABINET_LEVEL_STEP;
            if (next < target) {
                next = target;
            }
        } else {
            next += GP100_CABINET_LEVEL_STEP;
            if (next > target) {
                next = target;
            }
        }
    }
    cabinet->level = next;
}

bool gp100_cabinet_flash_offset(size_t cabinet_index,
                                     uint32_t *flash_offset)
{
    if (flash_offset == NULL || cabinet_index >= GP100_CABINET_COUNT) {
        return false;
    }
    if (cabinet_index < GP100_CABINET_FACTORY_COUNT) {
        *flash_offset = GP100_CABINET_FACTORY_FLASH_OFFSET +
                        (uint32_t)cabinet_index *
                            GP100_CABINET_RECORD_SIZE;
    } else {
        *flash_offset = GP100_CABINET_USER_FLASH_OFFSET +
                        (uint32_t)(cabinet_index -
                                   GP100_CABINET_FACTORY_COUNT) *
                            GP100_CABINET_RECORD_SIZE;
    }
    return true;
}

void gp100_cabinet_reset(gp100_cabinet_t *cabinet)
{
    if (cabinet != NULL) {
        memset(cabinet, 0, sizeof(*cabinet));
    }
}

bool gp100_cabinet_load_record(gp100_cabinet_t *cabinet,
                                    const uint8_t *record,
                                    size_t record_size)
{
    float retained_level;
    size_t tap;

    if (cabinet == NULL || record == NULL ||
        record_size < GP100_CABINET_RECORD_SIZE) {
        return false;
    }

    retained_level = cabinet->level;
    memcpy(cabinet->name, record, GP100_CABINET_NAME_SIZE);
    cabinet->name[GP100_CABINET_NAME_SIZE] = '\0';
    for (tap = 0u; tap < GP100_CABINET_TAP_COUNT; ++tap) {
        size_t offset = GP100_CABINET_NAME_SIZE +
                        GP100_CABINET_RESERVED_SIZE + tap * 4u;
        cabinet->impulse[tap] =
            (float)read_i32_le(&record[offset]) * GP100_CABINET_Q23_SCALE;
    }
    if (!gp100_partitioned_convolution_initialize(
            &cabinet->convolution, cabinet->impulse,
            GP100_CABINET_TAP_COUNT, cabinet->impulse_spectra,
            GP100_CABINET_SPECTRUM_COUNT, cabinet->input_spectra,
            GP100_CABINET_SPECTRUM_COUNT)) {
        return false;
    }
    cabinet->level = retained_level;
    cabinet->configured = true;
    return true;
}

bool gp100_cabinet_process(gp100_cabinet_t *cabinet,
                               float level_parameter,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled)
{
    float mono[GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES];
    size_t block_start;

    if (cabinet == NULL || !cabinet->configured ||
        !level_is_valid(level_parameter) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count % GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES != 0u ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    update_level(cabinet, level_parameter);
    if (!enabled || frame_count == 0u) {
        return true;
    }

    for (block_start = 0u; block_start < frame_count;
         block_start += GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES) {
        size_t frame;

        for (frame = 0u;
             frame < GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
             ++frame) {
            mono[frame] = interleaved_stereo[(block_start + frame) * 2u];
        }
        if (!gp100_partitioned_convolution_process(
                &cabinet->convolution, mono,
                GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)) {
            return false;
        }
        for (frame = 0u;
             frame < GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
             ++frame) {
            size_t sample = (block_start + frame) * 2u;
            float output =
                mono[frame] * cabinet->level * GP100_CABINET_OUTPUT_SCALE;

            interleaved_stereo[sample] = output;
            interleaved_stereo[sample + 1u] = output;
        }
    }
    return true;
}
