#include "audio/effects/pitch/octaver2.h"

#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <string.h>

#define TAU 6.283185307179586f

static void check_note(unsigned period, unsigned range)
{
    for (unsigned voice = 1u; voice <= 2u; ++voice) {
        const float knobs[4] = {
            0.0f, voice == 1u ? 99.0f : 0.0f, voice == 2u ? 99.0f : 0.0f,
            (float)range
        };
        gp100_octaver2_t effect;
        gp100_octaver2_reset(&effect, knobs);
        float in_phase = 0.0f;
        float quadrature = 0.0f;
        unsigned edges = 0u;
        const unsigned sample_count = 16u * period;
        for (unsigned n = 0u; n < 24u * period; ++n) {
            const float phase = TAU * (float)n / (float)period;
            /* A strong second harmonic exercises the bass tracking filter. */
            const float input = 0.12f * (sinf(phase) + 0.8f * sinf(2.0f * phase + 0.4f));
            float frame[2] = {input, input};
            const uint8_t divider = effect.divider;
            gp100_octaver2_process(&effect, knobs, frame, 1u, true);
            assert(isfinite(frame[0]) && frame[0] == frame[1]);
            if (n >= 8u * period) {
                const float sub_phase = phase / (float)(1u << voice);
                in_phase += frame[0] * cosf(sub_phase);
                quadrature += frame[0] * sinf(sub_phase);
                edges += divider != effect.divider;
            }
        }
        const float sub_amplitude = 2.0f * hypotf(in_phase, quadrature) / (float)sample_count;
        assert(edges == 16u);
        assert(sub_amplitude > 0.005f);
    }
}

static void check_stereo_and_bypass(void)
{
    const float knobs[4] = {99.0f, 0.0f, 0.0f, 0.0f};
    gp100_octaver2_t effect;
    gp100_octaver2_reset(&effect, knobs);
    float frame[2] = {0.20f, -0.20f};
    gp100_octaver2_process(&effect, knobs, frame, 1u, true);
    assert(frame[0] == 0.20f && frame[1] == -0.20f);
    const float bypassed[] = {0.20f, -0.10f, 0.30f, 0.10f};
    float block[4];
    memcpy(block, bypassed, sizeof(block));
    gp100_octaver2_process(&effect, knobs, block, 2u, false);
    assert(memcmp(block, bypassed, sizeof(block)) == 0);
}

static void check_note_release(void)
{
    const float knobs[4] = {0.0f, 99.0f, 99.0f, 0.0f};
    gp100_octaver2_t effect;
    gp100_octaver2_reset(&effect, knobs);
    for (unsigned n = 0u; n < 23000u; ++n) {
        const float input = n < 8800u ? 0.24f * sinf(TAU * (float)n / 1070.0f) : 0.0f;
        float frame[2] = {input, input};
        gp100_octaver2_process(&effect, knobs, frame, 1u, true);
        if (n == 8700u) {
            assert(effect.gate > 0.99f);
        }
        if (n > 18000u) {
            assert(effect.gate < 0.001f);
            assert(fabsf(frame[0]) < 0.001f);
        }
    }
}

int main(void)
{
    check_note(1428u, 0u); /* Bass low B, 30.9 Hz. */
    check_note(1070u, 0u); /* Bass low E, 41.2 Hz. */
    check_note(400u, 1u);  /* A2, 110.3 Hz. */
    check_note(100u, 2u);  /* A4, 441 Hz. */
    check_stereo_and_bypass();
    check_note_release();
    return 0;
}
