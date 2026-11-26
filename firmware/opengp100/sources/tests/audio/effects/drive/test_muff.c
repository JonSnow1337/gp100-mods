#include "audio/effects/drive/muff.h"

#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <string.h>

#define TAU 6.283185307179586f

typedef struct tone_measurement {
    float rms;
    float fundamental;
    float third_harmonic;
} tone_measurement_t;

static tone_measurement_t measure(float frequency, float amplitude, const float controls[3])
{
    gp100_muff_t effect;
    gp100_muff_reset(&effect, controls);
    float square_sum = 0.0f;
    float in_phase[2] = {0.0f, 0.0f};
    float quadrature[2] = {0.0f, 0.0f};
    for (unsigned n = 0u; n < 16384u; n += 64u) {
        float audio[128];
        for (unsigned i = 0u; i < 64u; ++i) {
            const float phase = TAU * frequency * (float)(n + i) / 44100.0f;
            audio[i * 2u] = audio[i * 2u + 1u] = amplitude * sinf(phase);
        }
        gp100_muff_process(&effect, controls, audio, 64u, true);
        for (unsigned i = 0u; i < 64u; ++i) {
            const float value = audio[i * 2u];
            assert(isfinite(value) && fabsf(value) < 1.2f && value == audio[i * 2u + 1u]);
            if (n >= 8192u) {
                const float phase = TAU * frequency * (float)(n + i) / 44100.0f;
                square_sum += value * value;
                in_phase[0] += value * sinf(phase);
                quadrature[0] += value * cosf(phase);
                in_phase[1] += value * sinf(3.0f * phase);
                quadrature[1] += value * cosf(3.0f * phase);
            }
        }
    }
    return (tone_measurement_t){
        .rms = sqrtf(square_sum / 8192.0f),
        .fundamental = hypotf(in_phase[0], quadrature[0]) / 4096.0f,
        .third_harmonic = hypotf(in_phase[1], quadrature[1]) / 4096.0f,
    };
}

int main(void)
{
    const float bass[3] = {60.0f, 0.0f, 60.0f};
    const float treble[3] = {60.0f, 99.0f, 60.0f};
    const float defaults[3] = {60.0f, 50.0f, 60.0f};
    const float low_sustain[3] = {0.0f, 50.0f, 60.0f};
    const float muted[3] = {99.0f, 50.0f, 0.0f};
    const float maximum[3] = {99.0f, 99.0f, 99.0f};
    const tone_measurement_t fuzz = measure(220.0f, 0.1f, defaults);
    assert(fuzz.fundamental > 0.03f && fuzz.third_harmonic > 0.15f * fuzz.fundamental);
    assert(measure(41.2f, 0.1f, bass).rms > 2.5f * measure(41.2f, 0.1f, treble).rms);
    assert(measure(4000.0f, 0.02f, treble).rms > 2.5f * measure(4000.0f, 0.02f, bass).rms);
    assert(measure(82.4f, 0.001f, defaults).rms > 3.0f * measure(82.4f, 0.001f, low_sustain).rms);
    assert(measure(41.2f, 1.0f, maximum).rms > 0.03f);
    assert(measure(220.0f, 0.1f, muted).rms == 0.0f);

    gp100_muff_t effect;
    gp100_muff_reset(&effect, defaults);
    float silence[128] = {0.0f};
    gp100_muff_process(&effect, defaults, silence, 64u, true);
    for (unsigned i = 0u; i < 128u; ++i) {
        assert(silence[i] == 0.0f);
    }
    const float dry[] = {0.3f, -0.2f, -0.3f, 0.5f};
    float bypass[4];
    memcpy(bypass, dry, sizeof(dry));
    gp100_muff_process(&effect, defaults, bypass, 2u, false);
    assert(memcmp(bypass, dry, sizeof(dry)) == 0);
    return 0;
}
