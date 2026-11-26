#include "audio/dsp/audio_biquad.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.000001f;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int test_factory_coefficients(void)
{
    const gp100_biquad_coefficients_t *pre =
        &GP100_PRE_CHAIN_BIQUAD_COEFFICIENTS;
    const gp100_biquad_coefficients_t *post =
        &GP100_POST_CHAIN_BIQUAD_COEFFICIENTS;
    const uint32_t expected_pre[6] = {
        UINT32_C(0x3eadf905), UINT32_C(0xbe0226a9),
        UINT32_C(0x3b416df4), UINT32_C(0x3f800000),
        UINT32_C(0xbf7be96c), UINT32_C(0x3e4ba98f)
    };
    const float pre_words[6] = {
        pre->b0, pre->b1, pre->b2, pre->a0, pre->a1, pre->a2
    };
    const float post_words[6] = {
        post->b0, post->b1, post->b2, post->a0, post->a1, post->a2
    };
    size_t index;

    for (index = 0u; index < 6u; ++index) {
        if (float_bits(pre_words[index]) != expected_pre[index] ||
            float_bits(post_words[index]) != expected_pre[(index + 3u) % 6u]) {
            return 1;
        }
    }
    return 0;
}

static int test_identity_and_reset(void)
{
    gp100_stereo_biquad_t filter;
    const gp100_biquad_coefficients_t coefficients = {
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f
    };
    float samples[4] = {0.25f, -0.5f, 1.0f, -1.0f};

    gp100_stereo_biquad_reset(&filter);
    if (!gp100_stereo_biquad_process(
            &filter, &coefficients, samples, 2u) ||
        !close_enough(samples[0], 0.25f) ||
        !close_enough(samples[1], -0.5f) ||
        !close_enough(samples[2], 1.0f) ||
        !close_enough(samples[3], -1.0f)) {
        return 1;
    }
    gp100_stereo_biquad_reset(&filter);
    if (filter.channel[0].x1 != 0.0f || filter.channel[0].y1 != 0.0f ||
        filter.channel[1].x2 != 0.0f || filter.channel[1].y2 != 0.0f) {
        return 2;
    }
    return 0;
}

static int test_stereo_histories_are_independent(void)
{
    gp100_stereo_biquad_t filter;
    const gp100_biquad_coefficients_t coefficients = {
        1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f
    };
    float samples[6] = {1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};
    const float expected[6] = {1.0f, 0.0f, 0.5f, 2.0f, 0.0f, 1.0f};
    size_t sample;

    gp100_stereo_biquad_reset(&filter);
    if (!gp100_stereo_biquad_process(
            &filter, &coefficients, samples, 3u)) {
        return 1;
    }
    for (sample = 0u; sample < 6u; ++sample) {
        if (!close_enough(samples[sample], expected[sample])) {
            return 2;
        }
    }
    return 0;
}

static int test_feedback_and_validation(void)
{
    gp100_stereo_biquad_t filter;
    const gp100_biquad_coefficients_t feedback = {
        1.0f, 0.0f, 0.0f, 1.0f, -0.5f, 0.0f
    };
    const gp100_biquad_coefficients_t invalid = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    float samples[6] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    gp100_stereo_biquad_reset(&filter);
    if (!gp100_stereo_biquad_process(
            &filter, &feedback, samples, 3u) ||
        !close_enough(samples[0], 1.0f) ||
        !close_enough(samples[2], 0.5f) ||
        !close_enough(samples[4], 0.25f) ||
        gp100_stereo_biquad_process(
            &filter, &invalid, samples, 3u) ||
        gp100_stereo_biquad_process(
            NULL, &feedback, samples, 3u) ||
        gp100_stereo_biquad_process(
            &filter, NULL, samples, 3u) ||
        gp100_stereo_biquad_process(
            &filter, &feedback, NULL, 3u) ||
        gp100_stereo_biquad_process(
            &filter, &feedback, samples, SIZE_MAX) ||
        !gp100_stereo_biquad_process(
            &filter, &feedback, NULL, 0u)) {
        return 1;
    }
    return 0;
}

static int test_factory_boundary_pair(void)
{
    gp100_audio_boundaries_t boundaries;
    const float input[12] = {
        0.75f, -0.5f, 0.25f, 0.125f, -0.375f, 0.625f,
        0.0f, 0.0f, 0.125f, -0.25f, -0.0625f, 0.5f
    };
    float samples[12];
    size_t sample;

    memcpy(samples, input, sizeof(samples));
    gp100_audio_boundaries_reset(&boundaries);
    if (!gp100_audio_boundaries_process_pre(
            &boundaries, samples, 6u) ||
        boundaries.pre_chain.channel[0].x1 == 0.0f ||
        boundaries.post_chain.channel[0].x1 != 0.0f ||
        !gp100_audio_boundaries_process_post(
            &boundaries, samples, 6u)) {
        return 1;
    }
    for (sample = 0u; sample < 12u; ++sample) {
        if (fabsf(samples[sample] - input[sample]) > 0.00001f) {
            return 2;
        }
    }
    gp100_audio_boundaries_reset(&boundaries);
    if (boundaries.pre_chain.channel[0].x1 != 0.0f ||
        boundaries.pre_chain.channel[1].y2 != 0.0f ||
        boundaries.post_chain.channel[0].x2 != 0.0f ||
        boundaries.post_chain.channel[1].y1 != 0.0f ||
        gp100_audio_boundaries_process_pre(
            NULL, samples, 6u) ||
        gp100_audio_boundaries_process_post(
            NULL, samples, 6u) ||
        !gp100_audio_boundaries_process_pre(
            &boundaries, NULL, 0u) ||
        !gp100_audio_boundaries_process_post(
            &boundaries, NULL, 0u)) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result = test_factory_coefficients();
    if (result != 0) {
        fprintf(stderr, "factory-coefficient test failed: %d\n", result);
        return result;
    }
    result = test_identity_and_reset();
    if (result != 0) {
        fprintf(stderr, "identity/reset test failed: %d\n", result);
        return 10 + result;
    }
    result = test_stereo_histories_are_independent();
    if (result != 0) {
        fprintf(stderr, "stereo-history test failed: %d\n", result);
        return 20 + result;
    }
    result = test_feedback_and_validation();
    if (result != 0) {
        fprintf(stderr, "feedback/validation test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_boundary_pair();
    if (result != 0) {
        fprintf(stderr, "factory-boundary-pair test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
