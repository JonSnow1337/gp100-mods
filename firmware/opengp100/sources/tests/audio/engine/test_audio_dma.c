#include "audio/engine/audio_dma.h"

#include <stdio.h>

static int test_half_selection(void)
{
    return gp100_audio_dma_uses_second_half(128u, 128u) &&
                   gp100_audio_dma_uses_second_half(128u, 65u) &&
                   !gp100_audio_dma_uses_second_half(128u, 64u) &&
                   gp100_audio_dma_uses_second_half(0x8080u, 0x8080u)
               ? 0
               : 1;
}

static int test_sample_conversion(void)
{
    int32_t words[4] = {
        (int32_t)UINT32_C(0x7fffff00),
        (int32_t)UINT32_C(0x80000000),
        (int32_t)UINT32_C(0xffffff00),
        0
    };
    int32_t samples[4];
    int32_t packed[4];

    gp100_audio_unpack_sai24(samples, words, 4u);
    if (samples[0] != INT32_C(0x007fffff) ||
        samples[1] != -INT32_C(0x00800000) || samples[2] != -1 ||
        samples[3] != 0) {
        return 1;
    }
    gp100_audio_pack_sai24(packed, samples, 4u);
    return packed[0] == words[0] && packed[1] == words[1] &&
                   packed[2] == words[2] && packed[3] == words[3]
               ? 0
               : 2;
}

static int test_difference_and_ring(void)
{
    gp100_audio_monitor_ring_t ring;
    int32_t samples[4] = {-3, 5, 8, -2};
    int32_t output[4] = {9, 9, 9, 9};

    if (!gp100_audio_fold_stereo_difference(samples, 2u) ||
        samples[0] != 3 || samples[1] != 3 ||
        samples[2] != -5 || samples[3] != -5) {
        return 1;
    }
    gp100_audio_monitor_ring_reset(&ring);
    ring.write_index = GP100_AUDIO_MONITOR_SAMPLES - 2u;
    if (!gp100_audio_monitor_ring_write(&ring, samples, 2u) ||
        ring.samples[GP100_AUDIO_MONITOR_SAMPLES - 2u] != 3 ||
        ring.samples[GP100_AUDIO_MONITOR_SAMPLES - 1u] != 3 ||
        ring.samples[0] != -5 || ring.samples[1] != -5 ||
        ring.write_index != 2u) {
        return 2;
    }

    gp100_audio_monitor_ring_reset(&ring);
    if (gp100_audio_monitor_ring_read(&ring, output, 2u) !=
            GP100_AUDIO_MONITOR_READ_UNDERRUN ||
        output[0] != 0 || output[1] != 0 || output[2] != 0 ||
        output[3] != 0 || ring.read_index != 0u ||
        !gp100_audio_monitor_ring_write(&ring, samples, 2u) ||
        gp100_audio_monitor_ring_read(&ring, output, 2u) !=
            GP100_AUDIO_MONITOR_READ_DATA ||
        output[0] != 3 || output[1] != 3 ||
        output[2] != -5 || output[3] != -5 ||
        ring.read_index != 4u) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_half_selection();
    if (result != 0) {
        fprintf(stderr, "half selection test failed: %d\n", result);
        return 10 + result;
    }
    result = test_sample_conversion();
    if (result != 0) {
        fprintf(stderr, "sample conversion test failed: %d\n", result);
        return 20 + result;
    }
    result = test_difference_and_ring();
    if (result != 0) {
        fprintf(stderr, "difference/ring test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
