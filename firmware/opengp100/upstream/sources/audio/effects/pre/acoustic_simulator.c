#include "acoustic_simulator.h"

#include <stdint.h>
#include <string.h>

#define AC_PARAMETER_STEP 0.0001f
#define AC_MODE_STEP 0.0005f

/*
 * The factory tables contain 256 four-decimal values and are indexed by
 * parameter * 2.55. These are the 100 entries reachable by integral controls.
 */
static const uint16_t ac_body_curve[100] = {
    500, 500, 535, 559, 595, 619, 655, 679, 715, 739,
    774, 810, 834, 870, 894, 930, 954, 990, 1013, 1049,
    1085, 1109, 1145, 1169, 1205, 1229, 1265, 1289, 1325, 1348,
    1384, 1420, 1444, 1481, 1505, 1541, 1565, 1601, 1626, 1662,
    1699, 1723, 1760, 1785, 1822, 1847, 1885, 1910, 1948, 1974,
    2012, 2052, 2078, 2118, 2145, 2187, 2215, 2257, 2286, 2330,
    2375, 2406, 2453, 2485, 2535, 2569, 2621, 2657, 2713, 2751,
    2811, 2873, 2915, 2982, 3028, 3100, 3150, 3229, 3283, 3368,
    3458, 3521, 3620, 3689, 3797, 3873, 3992, 4076, 4207, 4299,
    4445, 4599, 4708, 4878, 4998, 5188, 5320, 5530, 5677, 5909
};

static const uint16_t ac_top_curve[100] = {
    0, 47, 118, 166, 237, 284, 355, 403, 474, 521,
    592, 664, 711, 782, 830, 901, 948, 1019, 1067, 1138,
    1209, 1256, 1328, 1375, 1446, 1494, 1565, 1613, 1684, 1731,
    1803, 1874, 1922, 1993, 2041, 2113, 2161, 2233, 2281, 2353,
    2425, 2474, 2547, 2596, 2669, 2718, 2792, 2842, 2917, 2967,
    3043, 3120, 3172, 3250, 3303, 3383, 3437, 3519, 3575, 3660,
    3746, 3805, 3895, 3956, 4049, 4113, 4211, 4279, 4382, 4453,
    4562, 4676, 4754, 4875, 4958, 5088, 5178, 5318, 5414, 5565,
    5724, 5834, 6006, 6126, 6314, 6445, 6650, 6793, 7018, 7175,
    7421, 7682, 7865, 8152, 8353, 8669, 8890, 9239, 9482, 9866
};

static const uint16_t ac_volume_curve[100] = {
    0, 0, 20, 60, 120, 160, 220, 261, 321, 361,
    421, 481, 522, 582, 622, 682, 722, 783, 823, 883,
    943, 983, 1044, 1084, 1144, 1184, 1245, 1285, 1345, 1385,
    1446, 1506, 1547, 1607, 1647, 1708, 1748, 1809, 1850, 1911,
    1972, 2013, 2074, 2115, 2177, 2218, 2280, 2322, 2385, 2427,
    2491, 2555, 2598, 2663, 2707, 2774, 2819, 2887, 2933, 3003,
    3074, 3122, 3196, 3246, 3322, 3374, 3454, 3508, 3592, 3649,
    3738, 3829, 3892, 3988, 4055, 4159, 4230, 4341, 4418, 4537,
    4663, 4749, 4885, 4979, 5127, 5229, 5390, 5502, 5678, 5801,
    5993, 6197, 6339, 6563, 6719, 6965, 7137, 7408, 7598, 7896
};

/*
 * Fixed IIR coefficients in the order consumed by the readable topology
 * below. Hexadecimal floats preserve the shipped single-precision values.
 */
