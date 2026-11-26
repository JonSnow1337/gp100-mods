#include "audio/effects/dynamics/studio_comp.h"

#include "audio/dsp/audio_pcm.h"
#include "audio/effects/effect_catalog.h"
#include "ui/screens/effect_picker.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>

static float amplitude(float db)
{
    return GP100_EFFECT_DSP_FULL_SCALE * powf(10.0f, db / 20.0f);
}

static void check_db(float sample, float db)
{
    const float expected = amplitude(db);
    assert(fabsf(sample / expected - 1.0f) < 0.0002f);
}

static float apply_level(gp100_studio_comp_t *comp, const float controls[], float db)
{
    const float input = amplitude(db);
    float samples[] = {input, -0.5f * input};
    gp100_studio_comp_process(comp, controls, samples, 1u, true);
    assert(samples[1] == -0.5f * samples[0]);
    return samples[0];
}

static void test_transfer(void)
{
    float controls[] = {0.0f, GP100_STUDIO_COMP_MUTE_DB, -20.0f, 0.0f, 0.0f, 4.0f, 0.0f};
    gp100_studio_comp_t comp;
    gp100_studio_comp_reset(&comp, controls);
    const float impulse = amplitude(-8.0f);
    float samples[] = {0.0f, 0.0f, impulse, -0.5f * impulse, 0.0f, 0.0f};
    gp100_studio_comp_process(&comp, controls, samples, 3u, true);
    assert(samples[0] == 0.0f && samples[1] == 0.0f);
    check_db(samples[2], -17.0f);
    assert(samples[3] == -0.5f * samples[2]);
    assert(samples[4] == 0.0f && samples[5] == 0.0f);
    check_db(apply_level(&comp, controls, -24.0f), -24.0f);

    controls[GP100_STUDIO_COMP_KNEE] = 12.0f;
    check_db(apply_level(&comp, controls, -20.0f), -20.0f);
    check_db(apply_level(&comp, controls, -14.0f), -15.125f);
    check_db(apply_level(&comp, controls, -8.0f), -12.5f);
    check_db(apply_level(&comp, controls, 0.0f), -10.5f);

    controls[GP100_STUDIO_COMP_KNEE] = 0.0f;
    controls[GP100_STUDIO_COMP_RATIO] = GP100_STUDIO_COMP_MAX_RATIO;
    check_db(apply_level(&comp, controls, -8.0f), -19.7f);
    controls[GP100_STUDIO_COMP_RATIO] = GP100_STUDIO_COMP_INFINITE_RATIO;
    check_db(apply_level(&comp, controls, -8.0f), -20.0f);
    controls[GP100_STUDIO_COMP_RATIO] = 1.0f;
    check_db(apply_level(&comp, controls, -8.0f), -8.0f);
}

static void test_timing(void)
{
    const float controls[] = {0.0f, GP100_STUDIO_COMP_MUTE_DB, -20.0f, 3.0f, 100.0f, 4.0f, 0.0f};
    gp100_studio_comp_t comp;
    gp100_studio_comp_reset(&comp, controls);
    float output = apply_level(&comp, controls, -8.0f);
    check_db(output, -8.0f - 9.0f * (1.0f - expf(-1.0f / 132.3f)));
    for (unsigned i = 1; i < 132u; ++i) {
        output = apply_level(&comp, controls, -8.0f);
    }
    const float attenuation = 9.0f * (1.0f - expf(-132.0f / 132.3f));
    check_db(output, -8.0f - attenuation);
    for (unsigned i = 0; i < 4410u; ++i) {
        output = apply_level(&comp, controls, -30.0f);
    }
    check_db(output, -30.0f - attenuation * expf(-1.0f));
}

static void test_mix_and_bypass(void)
{
    float controls[] = {12.0f, 0.0f, -20.0f, 0.0f, 0.0f, 4.0f, 0.0f};
    gp100_studio_comp_t comp;
    gp100_studio_comp_reset(&comp, controls);
    const float parallel = apply_level(&comp, controls, -8.0f);
    assert(fabsf(parallel - amplitude(-8.0f) - amplitude(-5.0f)) < 0.0001f);

    controls[GP100_STUDIO_COMP_WET] = GP100_STUDIO_COMP_MUTE_DB;
    controls[GP100_STUDIO_COMP_DRY] = 12.0f;
    check_db(apply_level(&comp, controls, -8.0f), 4.0f);

    const float original[] = {0.125f, -0.25f, 0.5f, -0.125f};
    float samples[4];
    memcpy(samples, original, sizeof(samples));
    gp100_studio_comp_process(&comp, controls, samples, 2u, false);
    assert(memcmp(samples, original, sizeof(samples)) == 0);
}

static void test_catalog_entry(void)
{
    const gp100_effect_family_t family = GP100_EFFECT_PRE_FX;
    const uint16_t model = GP100_PRE_FX_STUDIO_COMP;
    static const char *const names[] = {"Wet", "Dry", "Threshold", "Attack", "Release", "Ratio", "Knee"};
    static const char *const values[] = {"0dB", "-inf", "0dB", "3ms", "100ms", "4:1", "0dB"};
    gp100_effect_group_t group;
    uint16_t index;
    assert(gp100_effect_picker_find((gp100_picker_effect_t){family, model}, &group, &index));
    assert(group == GP100_EFFECT_GROUP_DYNAMICS);
    assert(strcmp(gp100_effect_algorithm_name(family, model), "Studio Comp") == 0);
    assert(gp100_effect_parameter_count(family, model) == GP100_STUDIO_COMP_CONTROL_COUNT);
    for (size_t i = 0; i < GP100_STUDIO_COMP_CONTROL_COUNT; ++i) {
        char text[16];
        gp100_parameter_rule_t rule;
        assert(strcmp(gp100_effect_parameter_name(family, model, i), names[i]) == 0);
        assert(gp100_effect_parameter_rule(family, model, i, NULL, &rule));
        assert(gp100_effect_format_parameter_value(family, model, i, rule.default_value,
                                                    NULL, text, sizeof(text)) > 0);
        assert(strcmp(text, values[i]) == 0);
    }
    char ratio[10];
    assert(gp100_effect_format_parameter_value(family, model, GP100_STUDIO_COMP_RATIO,
        GP100_STUDIO_COMP_INFINITE_RATIO, NULL, ratio, sizeof(ratio)) == 5);
    assert(strcmp(ratio, "Inf:1") == 0);
}

int main(void)
{
    test_transfer();
    test_timing();
    test_mix_and_bypass();
    test_catalog_entry();
    return 0;
}
