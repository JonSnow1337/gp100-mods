#include "audio/engine/audio_output.h"

#include <math.h>
#include <stdio.h>

static int test_ramp_and_scale(void)
{
    gp100_audio_output_level_t level;
    float samples[4] = {1.0f, -1.0f, 0.5f, -0.5f};

    gp100_audio_output_level_reset(&level);
    if (level.current_percent != 0.0f || level.target_percent != 100u ||
        !gp100_audio_output_level_set_target(&level, 50u) ||
        !gp100_audio_output_level_apply(&level, samples, 2u) ||
        level.current_percent != 1.0f ||
        fabsf(samples[0] - 0.01f) > 0.000001f ||
        fabsf(samples[3] + 0.005f) > 0.000001f) {
        return 1;
    }

    level.current_percent = 52.0f;
    samples[0] = 1.0f;
    samples[1] = 1.0f;
    if (!gp100_audio_output_level_apply(&level, samples, 1u) ||
        level.current_percent != 51.0f ||
        fabsf(samples[0] - 0.51f) > 0.000001f ||
        fabsf(samples[1] - 0.51f) > 0.000001f) {
        return 2;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_audio_output_level_t level;
    float samples[2] = {1.0f, 1.0f};

    gp100_audio_output_level_reset(&level);
    if (gp100_audio_output_level_set_target(&level, 101u) ||
        gp100_audio_output_level_apply(NULL, samples, 1u) ||
        gp100_audio_output_level_apply(&level, NULL, 1u) ||
        !gp100_audio_output_level_apply(&level, NULL, 0u)) {
        return 1;
    }
    return 0;
}

static int test_input_db_gain(void)
{
    gp100_audio_input_level_t level;
    float samples[4] = {1.0f, -0.5f, 1.0f, -0.5f};

    gp100_audio_input_level_reset(&level);
    if (!gp100_audio_input_level_set_target(&level, 20) ||
        !gp100_audio_input_level_apply(&level, samples, 2u) ||
        fabsf(level.current_gain - 0.02f) > 0.000001f ||
        samples[0] != 0.0f || samples[1] != 0.0f ||
        fabsf(samples[2] - 0.01f) > 0.000001f ||
        fabsf(samples[3] + 0.005f) > 0.000001f) {
        return 1;
    }
    level.current_gain = 10.0f;
    level.target_db = 20;
    samples[0] = 2.0f;
    samples[1] = -2.0f;
    if (!gp100_audio_input_level_apply(&level, samples, 1u) ||
        samples[0] != 1.0f || samples[1] != -1.0f ||
        gp100_audio_input_level_set_target(&level, -21) ||
        gp100_audio_input_level_set_target(&level, 21) ||
        gp100_audio_input_level_apply(NULL, samples, 1u)) {
        return 2;
    }
    return 0;
}

static int test_volume_pedal_gain(void)
{
    gp100_audio_volume_pedal_t pedal;
    float samples[4] = {1.0f, -1.0f, 0.5f, -0.5f};

    gp100_audio_volume_pedal_reset(&pedal);
    if (pedal.current_gain != 0u || pedal.target_percent != 100u ||
        !gp100_audio_volume_pedal_apply(&pedal, samples, 2u) ||
        pedal.current_gain != 1u ||
        fabsf(samples[0] - 1.0f / 255.0f) > 0.000001f ||
        fabsf(samples[3] + 0.5f / 255.0f) > 0.000001f) {
        return 1;
    }

    pedal.current_gain = 128u;
    pedal.target_percent = 50u;
    samples[0] = 1.0f;
    samples[1] = -1.0f;
    if (!gp100_audio_volume_pedal_apply(&pedal, samples, 1u) ||
        pedal.current_gain != 127u ||
        fabsf(samples[0] - 127.0f / 255.0f) > 0.000001f ||
        fabsf(samples[1] + 127.0f / 255.0f) > 0.000001f ||
        !gp100_audio_volume_pedal_apply(&pedal, samples, 1u) ||
        pedal.current_gain != 127u) {
        return 2;
    }
    return 0;
}

static bool test_master_gain(void)
{
    gp100_audio_master_level_t master;
    float samples[2] = {1.0f, -0.5f};

    gp100_audio_master_level_reset(&master);
    if (master.current_gain != 0u || master.target_percent != 100u ||
        !gp100_audio_master_level_apply(&master, samples, 1u) ||
        samples[0] != 1.0f / 256.0f || samples[1] != -0.5f / 256.0f) {
        return false;
    }
    master.current_gain = 128u;
    master.target_percent = 50u;
    samples[0] = 1.0f;
    samples[1] = -1.0f;
    return gp100_audio_master_level_apply(&master, samples, 1u) &&
           master.current_gain == 127u &&
           samples[0] == 127.0f / 256.0f && samples[1] == -127.0f / 256.0f;
}

static bool test_volume_pedal_pcm(void)
{
    gp100_audio_volume_pedal_t pedal = {255u, 100u};
    const float unit = 0x1p-23f;
    float samples[4] = {8421504 * unit, -8421504 * unit,
                        8421505 * unit, -8421505 * unit};

    return gp100_audio_volume_pedal_apply_pcm(&pedal, samples, 2u) &&
           samples[0] == 8421504 * unit && samples[1] == -8421504 * unit &&
           samples[2] == -8421504 * unit && samples[3] == 8421504 * unit;
}

int main(void)
{
    int result = test_ramp_and_scale();
    if (result != 0) {
        fprintf(stderr, "ramp/scale test failed: %d\n", result);
        return 10 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 20 + result;
    }
    result = test_input_db_gain();
    if (result != 0) {
        fprintf(stderr, "input dB gain test failed: %d\n", result);
        return 30 + result;
    }
    result = test_volume_pedal_gain();
    if (result != 0) {
        fprintf(stderr, "volume pedal gain test failed: %d\n", result);
        return 40 + result;
    }
    if (!test_master_gain()) {
        fprintf(stderr, "master gain did not follow the factory Q8 ramp\n");
        return 50;
    }
    if (!test_volume_pedal_pcm()) {
        fprintf(stderr, "expression PCM gain did not wrap at signed 32-bit limits\n");
        return 60;
    }
    return 0;
}
