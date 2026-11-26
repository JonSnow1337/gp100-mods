#include "auto_wah.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define AUTO_WAH_CONTROL_SCALE 0.01f
#define AUTO_WAH_SMOOTHING_STEP 0x1.db8cdap-13f
#define AUTO_WAH_RATE_SCALE 0x1.d6cb72p-13f
#define AUTO_WAH_RATE_OFFSET 0x1.305a24p-19f
#define AUTO_WAH_SAMPLE_RATE 44100.0f

/* Exact factory curve, stored as the original four-decimal values. */
static const uint16_t unsynced_rate_curve_x10000[253] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 1u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 5u, 5u,
    6u, 7u, 8u, 9u, 11u, 12u, 13u, 15u, 17u, 18u, 20u, 22u,
    24u, 27u, 29u, 32u, 34u, 37u, 40u, 43u, 46u, 50u, 53u, 57u,
    61u, 65u, 69u, 74u, 79u, 83u, 88u, 94u, 99u, 105u, 111u, 117u,
    123u, 129u, 136u, 143u, 150u, 158u, 165u, 173u, 181u, 190u, 198u, 207u,
    216u, 226u, 236u, 246u, 256u, 266u, 277u, 288u, 300u, 311u, 323u, 336u,
    348u, 361u, 374u, 388u, 402u, 416u, 430u, 445u, 461u, 476u, 492u, 508u,
    525u, 542u, 559u, 577u, 595u, 613u, 632u, 651u, 671u, 691u, 711u, 732u,
    753u, 774u, 796u, 819u, 841u, 864u, 888u, 912u, 936u, 961u, 987u, 1012u,
    1039u, 1065u, 1092u, 1120u, 1148u, 1176u, 1205u, 1235u, 1265u, 1295u,
    1326u, 1357u, 1389u, 1421u, 1454u, 1487u, 1521u, 1555u, 1590u, 1626u,
    1661u, 1698u, 1735u, 1772u, 1810u, 1849u, 1888u, 1927u, 1968u, 2008u,
    2050u, 2091u, 2134u, 2177u, 2220u, 2264u, 2309u, 2354u, 2400u, 2447u,
    2494u, 2542u, 2590u, 2639u, 2688u, 2738u, 2789u, 2840u, 2892u, 2945u,
    2998u, 3052u, 3106u, 3162u, 3217u, 3274u, 3331u, 3389u, 3447u, 3506u,
    3566u, 3626u, 3688u, 3749u, 3812u, 3875u, 3939u, 4004u, 4069u, 4135u,
    4202u, 4269u, 4337u, 4406u, 4475u, 4546u, 4617u, 4689u, 4761u, 4834u,
    4908u, 4983u, 5059u, 5135u, 5212u, 5290u, 5368u, 5448u, 5528u, 5609u,
    5690u, 5773u, 5856u, 5940u, 6025u, 6111u, 6197u, 6284u, 6372u, 6461u,
    6551u, 6642u, 6733u, 6825u, 6918u, 7012u, 7107u, 7203u, 7299u, 7397u,
    7495u, 7594u, 7694u, 7795u, 7896u, 7999u, 8102u, 8207u, 8312u, 8418u,
    8525u, 8633u, 8742u, 8851u, 8962u, 9074u, 9186u, 9299u, 9414u, 9529u,
    9645u, 9762u, 9881u
};

_Static_assert(sizeof(unsynced_rate_curve_x10000) /
                   sizeof(unsynced_rate_curve_x10000[0]) == 253u,
               "A-Wah Rate taper must cover every reachable index");

static bool parameters_valid(uint8_t depth,
                             float rate,
                             uint8_t output_level,
                             uint8_t low,
                             uint8_t resonance,
                             uint8_t high,
                             bool tempo_sync,
                             uint16_t tempo_period_ms)
{
    return depth <= 99u && output_level <= 99u && low <= 99u &&
           resonance <= 99u && high <= 99u &&
           rate >= 0.0f &&
           ((!tempo_sync && rate <= 99.0f) ||
            (tempo_sync && rate <= 10.0f && tempo_period_ms != 0u));
}

static float phase_step(uint8_t rate,
                        bool tempo_sync,
                        uint16_t tempo_period_ms)
{
    if (tempo_sync) {
        float period_samples = (float)tempo_period_ms * 44.1f;

        return 1.0f / (period_samples * gp100_effect_tempo_beats(rate) * 0.5f);
    }
    {
        size_t index = (size_t)((float)rate * 2.55f);
        float curve = (float)unsynced_rate_curve_x10000[index] * 0.0001f;

        return curve * AUTO_WAH_RATE_SCALE + AUTO_WAH_RATE_OFFSET;
    }
}

