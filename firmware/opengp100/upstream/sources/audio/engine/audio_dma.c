#include "audio_dma.h"

#include <string.h>

#define GP100_EDMA_ITERATION_MASK UINT16_C(0x7fff)

bool gp100_audio_dma_uses_second_half(
    uint16_t beginning_iteration,
    uint16_t current_iteration)
{
    uint16_t beginning =
        (uint16_t)(beginning_iteration & GP100_EDMA_ITERATION_MASK);
    uint16_t current =
        (uint16_t)(current_iteration & GP100_EDMA_ITERATION_MASK);
    return (uint16_t)(beginning >> 1) < current;
}

void gp100_audio_unpack_sai24(
    int32_t *samples,
    const int32_t *sai_words,
    size_t sample_count)
{
    size_t sample;
    if (samples == NULL || sai_words == NULL) {
        return;
    }
    for (sample = 0u; sample < sample_count; ++sample) {
        samples[sample] = sai_words[sample] >> 8;
    }
}

void gp100_audio_pack_sai24(
    int32_t *sai_words,
    const int32_t *samples,
    size_t sample_count)
{
    size_t sample;
    if (sai_words == NULL || samples == NULL) {
        return;
    }
    for (sample = 0u; sample < sample_count; ++sample) {
        sai_words[sample] = (int32_t)((uint32_t)samples[sample] << 8);
    }
}

bool gp100_audio_fold_stereo_difference(
    int32_t *interleaved_stereo,
    size_t frame_count)
{
    size_t frame;
    if (frame_count != 0u && interleaved_stereo == NULL) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        int32_t left = interleaved_stereo[frame * 2u];
        int32_t right = interleaved_stereo[frame * 2u + 1u];
        int32_t difference = right / 2 - left / 2;
        interleaved_stereo[frame * 2u] = difference;
        interleaved_stereo[frame * 2u + 1u] = difference;
    }
    return true;
}

void gp100_audio_monitor_ring_reset(
    gp100_audio_monitor_ring_t *ring)
{
    if (ring != NULL) {
        memset(ring, 0, sizeof(*ring));
    }
}

bool gp100_audio_monitor_ring_write(
    gp100_audio_monitor_ring_t *ring,
    const int32_t *interleaved_stereo,
    size_t frame_count)
{
    size_t sample;
    if (ring == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        ring->write_index >= GP100_AUDIO_MONITOR_SAMPLES ||
        ring->read_index >= GP100_AUDIO_MONITOR_SAMPLES) {
        return false;
    }
    for (sample = 0u; sample < frame_count * 2u; ++sample) {
        ring->samples[ring->write_index] = interleaved_stereo[sample];
        ++ring->write_index;
        if (ring->write_index == GP100_AUDIO_MONITOR_SAMPLES) {
            ring->write_index = 0u;
        }
    }
    return true;
}

gp100_audio_monitor_read_result_t
gp100_audio_monitor_ring_read(
    gp100_audio_monitor_ring_t *ring,
    int32_t *interleaved_stereo,
    size_t frame_count)
{
    size_t available;
    size_t required;
    size_t sample;

    if (ring == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        ring->write_index >= GP100_AUDIO_MONITOR_SAMPLES ||
        ring->read_index >= GP100_AUDIO_MONITOR_SAMPLES) {
        return GP100_AUDIO_MONITOR_READ_INVALID;
    }
    required = frame_count * 2u;
    if (ring->write_index < ring->read_index) {
        available = ring->write_index + GP100_AUDIO_MONITOR_SAMPLES -
                    ring->read_index;
    } else {
        available = ring->write_index - ring->read_index;
    }
    if (required > available) {
        for (sample = 0u; sample < required; ++sample) {
            interleaved_stereo[sample] = 0;
        }
        return GP100_AUDIO_MONITOR_READ_UNDERRUN;
    }
    for (sample = 0u; sample < required; ++sample) {
        interleaved_stereo[sample] = ring->samples[ring->read_index];
        ++ring->read_index;
        if (ring->read_index == GP100_AUDIO_MONITOR_SAMPLES) {
            ring->read_index = 0u;
        }
    }
    return GP100_AUDIO_MONITOR_READ_DATA;
}