static const float ac_coefficients[115] = {
    -0x1.d8031ep-1f, -0x1.ddc95p-1f, -0x1.efc51ap-1f, -0x1.d8031ep-1f,
    -0x1.d0dea4p-1f, -0x1.eab5e2p-1f, -0x1.b5bce8p-1f, -0x1.f58d9cp-1f,
    -0x1.946524p-1f, 0x1.6b44a6p-2f, -0x1.da1f84p+0f, 0x1.c7cb4p-1f,
    -0x1.decf1cp+0f, 0x1.c77a2ep-1f, -0x1.d1068p+0f, 0x1.c82d7cp-1f,
    -0x1.c5172ep+0f, 0x1.c8c8bcp-1f, -0x1.f95202p+0f, 0x1.f55e3p-1f,
    0x1.179846p-5f, 0x1.179846p-5f, 0x1.d1da48p-6f, 0x1.d1da48p-6f,
    0x1.61af7cp-7f, 0x1.61af7cp-7f, -0x1.fcae66p+0f, 0x1.fa03d6p-1f,
    -0x1.fcdc86p+0f, 0x1.fa0166p-1f, -0x1.fcc9bap+0f, 0x1.fa0438p-1f,
    0x1.179846p-5f, 0x1.179846p-5f, 0x1.51136ep-5f, 0x1.51136ep-5f,
    0x1.0233p-6f, 0x1.0233p-6f, -0x1.fe95f2p+0f, 0x1.fd3ddcp-1f,
    -0x1.fe8e72p+0f, 0x1.fd3d36p-1f, 0x1.157174p-4f, 0x1.157174p-4f,
    0x1.4e4c94p-8f, 0x1.4e4c94p-8f, -0x1.57d1f6p-3f, -0x1.b887d6p-5f,
    -0x1.7abc64p-1f, -0x1.3024dcp-3f, -0x1.ab982ep-1f, -0x1.4c3994p-3f,
    -0x1.572256p+2f, 0x1.572256p+3f, -0x1.572256p+2f, 0x1.fd1046p-2f,
    -0x1.7dcc34p+0f, 0x1.7dcc34p+0f, -0x1.fd1046p-2f, -0x1.90c858p+0f,
    0x1.153b1ap+0f, -0x1.50746ep-2f, 0x1.90de6cp-2f, -0x1.2ca6dp+0f,
    0x1.2ca6dp+0f, -0x1.90de6cp-2f, -0x1.377368p+0f, 0x1.6196fcp-1f,
    -0x1.cbef8ap-3f, -0x1.88686cp-5f, 0x0p+0f, 0x1.88686cp-5f,
    -0x1.dc87cp-5f, 0x0p+0f, 0x1.dc87cp-5f, -0x1.d57218p-5f,
    0x0p+0f, 0x1.d57218p-5f, -0x1.0b3eacp-5f, 0x0p+0f,
    0x1.0b3eacp-5f, 0x1.896088p-14f, 0x1.270866p-12f, 0x1.270866p-12f,
    0x1.896088p-14f, -0x1.5ba7c2p+1f, 0x1.398c02p+1f, -0x1.772eacp-1f,
    -0x1.792c7ap-6f, 0x0p+0f, 0x1.792c7ap-6f, -0x1.65426ap-7f,
    0x0p+0f, 0x1.65426ap-7f, -0x1.655438p-7f, 0x0p+0f,
    0x1.655438p-7f, -0x1.64d5dp-7f, 0x0p+0f, 0x1.64d5dp-7f,
    -0x1.52ce04p-8f, 0x0p+0f, 0x1.52ce04p-8f, -0x1.5436cp-8f,
    0x0p+0f, 0x1.5436cp-8f, -0x1.8b6c6p+1f, 0x0p+0f,
    0x1.8b6c6p+1f, -0x1.f3b454p-1f, 0x0p+0f, 0x1.f3b454p-1f,
    -0x1.be4accp+2f, 0x0p+0f, 0x1.be4accp+2f
};

static float move_toward(float current, float target)
{
    if (current < target) {
        current += AC_PARAMETER_STEP;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= AC_PARAMETER_STEP;
        return current < target ? target : current;
    }
    return target;
}

static float run_filter(gp100_ac_sim_filter_state_t *state,
                        const float *numerator,
                        size_t numerator_count,
                        const float *denominator,
                        size_t denominator_count,
                        float input)
{
    float output = input * numerator[0];
    size_t index;

    for (index = 1u; index < numerator_count; ++index) {
        output += state->input[index - 1u] * numerator[index];
    }
    for (index = 0u; index < denominator_count; ++index) {
        output -= state->output[index] * denominator[index];
    }
    for (index = GP100_AC_SIM_FILTER_ORDER - 1u;
         index > 0u; --index) {
        state->input[index] = state->input[index - 1u];
        state->output[index] = state->output[index - 1u];
    }
    state->input[0] = input;
    state->output[0] = output;
    return output;
}

static float section(gp100_ac_sim_t *simulator,
                     size_t state_index,
                     size_t numerator_index,
                     size_t numerator_count,
                     size_t denominator_index,
                     size_t denominator_count,
                     float input)
{
    return run_filter(&simulator->filter[state_index],
                      &ac_coefficients[numerator_index], numerator_count,
                      &ac_coefficients[denominator_index], denominator_count,
                      input);
}

static void set_targets(gp100_ac_sim_t *simulator,
                        uint8_t body,
                        uint8_t top,
                        uint8_t volume,
                        uint8_t mode)
{
    simulator->body_parameter = body;
    simulator->top_parameter = top;
    simulator->volume_parameter = volume;
    simulator->body_target = (float)ac_body_curve[body] * 0.0001f;
    simulator->top_target = (float)ac_top_curve[top] * 0.0001f;
    simulator->volume_target = (float)ac_volume_curve[volume] * 0.0001f;
    simulator->mode_target = mode;
}

