#include "booster.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

#define GP100_BOOST14_GAIN_STEP 0.0001f

static const float boost_drive_curve[100] = {
    1.0000f, 1.0000f, 0.9536f, 0.9243f, 0.8826f,
    0.8562f, 0.8186f, 0.7949f, 0.7611f, 0.7397f,
    0.7093f, 0.6807f, 0.6626f, 0.6369f, 0.6206f,
    0.5974f, 0.5827f, 0.5618f, 0.5485f, 0.5295f,
    0.5117f, 0.5003f, 0.4841f, 0.4738f, 0.4590f,
    0.4496f, 0.4362f, 0.4276f, 0.4152f, 0.4073f,
    0.3959f, 0.3850f, 0.3781f, 0.3680f, 0.3615f,
    0.3521f, 0.3460f, 0.3371f, 0.3314f, 0.3231f,
    0.3150f, 0.3098f, 0.3021f, 0.2970f, 0.2897f,
    0.2848f, 0.2777f, 0.2731f, 0.2662f, 0.2617f,
    0.2550f, 0.2484f, 0.2440f, 0.2375f, 0.2333f,
    0.2269f, 0.2227f, 0.2165f, 0.2123f, 0.2061f,
    0.2000f, 0.1959f, 0.1897f, 0.1857f, 0.1796f,
    0.1756f, 0.1695f, 0.1655f, 0.1595f, 0.1555f,
    0.1495f, 0.1435f, 0.1395f, 0.1335f, 0.1295f,
    0.1235f, 0.1195f, 0.1135f, 0.1095f, 0.1035f,
    0.0976f, 0.0936f, 0.0876f, 0.0836f, 0.0776f,
    0.0737f, 0.0677f, 0.0637f, 0.0577f, 0.0537f,
    0.0478f, 0.0418f, 0.0378f, 0.0318f, 0.0278f,
    0.0219f, 0.0179f, 0.0119f, 0.0079f, 0.0019f
};

static const gp100_first_order_coefficients_t
    boost_bright_coefficients[2] = {
        {0x1.6a082cp-3f, 0x1.6a082cp-3f, -0x1.4afbeap-1f},
        {0x1.8c92c6p-1f, 0x1.8c92c6p-1f, 0x1.19259p-1f}
    };

static const gp100_first_order_coefficients_t
    boost_output_coefficients = {
        0x1.fee69p-1f, -0x1.fee69p-1f, -0x1.fff9b2p-1f
    };

static float boost_shape(float input)
{
    float magnitude = fabsf(input);
    float square = magnitude * magnitude;
    float cube = magnitude * square;

    if (input < 0.0f) {
        return -(cube * 4.373f + square * -10.7f +
                 magnitude * 28.9f) /
               (cube + square * 0.09519f +
                magnitude * -4.979f + 26.63f);
    }
    return (cube * 5.321f + square * -17.02f +
            magnitude * 55.15f) /
           (cube + square * 0.2738f +
            magnitude * -9.044f + 51.41f);
}

static float process_first_order(
    gp100_first_order_state_t *state,
    const gp100_first_order_coefficients_t *coefficients,
    float input)
{
    float output = input * coefficients->b0 +
                   state->x1 * coefficients->b1 -
                   state->y1 * coefficients->a1;

    state->x1 = input;
    state->y1 = output;
    return output;
}

static void configure_boost(gp100_boost_t *booster,
                            uint8_t gain,
                            bool bright)
{
    float drive = boost_drive_curve[gain] * 0.885f + 0.115f;

    booster->gain = gain;
    booster->bright = bright;
    booster->drive = drive;
    booster->input_numerator =
        drive * -0.7421875f - 0.7420371770858765f;
    booster->input_denominator =
        (float)(0x1.a897e7a769720p-13 - (double)drive);
    booster->bright_filter = boost_bright_coefficients[bright ? 1u : 0u];
    booster->output_filter = boost_output_coefficients;
}

bool gp100_boost_reset(gp100_boost_t *booster,
                            uint8_t gain,
                            bool bright)
{
    if (booster == NULL || gain > 99u) {
        return false;
    }
    memset(booster, 0, sizeof(*booster));
    configure_boost(booster, gain, bright);
    booster->configured = true;
    return true;
}

