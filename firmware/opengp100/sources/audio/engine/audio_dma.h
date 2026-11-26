#ifndef GP100_AUDIO_DMA_H
#define GP100_AUDIO_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_AUDIO_MONITOR_SAMPLES 0x480u

typedef struct gp100_audio_monitor_ring {
    int32_t samples[GP100_AUDIO_MONITOR_SAMPLES];
    size_t write_index;
    size_t read_index;
} gp100_audio_monitor_ring_t;

typedef enum gp100_audio_monitor_read_result {
    GP100_AUDIO_MONITOR_READ_INVALID = -1,
    GP100_AUDIO_MONITOR_READ_UNDERRUN = 0,
    GP100_AUDIO_MONITOR_READ_DATA = 1
} gp100_audio_monitor_read_result_t;

/* eDMA ELINK bit is ignored; the BITER midpoint is compared with CITER. */
bool gp100_audio_dma_uses_second_half(
    uint16_t beginning_iteration,
    uint16_t current_iteration);

void gp100_audio_unpack_sai24(
    int32_t *samples,
    const int32_t *sai_words,
    size_t sample_count);

void gp100_audio_pack_sai24(
    int32_t *sai_words,
    const int32_t *samples,
    size_t sample_count);

/* Replaces each stereo frame with (right / 2 - left / 2) on both channels. */
bool gp100_audio_fold_stereo_difference(
    int32_t *interleaved_stereo,
    size_t frame_count);

void gp100_audio_monitor_ring_reset(
    gp100_audio_monitor_ring_t *ring);

bool gp100_audio_monitor_ring_write(
    gp100_audio_monitor_ring_t *ring,
    const int32_t *interleaved_stereo,
    size_t frame_count);

gp100_audio_monitor_read_result_t
gp100_audio_monitor_ring_read(
    gp100_audio_monitor_ring_t *ring,
    int32_t *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