bool gp100_ac_sim_reset(gp100_ac_sim_t *simulator,
                             uint8_t body,
                             uint8_t top,
                             uint8_t volume,
                             uint8_t mode)
{
    if (simulator == NULL || body > 99u || top > 99u ||
        volume > 99u || mode > 3u) {
        return false;
    }
    memset(simulator, 0, sizeof(*simulator));
    set_targets(simulator, body, top, volume, mode);
    simulator->body = simulator->body_target;
    simulator->top = simulator->top_target;
    simulator->volume = simulator->volume_target;
    simulator->mode = mode;
    simulator->configured = true;
    return true;
}

bool gp100_ac_sim_process(gp100_ac_sim_t *simulator,
                               uint8_t body,
                               uint8_t top,
                               uint8_t volume,
                               uint8_t mode,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled)
{
    size_t frame;

    if (simulator == NULL || !simulator->configured || body > 99u ||
        top > 99u || volume > 99u || mode > 3u ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_targets(simulator, body, top, volume, mode);

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float shaped;
        float mode_body;
        float parallel;
        float common;
        float common_tone;
        float branch1;
        float branch2;
        float branch3;
        float output1;
        float output2;
        float output3;
        float output_sample;
        size_t parallel_mode;
        size_t group;

        if (!enabled) {
            continue;
        }
        simulator->body = move_toward(simulator->body, simulator->body_target);
        simulator->top = move_toward(simulator->top, simulator->top_target);
        simulator->volume = move_toward(
            simulator->volume, simulator->volume_target);

        input = interleaved_stereo[frame * 2u];
        if (simulator->mode != simulator->mode_target) {
            simulator->fading_out = true;
            input *= simulator->mode_fade_out_gain;
            simulator->mode_fade_out_gain -= AC_MODE_STEP;
            if (simulator->mode_fade_out_gain < 0.0f) {
                simulator->mode_gain = 0.0f;
                simulator->mode_fade_out_gain = 1.0f;
                simulator->mode = simulator->mode_target;
                simulator->fading_out = false;
            }
        } else {
            simulator->fading_out = false;
            input *= simulator->mode_gain;
            simulator->mode_gain += AC_MODE_STEP;
            if (simulator->mode_gain > 1.0f) {
                simulator->mode_gain = 1.0f;
            }
        }

        /* Shared input shaper and the mode-dependent acoustic-body branch. */
        shaped = section(simulator, 0u, 52u, 3u, 8u, 2u, input);
        if (simulator->mode == 2u) {
            mode_body = section(
                simulator, 1u, 55u, 4u, 59u, 3u, shaped);
        } else {
            mode_body = section(
                simulator, 2u, 62u, 4u, 66u, 3u, shaped);
        }

        parallel = section(simulator, 3u + simulator->mode,
                           69u + (size_t)simulator->mode * 3u, 3u,
                           10u + (size_t)simulator->mode * 2u, 2u, input);
        common = section(simulator, 7u, 81u, 4u, 85u, 3u, input);
        common_tone = section(simulator, 8u, 88u, 3u, 18u, 2u, common);

        group = simulator->mode <= 1u ? 0u : (size_t)simulator->mode - 1u;
        branch1 = section(simulator, 9u + group,
                          20u + group * 2u, 2u, group, 1u, common_tone);

        /* Modes 1 and 3 share this branch's input filter. */
        parallel_mode = simulator->mode == 0u ? 0u :
                        simulator->mode == 2u ? 2u : 1u;
        branch2 = section(simulator, 12u + parallel_mode,
                          91u + parallel_mode * 3u, 3u,
                          26u + parallel_mode * 2u, 2u, common);
        branch2 = section(simulator, 15u + group,
                          32u + group * 2u, 2u,
                          3u + group, 1u, branch2);

        if (simulator->mode == 1u) {
            branch3 = section(
                simulator, 18u, 100u, 3u, 38u, 2u, common);
        } else {
            branch3 = section(
                simulator, 19u, 103u, 3u, 40u, 2u, common);
        }
        group = simulator->mode == 3u ? 1u : 0u;
        branch3 = section(simulator, 20u + group,
                          42u + group * 2u, 2u,
                          6u + group, 1u, branch3);

        mode_body *= simulator->mode == 3u
                         ? simulator->top * 1.076924f + 0.1f
                         : simulator->top * 0.88462f + 0.292304f;
        parallel *= (1.0f - simulator->top * 0.8f) * 1.3153279f;
        output1 = section(
            simulator, 22u, 106u, 3u, 46u, 2u, mode_body);
        output2 = section(
            simulator, 23u, 109u, 3u, 48u, 2u, parallel);
        output3 = section(
            simulator, 24u, 112u, 3u, 50u, 2u,
            (branch1 + branch2 + branch3) *
                (simulator->body * 0.9999f + 0.00009999f));
        output_sample = (output1 + output2 + output3) * simulator->volume;
        interleaved_stereo[frame * 2u] = output_sample;
        interleaved_stereo[frame * 2u + 1u] = output_sample;
    }
    return true;
}
