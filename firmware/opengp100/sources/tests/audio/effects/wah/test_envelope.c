#include "audio/effects/wah/envelope.h"

#include "audio/effects/effect_catalog.h"
#include "ui/screens/effect_picker.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>

static float measure_gain(gp100_envelope_t *effect, const float controls[5],
                          float frequency, float trigger)
{
    const float phase_step = (2.0f * 3.14159265f / 44100.0f) * frequency;
    const float amplitude = 0.001f;
    float energy_in = 0.0f;
    float energy_out = 0.0f;
    for (size_t block = 0; block < 512u; ++block) {
        float audio[128];
        for (size_t frame = 0; frame < 64u; ++frame) {
            const float input = amplitude * sinf(phase_step * (float)(64u * block + frame));
            audio[2u * frame] = input;
            /* Use the other channel to hold the linked detector at a
             * known level independently of the frequency probe. */
            audio[2u * frame + 1u] = trigger;
            if (block >= 400u) {
                energy_in += input * input;
            }
        }
        gp100_envelope_process(effect, controls, audio, 64u, true);
        if (block >= 400u) {
            for (size_t frame = 0; frame < 64u; ++frame) {
                assert(isfinite(audio[2u * frame]));
                energy_out += audio[2u * frame] * audio[2u * frame];
            }
        }
    }
    return sqrtf(energy_out / energy_in);
}

static void test_filter_sweep(void)
{
    float controls[5] = {0.0f, 99.0f, 0.0f, 99.0f, 99.0f};
    gp100_envelope_t effect;
    gp100_envelope_reset(&effect, controls);
    const float floor = measure_gain(&effect, controls, 76.0f, 0.0f);
    assert(fabsf(floor - 7.94f) < 0.15f);

    const float peak = measure_gain(&effect, controls, 3200.0f, 0.14f);
    assert(fabsf(peak - 19.95f) < 0.15f);
    const float released = measure_gain(&effect, controls, 3200.0f, 0.0f);
    assert(released < 0.1f);

    controls[2] = 99.0f;
    controls[3] = 0.0f;
    const float raised_floor = measure_gain(&effect, controls, 1300.0f, 0.0f);
    assert(fabsf(raised_floor - 2.51f) < 0.05f);

    controls[4] = 0.0f;
    const float low_sensitivity = measure_gain(&effect, controls, 1300.0f, 0.14f);
    assert(fabsf(low_sensitivity - raised_floor) < 0.05f);
}

static void test_mix_and_bypass(void)
{
    float controls[5] = {99.0f, 0.0f, 50.0f, 50.0f, 50.0f};
    const float stereo[4] = {0.125f, -0.25f, -0.375f, 0.0625f};
    float audio[4];
    memcpy(audio, stereo, sizeof(audio));
    gp100_envelope_t effect;
    gp100_envelope_reset(&effect, controls);
    gp100_envelope_process(&effect, controls, audio, 2u, true);
    for (size_t i = 0; i < 4u; ++i) {
        assert(fabsf(audio[i] - 1.995262f * stereo[i]) < 0.00001f);
    }

    controls[0] = 0.0f;
    gp100_envelope_process(&effect, controls, audio, 2u, true);
    assert(effect.settings.dry > 1.9f);
    memcpy(audio, stereo, sizeof(audio));
    gp100_envelope_process(&effect, controls, audio, 2u, false);
    assert(memcmp(audio, stereo, sizeof(audio)) == 0);
    assert(effect.detector == 0.0f && !effect.enabled);
}

static void test_catalog_entry(void)
{
    const gp100_effect_family_t family = GP100_EFFECT_PRE_FX;
    const uint16_t model = GP100_PRE_FX_ENVELOPE;
    static const char *const names[] = {"Dry", "FX", "Decay", "Q", "Sens"};
    gp100_effect_group_t group;
    uint16_t index;
    assert(gp100_effect_picker_find((gp100_picker_effect_t){family, model}, &group, &index));
    assert(group == GP100_EFFECT_GROUP_WAH);
    assert(strcmp(gp100_effect_algorithm_name(family, model), "Envelope") == 0);
    assert(gp100_effect_parameter_count(family, model) == 5u);
    for (size_t i = 0u; i < 5u; ++i) {
        assert(strcmp(gp100_effect_parameter_name(family, model, i), names[i]) == 0);
    }
}

int main(void)
{
    test_filter_sweep();
    test_mix_and_bypass();
    test_catalog_entry();
    return 0;
}
