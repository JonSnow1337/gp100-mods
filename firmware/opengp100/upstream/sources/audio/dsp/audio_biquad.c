#include "audio_biquad.h"

#include <stdint.h>
#include <string.h>

const gp100_biquad_coefficients_t
    GP100_PRE_CHAIN_BIQUAD_COEFFICIENTS = {
        0x1.5bf20ap-2f,
        -0x1.044d52p-3f,
        0x1.82dbe8p-9f,
        0x1.000000p+0f,
        -0x1.f7d2d8p-1f,
        0x1.97531ep-3f
    };

const gp100_biquad_coefficients_t
    GP100_POST_CHAIN_BIQUAD_COEFFICIENTS = {
        0x1.000000p+0f,
        -0x1.f7d2d8p-1f,
        0x1.97531ep-3f,
        0x1.5bf20ap-2f,
        -0x1.044d52p-3f,
        0x1.82dbe8p-9f
    };

void gp100_stereo_biquad_reset(gp100_stereo_biquad_t *filter)
{
    if (filter != NULL) {
        memset(filter, 0, sizeof(*filter));
    }
}

static float process_channel(
    gp100_biquad_channel_state_t *state,
    const gp100_biquad_coefficients_t *coefficients,
    float input)
{
    float output =
        (input * coefficients->b0 +
         state->x1 * coefficients->b1 +
         state->x2 * coefficients->b2 -
         state->y1 * coefficients->a1 -
         state->y2 * coefficients->a2) /
        coefficients->a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

bool gp100_stereo_biquad_process(
    gp100_stereo_biquad_t *filter,
    const gp100_biquad_coefficients_t *coefficients,
    float *interleaved_stereo,
    size_t frame_count)
{
    size_t frame;

    if (filter == NULL || coefficients == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        coefficients->a0 == 0.0f) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;

        interleaved_stereo[sample] = process_channel(
            &filter->channel[0], coefficients,
            interleaved_stereo[sample]);
        interleaved_stereo[sample + 1u] = process_channel(
            &filter->channel[1], coefficients,
            interleaved_stereo[sample + 1u]);
    }
    return true;
}

void gp100_audio_boundaries_reset(
    gp100_audio_boundaries_t *boundaries)
{
    if (boundaries != NULL) {
        gp100_stereo_biquad_reset(&boundaries->pre_chain);
        gp100_stereo_biquad_reset(&boundaries->post_chain);
    }
}

bool gp100_audio_boundaries_process_pre(
    gp100_audio_boundaries_t *boundaries,
    float *interleaved_stereo,
    size_t frame_count)
{
    return boundaries != NULL &&
        gp100_stereo_biquad_process(
            &boundaries->pre_chain,
            &GP100_PRE_CHAIN_BIQUAD_COEFFICIENTS,
            interleaved_stereo, frame_count);
}

bool gp100_audio_boundaries_process_post(
    gp100_audio_boundaries_t *boundaries,
    float *interleaved_stereo,
    size_t frame_count)
{
    return boundaries != NULL &&
        gp100_stereo_biquad_process(
            &boundaries->post_chain,
            &GP100_POST_CHAIN_BIQUAD_COEFFICIENTS,
            interleaved_stereo, frame_count);
}
