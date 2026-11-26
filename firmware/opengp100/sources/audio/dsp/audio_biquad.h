#ifndef GP100_AUDIO_BIQUAD_H
#define GP100_AUDIO_BIQUAD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gp100_biquad_coefficients {
    float b0;
    float b1;
    float b2;
    float a0;
    float a1;
    float a2;
} gp100_biquad_coefficients_t;

typedef struct gp100_biquad_channel_state {
    float x1;
    float x2;
    float y1;
    float y2;
} gp100_biquad_channel_state_t;

typedef struct gp100_stereo_biquad {
    gp100_biquad_channel_state_t channel[2];
} gp100_stereo_biquad_t;

/* The factory owns two independent history banks around the main DSP route. */
typedef struct gp100_audio_boundaries {
    gp100_stereo_biquad_t pre_chain;
    gp100_stereo_biquad_t post_chain;
} gp100_audio_boundaries_t;

/* Exact initialized coefficients used around the factory effects chain. */
extern const gp100_biquad_coefficients_t
    GP100_PRE_CHAIN_BIQUAD_COEFFICIENTS;
extern const gp100_biquad_coefficients_t
    GP100_POST_CHAIN_BIQUAD_COEFFICIENTS;

void gp100_stereo_biquad_reset(gp100_stereo_biquad_t *filter);

/* Processes interleaved left/right frames in place using shared coefficients. */
bool gp100_stereo_biquad_process(
    gp100_stereo_biquad_t *filter,
    const gp100_biquad_coefficients_t *coefficients,
    float *interleaved_stereo,
    size_t frame_count);

/* Reset and process the recovered complementary boundary pair. The two stages
 * are deliberately separate because the effect route lives between them. */
void gp100_audio_boundaries_reset(
    gp100_audio_boundaries_t *boundaries);

bool gp100_audio_boundaries_process_pre(
    gp100_audio_boundaries_t *boundaries,
    float *interleaved_stereo,
    size_t frame_count);

bool gp100_audio_boundaries_process_post(
    gp100_audio_boundaries_t *boundaries,
    float *interleaved_stereo,
    size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