static void set_immediate_parameters(gp100_auto_wah_t *wah,
                                     uint8_t low,
                                     uint8_t resonance,
                                     uint8_t high)
{
    wah->low_hz = (float)low * AUTO_WAH_CONTROL_SCALE * 300.0f + 200.0f;
    wah->high_hz = (float)high * AUTO_WAH_CONTROL_SCALE * 4000.0f + 1000.0f;
    wah->resonance = (float)resonance * 9.0f * AUTO_WAH_CONTROL_SCALE + 1.0f;
}

static float process_filter(gp100_auto_wah_t *wah, float input)
{
    gp100_biquad_channel_state_t *state = &wah->filter_state;
    const gp100_biquad_coefficients_t *c = &wah->filter_coefficients;
    float output = (input * c->b0 + state->x1 * c->b1 +
                    state->x2 * c->b2 - state->y1 * c->a1 -
                    state->y2 * c->a2) / c->a0;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

bool gp100_auto_wah_reset(gp100_auto_wah_t *wah,
                               uint8_t depth,
                               float rate,
                               uint8_t output_level,
                               uint8_t low,
                               uint8_t resonance,
                               uint8_t high,
                               bool tempo_sync)
{
    if (wah == NULL || !parameters_valid(depth, rate, output_level, low,
                                          resonance, high, tempo_sync, 1u)) {
        return false;
    }
    memset(wah, 0, sizeof(*wah));
    wah->target_depth = (float)depth * AUTO_WAH_CONTROL_SCALE;
    wah->current_depth = wah->target_depth;
    wah->target_output_level =
        (float)output_level * AUTO_WAH_CONTROL_SCALE;
    wah->current_output_level = wah->target_output_level;
    wah->phase_direction = 1.0f;
    wah->rate = (uint8_t)rate;
    wah->tempo_sync = tempo_sync;
    set_immediate_parameters(wah, low, resonance, high);
    wah->configured = true;
    return true;
}

bool gp100_auto_wah_process(gp100_auto_wah_t *wah,
                                 uint8_t depth,
                                 float rate,
                                 uint8_t output_level,
                                 uint8_t low,
                                 uint8_t resonance,
                                 uint8_t high,
                                 bool tempo_sync,
                                 uint16_t tempo_period_ms,
                                 float *interleaved_stereo,
                                 size_t frame_count,
                                 bool enabled)
{
    float depth_delta;
    float phase_step_delta;
    float output_level_delta;
    size_t frame;

    if (wah == NULL || !wah->configured ||
        !parameters_valid(depth, rate, output_level, low, resonance, high,
                          tempo_sync, tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }

    wah->target_depth = (float)depth * AUTO_WAH_CONTROL_SCALE;
    wah->target_phase_step = phase_step((uint8_t)rate, tempo_sync, tempo_period_ms);
    wah->target_output_level =
        (float)output_level * AUTO_WAH_CONTROL_SCALE;
    wah->rate = (uint8_t)rate;
    wah->tempo_sync = tempo_sync;
    set_immediate_parameters(wah, low, resonance, high);

    depth_delta = (wah->target_depth - wah->current_depth) *
                  AUTO_WAH_SMOOTHING_STEP;
    phase_step_delta = (wah->target_phase_step - wah->current_phase_step) *
                       AUTO_WAH_SMOOTHING_STEP;
    output_level_delta =
        (wah->target_output_level - wah->current_output_level) *
        AUTO_WAH_SMOOTHING_STEP;

    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float tangent;
        float tangent_squared;
        float denominator;
        float wet;
        float output;

        if (!enabled) {
            continue;
        }
        wah->current_depth += depth_delta;
        wah->current_phase_step += phase_step_delta;
        wah->current_output_level += output_level_delta;
        wah->phase += wah->phase_direction * wah->current_phase_step;
        if (wah->phase >= 1.0f) {
            wah->phase_direction = -1.0f;
            wah->phase = 1.0f;
        } else if (wah->phase < 0.0f) {
            wah->phase_direction = 1.0f;
            wah->phase = 0.0f;
        }

        wah->cutoff_hz = wah->low_hz +
            (wah->high_hz - wah->low_hz) * wah->phase * wah->target_depth;
        tangent = tanf(wah->cutoff_hz * 3.1415927f /
                       AUTO_WAH_SAMPLE_RATE);
        tangent_squared = tangent * tangent;
        denominator = tangent + tangent_squared * wah->resonance +
                      wah->resonance;
        wah->filter_coefficients.b0 = tangent / denominator;
        wah->filter_coefficients.b1 = 0.0f;
        wah->filter_coefficients.b2 = -wah->filter_coefficients.b0;
        wah->filter_coefficients.a0 = 1.0f;
        wah->filter_coefficients.a1 =
            wah->resonance * 2.0f * (tangent_squared - 1.0f) /
            denominator;
        wah->filter_coefficients.a2 =
            (tangent_squared * wah->resonance - tangent +
             wah->resonance) / denominator;

        input = interleaved_stereo[frame * 2u];
        wet = process_filter(wah, input);
        output = wet * 5.0f * wah->current_output_level;
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