bool gp100_boost_process(gp100_boost_t *booster,
                              uint8_t gain,
                              bool bright,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled)
{
    size_t frame;

    if (booster == NULL || !booster->configured || gain > 99u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    if (!enabled || frame_count == 0u) {
        return true;
    }

    if (booster->gain != gain || booster->bright != bright) {
        configure_boost(booster, gain, bright);
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        float filtered = interleaved_stereo[frame * 2u] *
                         booster->input_numerator /
                         booster->input_denominator;
        float step = (filtered - booster->oversample_previous) / 3.0f;
        float shaped[GP100_BOOST_OVERSAMPLE];
        float fir_output;
        float output;

        shaped[0] = boost_shape(booster->oversample_previous);
        shaped[1] = boost_shape(booster->oversample_previous + step);
        shaped[2] = boost_shape(booster->oversample_previous + step * 2.0f);
        shaped[3] = boost_shape(booster->oversample_previous + step * 3.0f);
        booster->oversample_previous = filtered;

        fir_output = -gp100_decimate_four(booster->fir_history, shaped);
        output = process_first_order(&booster->bright_state,
                                     &booster->bright_filter, fir_output);
        output = process_first_order(&booster->output_state,
                                     &booster->output_filter, output);
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}

/*
 * The module indexes a 256-entry, four-decimal factory curve by gain * 2.55.
 * Catalog parameters are integral, so these are the 100 reachable entries.
 */
static const float boost14_gain_curve[100] = {
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

static const gp100_biquad_coefficients_t boost14_coefficients[] = {
    {
        0x1.b100e2p+2f, -0x1.a29ddp+3f, 0x1.94ad6p+2f,
        1.0f, -0x1.e3d36ep+0f, 0x1.cc026ap-1f
    },
    {
        0x1.a29ddp+3f, -0x1.054468p+1f, 0x1.c0ca6p-15f,
        1.0f, -0x1.08d9c4p-4f, 0x1.0025cp-7f
    },
    {
        0x1.26d538p-1f, -0x1.b7b73p-1f, 0x1.66969cp-2f,
        1.0f, -0x1.67bf38p+0f, 0x1.0af87p-1f
    },
    {
        0x1.b7b73p-1f, -0x1.1d0eb4p-2f, 0x1.07c096p-3f,
        1.0f, -0x1.60eacep-2f, 0x1.2221e4p-2f
    }
};

static float process_stage(gp100_biquad_channel_state_t *state,
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

static float move_toward(float current, float target)
{
    if (current < target) {
        current += GP100_BOOST14_GAIN_STEP;
        if (current > target) {
            current = target;
        }
    } else if (current > target) {
        current -= GP100_BOOST14_GAIN_STEP;
        if (current < target) {
            current = target;
        }
    }
    return current;
}

bool gp100_boost14_reset(gp100_boost14_t *booster,
                              uint8_t gain)
{
    size_t stage;

    if (booster == NULL || gain > 99u) {
        return false;
    }
    memset(booster, 0, sizeof(*booster));
    for (stage = 0u; stage < GP100_BOOST14_STAGE_COUNT; ++stage) {
        booster->coefficients[stage] = boost14_coefficients[stage];
    }
    booster->target_gain = boost14_gain_curve[gain];
    booster->current_gain = booster->target_gain;
    booster->configured = true;
    return true;
}

bool gp100_boost14_process(gp100_boost14_t *booster,
                                uint8_t gain,
                                float *interleaved_stereo,
                                size_t frame_count,
                                bool enabled)
{
    size_t frame;

    if (booster == NULL || !booster->configured || gain > 99u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    booster->target_gain = boost14_gain_curve[gain];
    if (!enabled || frame_count == 0u) {
        return true;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        size_t sample = frame * 2u;
        size_t stage;
        float output = interleaved_stereo[sample];

        booster->current_gain =
            move_toward(booster->current_gain, booster->target_gain);
        for (stage = 0u; stage < GP100_BOOST14_STAGE_COUNT; ++stage) {
            output = process_stage(&booster->stage[stage],
                                   &booster->coefficients[stage], output);
        }
        output *= (booster->current_gain * 0.15f + 0.005f) * 8.0f;
        interleaved_stereo[sample] = output;
        interleaved_stereo[sample + 1u] = output;
    }
    return true;
}
