#include "audio/effects/dynamics/output_limiter.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>

static void test_makeup_and_latency(int16_t threshold, int16_t ceiling,
                                    float expected_makeup)
{
    gp100_output_limiter_t state = {0};
    gp100_output_limiter_config_t config = {0};
    float samples[128u * 2u];
    for (size_t i = 0u; i < 128u; ++i) {
        samples[2u * i] = 0.125f;
        samples[2u * i + 1u] = -0.0625f;
    }
    gp100_output_limiter_process(&state, &config, samples, 128u);
    for (size_t i = 0u; i < 128u; ++i) {
        assert(samples[2u * i] == 0.125f && samples[2u * i + 1u] == -0.0625f);
    }

    state = (gp100_output_limiter_t){0};
    gp100_output_limiter_configure(&config, true, threshold, ceiling, 15, 1.0f);
    gp100_output_limiter_process(&state, &config, samples, 64u);
    gp100_output_limiter_process(&state, &config, &samples[2u * 64], 64u);
    for (size_t i = 0u; i < GP100_LIMITER_LOOKAHEAD_FRAMES; ++i) {
        assert(samples[2u * i] == 0.0f && samples[2u * i + 1u] == 0.0f);
    }
    for (size_t i = GP100_LIMITER_LOOKAHEAD_FRAMES; i < 128u; ++i) {
        assert(fabsf(samples[2u * i] - 0.125f * expected_makeup) < 1e-6f);
        assert(fabsf(samples[2u * i + 1u] + 0.0625f * expected_makeup) < 1e-6f);
    }
}

static void test_stereo_lookahead(void)
{
    gp100_output_limiter_t state = {0};
    gp100_output_limiter_config_t config = {0};
    float samples[256u * 2u];
    const size_t peak_frame = 70u;
    for (size_t i = 0u; i < 256u; ++i) {
        samples[2u * i] = 0.125f;
        samples[2u * i + 1u] = 0.25f;
    }
    samples[2u * peak_frame] = -0.5f;
    samples[2u * peak_frame + 1u] = 2.0f;

    gp100_output_limiter_configure(&config, true, 0, -1, 15, 1.0f);
    gp100_output_limiter_process(&state, &config, samples, 0u);
    state.frame = UINT32_MAX - 90u;
    for (size_t i = 0u; i < 256u; i += 64u) {
        gp100_output_limiter_process(&state, &config, &samples[2u * i], 64u);
    }
    const size_t output_peak = peak_frame + GP100_LIMITER_LOOKAHEAD_FRAMES;
    assert(samples[2u * (output_peak - 32u) + 1u] < samples[2u * (output_peak - 33u) + 1u]);
    assert(samples[2u * (output_peak - 1u) + 1u] < samples[2u * (output_peak - 32u) + 1u]);
    assert(fabsf(samples[2u * output_peak + 1u] - config.ceiling) < 1e-6f);
    assert(fabsf(samples[2u * output_peak] + 0.25f * samples[2u * output_peak + 1u]) < 1e-6f);
    for (size_t i = 0u; i < 256u; ++i) {
        assert(fabsf(samples[2u * i]) <= config.ceiling);
        assert(fabsf(samples[2u * i + 1u]) <= config.ceiling);
    }
}

static void test_release(void)
{
    gp100_output_limiter_t state = {0};
    gp100_output_limiter_config_t config = {0};
    static float samples[19200u * 2u];
    for (size_t i = 0u; i < sizeof(samples) / sizeof(samples[0]) / 2u; ++i) {
        samples[2u * i] = i < 256u ? 2.0f : 0.25f;
        samples[2u * i + 1u] = -0.125f;
    }
    gp100_output_limiter_configure(&config, true, 0, 0, 15, 1.0f);
    for (size_t i = 0u; i < sizeof(samples) / sizeof(samples[0]) / 2u; i += 64u) {
        gp100_output_limiter_process(&state, &config, &samples[2u * i], 64u);
    }
    const float recovered_db = 20.0f * log10f(samples[2u * 5210] / samples[2u * 800]);
    assert(fabsf(recovered_db - 1.5f) < 0.01f);
    assert(fabsf(samples[2u * 19199] - 0.25f) < 1e-6f);
}

static void test_live_settings(void)
{
    gp100_output_limiter_t state = {0};
    gp100_output_limiter_config_t config = {0};
    float samples[256u * 2u];
    for (size_t i = 0u; i < 256u; ++i) {
        samples[2u * i] = -0.5f;
        samples[2u * i + 1u] = 0.25f;
    }
    gp100_output_limiter_configure(&config, true, 0, 0, 15, 1.0f);
    gp100_output_limiter_process(&state, &config, samples, 128u);
    gp100_output_limiter_configure(&config, true, -30, -6, 15, 1.0f);
    gp100_output_limiter_process(&state, &config, &samples[2u * 128], 128u);
    for (size_t i = 128u; i < 256u; ++i) {
        assert(fabsf(samples[2u * i]) <= config.ceiling);
        assert(fabsf(samples[2u * i] + 2.0f * samples[2u * i + 1u]) < 1e-6f);
    }
}

static void test_live_toggle(void)
{
    gp100_output_limiter_t state = {0};
    gp100_output_limiter_config_t config = {0};
    float samples[128u * 2u];
    for (unsigned stage = 0u; stage < 3u; ++stage) {
        const bool enabled = stage == 1u;
        gp100_output_limiter_configure(&config, enabled, -6, 0, 15, 1.0f);
        for (size_t i = 0u; i < 128u; ++i) {
            samples[2u * i] = 0.125f;
            samples[2u * i + 1u] = -0.0625f;
        }
        gp100_output_limiter_process(&state, &config, samples, 128u);
        for (size_t i = 0u; i < 128u; ++i) {
            const float left = samples[2u * i];
            assert(left >= 0.125f && left < 0.25f);
            assert(fabsf(left + 2.0f * samples[2u * i + 1u]) < 1e-6f);
            if (i > 0u) {
                assert(fabsf(left - samples[2u * (i - 1u)]) < 0.002f);
            }
        }
        const float expected = enabled ? 0.125f * config.makeup : 0.125f;
        assert(fabsf(samples[2u * 127u] - expected) < 1e-6f);
    }
}

int main(void)
{
    test_makeup_and_latency(0, 0, 1.0f);
    test_makeup_and_latency(-6, -1, powf(10.0f, 5.0f / 20.0f));
    test_makeup_and_latency(12, 0, powf(10.0f, -12.0f / 20.0f));
    test_stereo_lookahead();
    test_release();
    test_live_settings();
    test_live_toggle();
    return 0;
}
