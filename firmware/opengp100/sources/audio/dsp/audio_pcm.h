#ifndef GP100_AUDIO_PCM_H
#define GP100_AUDIO_PCM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_PCM24_MIN INT32_C(-8388608)
#define GP100_PCM24_MAX INT32_C(8388607)
/* Factory guitar scaling and the effect chain's amplitude at 0 dBFS. */
#define GP100_EFFECT_DSP_FULL_SCALE 0x1.1a1982p+0f

/* Convert right-aligned signed 24-bit transport samples to normalized DSP. */
bool gp100_audio_pcm24_to_float(const int32_t *input,
                                     float *output,
                                     size_t sample_count);

/* Convert normalized DSP samples back to signed 24-bit with saturation. */
bool gp100_audio_float_to_pcm24(const float *input,
                                     int32_t *output,
                                     size_t sample_count,
                                     size_t *clipped_samples);

#ifdef __cplusplus
}
#endif

#endif
