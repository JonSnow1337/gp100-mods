#include "compressor.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define GP100_COMP_PARAMETER_STEP 0.0001f

/*
 * The module indexes a 256-entry, four-decimal factory curve by
 * Sustain * 2.55. Catalog parameters are integral, so only these 100 entries
 * are reachable through the product UI and protocol.
 */
static const float comp_sustain_curve[100] = {
    0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,
    0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,
    0.0000f, 0.0001f, 0.0001f, 0.0002f, 0.0003f,
    0.0004f, 0.0005f, 0.0007f, 0.0008f, 0.0011f,
    0.0014f, 0.0017f, 0.0021f, 0.0024f, 0.0030f,
    0.0034f, 0.0042f, 0.0047f, 0.0057f, 0.0064f,
    0.0075f, 0.0088f, 0.0098f, 0.0113f, 0.0125f,
    0.0144f, 0.0158f, 0.0180f, 0.0196f, 0.0223f,
    0.0251f, 0.0272f, 0.0306f, 0.0330f, 0.0368f,
    0.0396f, 0.0440f, 0.0472f, 0.0522f, 0.0558f,
    0.0615f, 0.0676f, 0.0719f, 0.0788f, 0.0836f,
    0.0913f, 0.0967f, 0.1053f, 0.1113f, 0.1208f,
    0.1309f, 0.1380f, 0.1492f, 0.1570f, 0.1692f,
    0.1778f, 0.1913f, 0.2007f, 0.2154f, 0.2256f,
    0.2417f, 0.2586f, 0.2704f, 0.2887f, 0.3015f,
    0.3214f, 0.3353f, 0.3568f, 0.3718f, 0.3951f,
    0.4194f, 0.4363f, 0.4625f, 0.4807f, 0.5089f,
    0.5284f, 0.5586f, 0.5795f, 0.6119f, 0.6343f,
    0.6689f, 0.7050f, 0.7298f, 0.7683f, 0.7948f,
    0.8358f, 0.8641f, 0.9077f, 0.9377f, 0.9841f
};

static const float comp4_clip_curve[100] = {
    0.0000f, 0.0000f, 0.0012f, 0.0020f, 0.0032f,
    0.0040f, 0.0052f, 0.0060f, 0.0072f, 0.0080f,
    0.0092f, 0.0104f, 0.0112f, 0.0124f, 0.0131f,
    0.0143f, 0.0151f, 0.0163f, 0.0171f, 0.0183f,
    0.0195f, 0.0203f, 0.0215f, 0.0223f, 0.0235f,
    0.0243f, 0.0256f, 0.0264f, 0.0276f, 0.0284f,
    0.0297f, 0.0309f, 0.0317f, 0.0330f, 0.0339f,
    0.0352f, 0.0361f, 0.0375f, 0.0384f, 0.0398f,
    0.0413f, 0.0423f, 0.0439f, 0.0449f, 0.0466f,
    0.0478f, 0.0496f, 0.0509f, 0.0529f, 0.0544f,
    0.0566f, 0.0591f, 0.0608f, 0.0635f, 0.0655f,
    0.0686f, 0.0709f, 0.0745f, 0.0771f, 0.0813f,
    0.0858f, 0.0891f, 0.0944f, 0.0982f, 0.1044f,
    0.1088f, 0.1159f, 0.1211f, 0.1294f, 0.1354f,
    0.1451f, 0.1557f, 0.1634f, 0.1757f, 0.1845f,
    0.1988f, 0.2091f, 0.2256f, 0.2374f, 0.2565f,
    0.2773f, 0.2921f, 0.3160f, 0.3330f, 0.3604f,
    0.3800f, 0.4114f, 0.4337f, 0.4695f, 0.4950f,
    0.5358f, 0.5799f, 0.6112f, 0.6612f, 0.6967f,
    0.7533f, 0.7934f, 0.8574f, 0.9027f, 0.9748f
};

static const gp100_biquad_coefficients_t comp_filter_coefficients[2] = {
    {
        0x1.3c6a0ap-1f, -0x1.24d2p+0f, 0x1.0d3c36p-1f,
        1.0f, -0x1.d8259cp+0f, 0x1.b0723ap-1f
    },
    {
        0x1.24d2p+0f, 0x1.7a293ap-2f, 0x1.1fad44p-7f,
        1.0f, 0x1.10921ep-7f, 0x1.48d55cp-17f
    }
};

static float move_toward(float current, float target)
{
    if (current < target) {
        current += GP100_COMP_PARAMETER_STEP;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= GP100_COMP_PARAMETER_STEP;
        return current < target ? target : current;
    }
    return target;
}

