#include "audio/effects/wah/balls.h"

#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <string.h>

#define TAU 6.283185307179586f

static float run_tone(gp100_balls_t *effect, float response, float frequency,
                     float amplitude, unsigned blocks)
{
    float power = 0.0f;
    for (unsigned block = 0u; block < blocks; ++block) {
        float audio[128];
        for (unsigned i = 0u; i < 64u; ++i) {
            const float sample = amplitude * sinf(TAU * frequency * (float)(64u * block + i) / 44100.0f);
            audio[2u * i] = audio[2u * i + 1u] = sample;
        }
        gp100_balls_process(effect, response, audio, 64u, true);
        if (block >= blocks / 2u) {
            for (unsigned i = 0u; i < 64u; ++i) {
                const float sample = audio[2u * i];
                assert(isfinite(sample) && fabsf(sample) <= 1.08f && sample == audio[2u * i + 1u]);
                power += sample * sample;
            }
        }
    }
    return sqrtf(power / (64.0f * (float)(blocks - blocks / 2u)));
}

static float quiet_tone_rms(float frequency)
{
    gp100_balls_t effect;
    gp100_balls_reset(&effect, 0.0f);
    const float rms = run_tone(&effect, 0.0f, frequency, 0.015f, 256u);
    assert(effect.envelope == 0.0f);
    return rms;
}

static void check_response(void)
{
    gp100_balls_t effect;
    gp100_balls_reset(&effect, 99.0f);
    const float soft = run_tone(&effect, 99.0f, 41.2f, 0.02f, 120u);
    assert(soft > 0.004f && effect.envelope < 1.0f);
    (void)run_tone(&effect, 99.0f, 41.2f, 0.12f, 120u);
    assert(effect.envelope > 2.0f);
    const float peak = effect.envelope;
    const float tail = run_tone(&effect, 99.0f, 0.0f, 0.0f, 700u);
    assert(tail < 0.002f && effect.envelope > 0.4f * peak && effect.envelope < 0.6f * peak);

    gp100_balls_reset(&effect, 20.0f);
    (void)run_tone(&effect, 20.0f, 41.2f, 0.12f, 120u);
    assert(effect.envelope < 0.8f);
    gp100_balls_reset(&effect, 99.0f);
    (void)run_tone(&effect, 99.0f, 41.2f, 1.0f, 120u);
    assert(effect.envelope > 5.0f && effect.envelope < 7.0f);
}

static void check_bypass(void)
{
    gp100_balls_t effect;
    gp100_balls_reset(&effect, 60.0f);
    float silence[128] = {0.0f};
    gp100_balls_process(&effect, 60.0f, silence, 64u, true);
    for (unsigned i = 0u; i < 128u; ++i) {
        assert(silence[i] == 0.0f);
    }
    const float dry[4] = {0.25f, -0.1f, 0.3f, -0.7f};
    float bypass[4];
    memcpy(bypass, dry, sizeof(dry));
    gp100_balls_process(&effect, 60.0f, bypass, 2u, false);
    assert(memcmp(bypass, dry, sizeof(dry)) == 0);
}

int main(void)
{
    const float between_peaks = quiet_tone_rms(100.0f);
    assert(quiet_tone_rms(47.0f) > 1.7f * between_peaks);
    assert(quiet_tone_rms(210.0f) > 1.5f * between_peaks);
    assert(quiet_tone_rms(210.0f) > 2.0f * quiet_tone_rms(450.0f));
    check_response();
    check_bypass();
    return 0;
}
