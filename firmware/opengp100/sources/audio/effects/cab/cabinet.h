#ifndef GP100_CABINET_H
#define GP100_CABINET_H

#include "audio/dsp/partitioned_convolution.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_CABINET_RECORD_SIZE 0x1020u
#define GP100_CABINET_NAME_SIZE 16u
#define GP100_CABINET_RESERVED_SIZE 16u
#define GP100_CABINET_TAP_COUNT 1024u
#define GP100_CABINET_FACTORY_COUNT 40u
#define GP100_CABINET_USER_COUNT 20u
#define GP100_CABINET_COUNT 60u
#define GP100_CABINET_PARTITION_COUNT \
    (GP100_CABINET_TAP_COUNT / \
     GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)
#define GP100_CABINET_SPECTRUM_COUNT \
    (GP100_CABINET_PARTITION_COUNT * \
     GP100_PARTITIONED_CONVOLUTION_FFT_SAMPLES)

typedef struct gp100_cabinet {
    char name[GP100_CABINET_NAME_SIZE + 1u];
    float impulse[GP100_CABINET_TAP_COUNT];
    gp100_partitioned_convolution_t convolution;
    gp100_complex_t
        impulse_spectra[GP100_CABINET_SPECTRUM_COUNT];
    gp100_complex_t
        input_spectra[GP100_CABINET_SPECTRUM_COUNT];
    float level;
    bool configured;
} gp100_cabinet_t;

/* Returns the absolute offset of a factory (0..39) or user (40..59) record. */
bool gp100_cabinet_flash_offset(size_t cabinet_index,
                                     uint32_t *flash_offset);

void gp100_cabinet_reset(gp100_cabinet_t *cabinet);

/*
 * Decodes the 16-byte name, 16 reserved bytes, and 1,024 signed Q23 taps.
 * Loading a new impulse clears convolution history but preserves level slew.
 */
bool gp100_cabinet_load_record(gp100_cabinet_t *cabinet,
                                    const uint8_t *record,
                                    size_t record_size);

/*
 * Processes interleaved stereo in place. The left input drives a mono FIR and
 * its result is copied to both channels. frame_count must be a multiple of 64.
 * A bypassed call updates level but does not alter audio or convolution state.
 */
bool gp100_cabinet_process(gp100_cabinet_t *cabinet,
                               float level_parameter,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled);

#ifdef __cplusplus
}
#endif

#endif