static float process_biquad(
    gp100_biquad_channel_state_t *state,
    const gp100_biquad_coefficients_t *coefficients,
    float input)
{
    float output = input * coefficients->b0 +
                   state->x1 * coefficients->b1 +
                   state->x2 * coefficients->b2 -
                   state->y1 * coefficients->a1 -
                   state->y2 * coefficients->a2;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static void set_comp_targets(gp100_comp_t *compressor,
                             uint8_t sustain,
                             uint8_t output)
{
    compressor->sustain_parameter = sustain;
    compressor->output_parameter = output;
    compressor->sustain_target = comp_sustain_curve[sustain];
    compressor->output_target = (float)output * 0.01f;
}

bool gp100_comp_reset(gp100_comp_t *compressor,
                           uint8_t sustain,
                           uint8_t output)
{
    if (compressor == NULL || sustain > 99u || output > 99u) {
        return false;
    }

    memset(compressor, 0, sizeof(*compressor));
    set_comp_targets(compressor, sustain, output);
    compressor->sustain = compressor->sustain_target;
    compressor->output_level = compressor->output_target;
    compressor->attack_coefficient = 0x1.f7912cp-1f;
    compressor->release_coefficient = 0x1.ffe4cap-1f;
    memcpy(compressor->filter_coefficients, comp_filter_coefficients,
           sizeof(comp_filter_coefficients));
    compressor->configured = true;
    return true;
}

bool gp100_comp_process(gp100_comp_t *compressor,
                             uint8_t sustain,
                             uint8_t output,
                             float *interleaved_stereo,
                             size_t frame_count,
                             bool enabled)
{
    size_t frame;

    if (compressor == NULL || !compressor->configured || sustain > 99u ||
        output > 99u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    set_comp_targets(compressor, sustain, output);
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float detector;
        float reduction_db = 0.0f;
        float coefficient;
        float compressed;
        float output_sample;

        compressor->sustain = move_toward(
            compressor->sustain, compressor->sustain_target);
        compressor->output_level = move_toward(
            compressor->output_level, compressor->output_target);

        if (!enabled) {
            continue;
        }

        input = interleaved_stereo[frame * 2u];
        detector = fabsf((compressor->sustain * 97.0f + 3.0f) * input);
        if (detector > 0.0f) {
            float input_db = log10f(detector) * 20.0f;
            float compressed_db = input_db;

            if (input_db >= -10.0f) {
                compressed_db = (input_db + 10.0f) / 100.0f - 10.0f;
            }
            reduction_db = compressed_db - input_db;
        }

        coefficient = reduction_db > compressor->gain_reduction_db
                          ? compressor->release_coefficient
                          : compressor->attack_coefficient;
        compressor->gain_reduction_db =
            coefficient * compressor->gain_reduction_db +
            (1.0f - coefficient) * reduction_db;
        compressed = input * powf(
            10.0f, compressor->gain_reduction_db / 20.0f);
        output_sample = process_biquad(
            &compressor->filter_state[0],
            &compressor->filter_coefficients[0], compressed);
        output_sample = process_biquad(
            &compressor->filter_state[1],
            &compressor->filter_coefficients[1], output_sample);
        output_sample *= 5.0f;
        output_sample *= compressor->output_level;
        output_sample *= compressor->sustain * 10.0f + 1.0f;
        interleaved_stereo[frame * 2u] = output_sample;
        interleaved_stereo[frame * 2u + 1u] = output_sample;
    }
    return true;
}

static void set_comp4_targets(gp100_comp4_t *compressor,
                              uint8_t sustain,
                              uint8_t attack,
                              uint8_t output,
                              uint8_t clip)
{
    compressor->sustain_parameter = sustain;
    compressor->attack_parameter = attack;
    compressor->output_parameter = output;
    compressor->clip_parameter = clip;
    compressor->sustain_target = comp_sustain_curve[sustain];
    compressor->attack_target = (float)attack * 0.01f;
    compressor->output_target = (float)output * 0.01f;
    compressor->clip_target = comp4_clip_curve[clip];
}

static void configure_comp4_attack(gp100_comp4_t *compressor)
{
    float attack_time = (float)((double)compressor->attack * 0.008 + 0.002);
    float release_time = attack_time * 50.0f;

    compressor->attack_time_seconds = attack_time;
    compressor->attack_coefficient =
        expf(-2.1972f / (attack_time * 44100.0f));
    compressor->release_coefficient =
        expf(-2.1972f / (release_time * 44100.0f));
}

static void configure_comp4_prefilter(gp100_comp4_t *compressor)
{
    float clip = compressor->clip;
    gp100_comp4_prefilter_coefficients_t *coefficients =
        &compressor->prefilter_coefficients;

    coefficients->numerator[0] = clip * 0.2938786f + 0.0010096839f;
    coefficients->numerator[1] = 0.001011194f - clip * 0.29343906f;
    coefficients->numerator[2] = clip * -0.2938786f - 0.0010066637f;
    coefficients->numerator[3] = clip * 0.29343906f - 0.0010081738f;
    coefficients->denominator[0] = clip + 0.002938661f;
    coefficients->denominator[1] = clip * -2.0002854f - 0.00058403675f;
    coefficients->denominator[2] = clip * 1.0515488f - 0.0029355402f;
    coefficients->denominator[3] =
        0.0005871577f - clip * 0.051248256f;
}

static float process_comp4_prefilter(gp100_comp4_t *compressor,
                                     float input)
{
    gp100_comp4_prefilter_state_t *state = &compressor->prefilter_state;
    const gp100_comp4_prefilter_coefficients_t *coefficients =
        &compressor->prefilter_coefficients;
    float output = input * coefficients->numerator[0] +
                   state->input[0] * coefficients->numerator[1] +
                   state->input[1] * coefficients->numerator[2] +
                   state->input[2] * coefficients->numerator[3] -
                   state->output[0] * coefficients->denominator[1] -
                   state->output[1] * coefficients->denominator[2] -
                   state->output[2] * coefficients->denominator[3];

    output /= coefficients->denominator[0];
    state->input[2] = state->input[1];
    state->input[1] = state->input[0];
    state->input[0] = input;
    state->output[2] = state->output[1];
    state->output[1] = state->output[0];
    state->output[0] = output;
    return output;
}

bool gp100_comp4_reset(gp100_comp4_t *compressor,
                            uint8_t sustain,
                            uint8_t attack,
                            uint8_t output,
                            uint8_t clip)
{
    if (compressor == NULL || sustain > 99u || attack > 99u ||
        output > 99u || clip > 99u) {
        return false;
    }

    memset(compressor, 0, sizeof(*compressor));
    set_comp4_targets(compressor, sustain, attack, output, clip);
    compressor->sustain = compressor->sustain_target;
    compressor->attack = compressor->attack_target;
    compressor->output_level = compressor->output_target;
    compressor->clip = compressor->clip_target;
    configure_comp4_attack(compressor);
    configure_comp4_prefilter(compressor);
    memcpy(compressor->filter_coefficients, comp_filter_coefficients,
           sizeof(comp_filter_coefficients));
    compressor->configured = true;
    return true;
}

bool gp100_comp4_process(gp100_comp4_t *compressor,
                              uint8_t sustain,
                              uint8_t attack,
                              uint8_t output,
                              uint8_t clip,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled)
{
    size_t frame;

    if (compressor == NULL || !compressor->configured || sustain > 99u ||
        attack > 99u || output > 99u || clip > 99u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    set_comp4_targets(compressor, sustain, attack, output, clip);
    for (frame = 0u; frame < frame_count; ++frame) {
        /*
         * The package tests current != target before refreshing derived
         * values. Tracking the actual change also refreshes the final slew
         * step and parameters that finished moving while bypassed.
         */
        float previous_attack = compressor->attack;
        float previous_clip = compressor->clip;
        float input;
        float detector_sample;
        float detector;
        float input_db;
        float compressed_db;
        float reduction_db;
        float coefficient;
        float compressed;
        float output_sample;

        compressor->sustain = move_toward(
            compressor->sustain, compressor->sustain_target);
        compressor->attack = move_toward(
            compressor->attack, compressor->attack_target);
        compressor->output_level = move_toward(
            compressor->output_level, compressor->output_target);
        compressor->clip = move_toward(
            compressor->clip, compressor->clip_target);

        if (!enabled) {
            continue;
        }
        if (compressor->attack != previous_attack) {
            configure_comp4_attack(compressor);
        }
        if (compressor->clip != previous_clip) {
            configure_comp4_prefilter(compressor);
        }

        input = interleaved_stereo[frame * 2u];
        detector_sample = process_comp4_prefilter(compressor, input);
        detector = fabsf(detector_sample *
                         (compressor->sustain * 97.0f + 3.0f)) + 0.000001f;
        input_db = log10f(detector) * 20.0f;
        compressed_db = input_db;
        if (input_db >= -10.0f) {
            compressed_db = (input_db + 10.0f) / 100.0f - 10.0f;
        }
        reduction_db = compressed_db - input_db;
        coefficient = reduction_db > compressor->gain_reduction_db
                          ? compressor->release_coefficient
                          : compressor->attack_coefficient;
        compressor->gain_reduction_db =
            coefficient * compressor->gain_reduction_db +
            (1.0f - coefficient) * reduction_db;
        compressed = input * powf(
            10.0f, compressor->gain_reduction_db / 20.0f);
        output_sample = process_biquad(
            &compressor->filter_state[0],
            &compressor->filter_coefficients[0], compressed);
        output_sample = process_biquad(
            &compressor->filter_state[1],
            &compressor->filter_coefficients[1], output_sample);
        output_sample *= 4.0f;
        output_sample *= compressor->output_level;
        output_sample *= compressor->sustain * 10.0f + 1.0f;
        interleaved_stereo[frame * 2u] = output_sample;
        interleaved_stereo[frame * 2u + 1u] = output_sample;
    }
    return true;
}
