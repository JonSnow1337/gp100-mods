#include "audio/effects/tone/global_equalizer.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FACTORY_PARAMETER_TABLE_OFFSET 0x226474L
#define FACTORY_PARAMETER_RECORD_BYTES 28u
#define FACTORY_PARAMETER_COUNT 31u

static uint32_t float_as_word(float value)
{
    uint32_t word;

    memcpy(&word, &value, sizeof(word));
    return word;
}

static uint32_t read_u32_le(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static int test_factory_parameter_defaults(const char *artifact_path)
{
    static const float expected[FACTORY_PARAMETER_COUNT] = {
        1.0f, 19.0f, 0.71f,
        0.0f, 20.0f, 0.71f, 0.0f,
        1.0f, 100.0f, 0.71f, 0.0f,
        1.0f, 500.0f, 0.71f, 0.0f,
        1.0f, 1000.0f, 0.71f, 0.0f,
        1.0f, 5000.0f, 0.71f, 0.0f,
        0.0f, 20000.0f, 0.71f, 0.0f,
        1.0f, 20001.0f, 0.71f, 50.0f
    };
    uint8_t record[FACTORY_PARAMETER_RECORD_BYTES];
    FILE *artifact;
    size_t index;

    artifact = fopen(artifact_path, "rb");
    if (artifact == NULL ||
        fseek(artifact, FACTORY_PARAMETER_TABLE_OFFSET, SEEK_SET) != 0) {
        if (artifact != NULL) {
            fclose(artifact);
        }
        return 1;
    }
    for (index = 0u; index < FACTORY_PARAMETER_COUNT; ++index) {
        if (fread(record, 1u, sizeof(record), artifact) != sizeof(record) ||
            read_u32_le(&record[20]) != float_as_word(expected[index])) {
            fclose(artifact);
            return 2;
        }
    }
    fclose(artifact);
    return 0;
}

static gp100_sysex_global_state_t default_state(void)
{
    gp100_sysex_global_state_t state;
    static const float values[GP100_SYSEX_GLOBAL_PARAMETER_COUNT] = {
        0.0f, 19.0f, 20001.0f,
        100.0f, 0.71f, 0.0f,
        500.0f, 0.71f, 0.0f,
        1000.0f, 0.71f, 0.0f,
        5000.0f, 0.71f, 0.0f
    };
    size_t index;

    for (index = 0u; index < GP100_SYSEX_GLOBAL_PARAMETER_COUNT;
         ++index) {
        state.parameters[index] = float_as_word(values[index]);
    }
    return state;
}

static int test_default_bypass(void)
{
    gp100_global_equalizer_t equalizer;
    gp100_sysex_global_state_t state = default_state();
    float audio[4] = {0.25f, -0.5f, 0.75f, -1.0f};

    gp100_global_equalizer_reset(&equalizer);
    if (!gp100_global_equalizer_configure(&equalizer, &state) ||
        equalizer.enabled || !equalizer.filter_active[0] ||
        !equalizer.filter_active[1] || !equalizer.filter_active[2] ||
        !equalizer.filter_active[3] || !equalizer.filter_active[4] ||
        !equalizer.filter_active[5] ||
        !gp100_global_equalizer_process(&equalizer, audio, 2u) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        audio[2] != 0.75f || audio[3] != -1.0f) {
        return 1;
    }
    return 0;
}

static int test_active_stereo_filters(void)
{
    gp100_global_equalizer_t equalizer;
    gp100_sysex_global_state_t state = default_state();
    float impulse[16] = {1.0f, 1.0f};
    size_t sample;

    state.parameters[GP100_GLOBAL_EQ_ENABLE] = float_as_word(1.0f);
    state.parameters[GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY] =
        float_as_word(80.0f);
    state.parameters[GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY] =
        float_as_word(8000.0f);
    state.parameters[GP100_GLOBAL_EQ_BAND_2_GAIN] =
        float_as_word(6.0f);
    gp100_global_equalizer_reset(&equalizer);
    if (!gp100_global_equalizer_configure(&equalizer, &state) ||
        !equalizer.enabled || !equalizer.filter_active[0] ||
        !equalizer.filter_active[2] || !equalizer.filter_active[5] ||
        !gp100_global_equalizer_process(&equalizer, impulse, 8u)) {
        return 1;
    }
    for (sample = 0u; sample < 16u; sample += 2u) {
        if (!isfinite(impulse[sample]) ||
            impulse[sample] != impulse[sample + 1u]) {
            return 2;
        }
    }
    if (impulse[0] == 0.0f || fabsf(impulse[0]) >= 0.0002f ||
        fabsf(equalizer.current_output_gain - 0.0008f) > 0.0000001f) {
        return 3;
    }
    const float previous_gain = equalizer.current_output_gain;
    state.parameters[GP100_GLOBAL_EQ_ENABLE] = float_as_word(0.0f);
    if (!gp100_global_equalizer_configure(&equalizer, &state) ||
        !gp100_global_equalizer_process(&equalizer, impulse, 8u) ||
        equalizer.current_output_gain != previous_gain) {
        return 4;
    }
    state.parameters[GP100_GLOBAL_EQ_ENABLE] = float_as_word(1.0f);
    if (!gp100_global_equalizer_configure(&equalizer, &state) ||
        !gp100_global_equalizer_process(&equalizer, impulse, 8u) ||
        fabsf(equalizer.current_output_gain - 0.0016f) > 0.0000001f) {
        return 5;
    }
    return 0;
}

static int test_factory_cut_q_and_sentinels(void)
{
    gp100_global_equalizer_t equalizer;
    gp100_sysex_global_state_t state = default_state();
    const float low_angle = 19.0f /
        GP100_GLOBAL_EQUALIZER_SAMPLE_RATE *
        6.2831853071795864769f;
    const float high_angle = 20001.0f /
        GP100_GLOBAL_EQUALIZER_SAMPLE_RATE *
        6.2831853071795864769f;
    const float expected_low_a0 =
        1.0f + sinf(low_angle) / (2.0f * 0.71f);
    const float expected_high_a0 =
        1.0f + sinf(high_angle) / (2.0f * 0.71f);

    state.parameters[GP100_GLOBAL_EQ_ENABLE] = float_as_word(1.0f);
    gp100_global_equalizer_reset(&equalizer);
    if (!gp100_global_equalizer_configure(&equalizer, &state) ||
        fabsf(equalizer.coefficients[0].a0 - expected_low_a0) > 0.0000001f ||
        fabsf(equalizer.coefficients[5].a0 - expected_high_a0) > 0.0000001f) {
        return 1;
    }
    return 0;
}

static int test_invalid_state_is_transactional(void)
{
    gp100_global_equalizer_t equalizer;
    gp100_sysex_global_state_t state = default_state();
    gp100_global_equalizer_t before;

    gp100_global_equalizer_reset(&equalizer);
    if (!gp100_global_equalizer_configure(&equalizer, &state)) {
        return 1;
    }
    before = equalizer;
    state.parameters[GP100_GLOBAL_EQ_BAND_1_Q] =
        float_as_word(0.0f);
    if (gp100_global_equalizer_configure(&equalizer, &state) ||
        memcmp(&equalizer, &before, sizeof(equalizer)) != 0 ||
        gp100_global_equalizer_process(NULL, NULL, 0u) ||
        gp100_global_equalizer_process(&equalizer, NULL, 1u)) {
        return 2;
    }
    state = default_state();
    state.parameters[GP100_GLOBAL_EQ_ENABLE] =
        UINT32_C(0x7fc00000);
    if (gp100_global_equalizer_configure(&equalizer, &state)) {
        return 3;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s app_v210_sdram.bin\n", argv[0]);
        return 1;
    }
    result = test_factory_parameter_defaults(argv[1]);
    if (result != 0) {
        fprintf(stderr, "factory parameter table test failed: %d\n", result);
        return result;
    }
    result = test_default_bypass();

    if (result != 0) {
        fprintf(stderr, "default bypass test failed: %d\n", result);
        return 10 + result;
    }
    result = test_active_stereo_filters();
    if (result != 0) {
        fprintf(stderr, "active filter test failed: %d\n", result);
        return 20 + result;
    }
    result = test_invalid_state_is_transactional();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 30 + result;
    }
    result = test_factory_cut_q_and_sentinels();
    if (result != 0) {
        fprintf(stderr, "factory cut test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
