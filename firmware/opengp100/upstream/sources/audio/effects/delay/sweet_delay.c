#include "sweet_delay.h"

#include "audio/dsp/decimation_internal.h"
#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define SWEET_DELAY_CONTROL_GLIDE 0x1.db8cdap-13f

static bool parameters_valid(uint8_t mix,
                             float time,
                             uint8_t feedback,
                             bool tempo_sync,
                             bool trail,
                             uint16_t tempo_period_ms)
{
    bool time_valid = isfinite(time) &&
        (tempo_sync ? time >= 0.0f && time <= 10.0f
                    : time >= 20.0f && time <= 4000.0f);

    (void)trail;
    return mix <= 99u && feedback <= 99u && time_valid &&
           (!tempo_sync || tempo_period_ms != 0u);
}

static float feedback_gain(uint8_t feedback)
{
    static const float gains[100] = {
        0.0f, 0.0f, 0.00416000001f, 0.0069600001f,
        0.0112000003f, 0.0140000001f, 0.0181600004f, 0.0209600002f,
        0.0252000004f, 0.0280000001f, 0.0322400004f, 0.0363999993f,
        0.039199999f, 0.0434399992f, 0.0462399989f, 0.0504800022f,
        0.0532800019f, 0.0574400008f, 0.0602400005f, 0.0644800007f,
        0.0686399996f, 0.0715200007f, 0.0756799996f, 0.0784799993f,
        0.0827199996f, 0.0855199993f, 0.0897599995f, 0.0925599992f,
        0.0967199981f, 0.0995199978f, 0.103760004f, 0.108000004f,
        0.110800004f, 0.115040004f, 0.117840004f, 0.122080004f,
        0.124880004f, 0.129120004f, 0.131920004f, 0.136240005f,
        0.140480006f, 0.143280005f, 0.147599995f, 0.150399995f,
        0.154719996f, 0.157599998f, 0.161919999f, 0.1648f,
        0.169200003f, 0.172080004f, 0.176479995f, 0.18096f,
        0.183920002f, 0.188399994f, 0.191439998f, 0.196000004f,
        0.199039996f, 0.203679991f, 0.206800008f, 0.211599994f,
        0.216400003f, 0.21960001f, 0.224559999f, 0.227920008f,
        0.233039999f, 0.236479998f, 0.241840005f, 0.245359993f,
        0.250880003f, 0.254640007f, 0.260400009f, 0.26631999f,
        0.2704f, 0.276559997f, 0.280879998f, 0.28743999f,
        0.292000008f, 0.298959994f, 0.303760004f, 0.311199999f,
        0.318959999f, 0.324320006f, 0.332640004f, 0.338400006f,
        0.347359991f, 0.353600001f, 0.36328001f, 0.370000005f,
        0.380480003f, 0.387759995f, 0.39920001f, 0.411199999f,
        0.419519997f, 0.43263998f, 0.441680002f, 0.455999994f,
        0.465920019f, 0.481519985f, 0.492399979f, 0.509520006f
    };

    return gains[feedback];
}

static float biquad(gp100_sweet_delay_biquad_t *state,
                    float input,
                    float b0,
                    float b1,
                    float b2,
                    float a1,
                    float a2)
{
    float output = input * b0 + state->previous_input[0] * b1 +
                   state->previous_input[1] * b2 -
                   state->previous_output[0] * a1 -
                   state->previous_output[1] * a2;

    state->previous_input[1] = state->previous_input[0];
    state->previous_input[0] = input;
    state->previous_output[1] = state->previous_output[0];
    state->previous_output[0] = output;
    return output;
}

static float limit(float sample)
{
    if (sample > 1.7f) {
        return 1.7f;
    }
    return sample < -1.7f ? -1.7f : sample;
}

static float third_order(gp100_sweet_delay_third_order_t *state,
                         float input,
                         const float b[4],
                         const float a[3])
{
    float output = input * b[0] + state->previous_input[0] * b[1] +
                   state->previous_input[1] * b[2] +
                   state->previous_input[2] * b[3] -
                   state->previous_output[0] * a[0] -
                   state->previous_output[1] * a[1] -
                   state->previous_output[2] * a[2];

    state->previous_input[2] = state->previous_input[1];
    state->previous_input[1] = state->previous_input[0];
    state->previous_input[0] = input;
    state->previous_output[2] = state->previous_output[1];
    state->previous_output[1] = state->previous_output[0];
    state->previous_output[0] = output;
    return output;
}

static float shape(float input)
{
    float magnitude = fabsf(input);
    float squared = magnitude * magnitude;
    float cubic = squared * magnitude;
    float numerator = magnitude * 2.024f * magnitude * magnitude -
                      squared * 3.141f + magnitude * 3.104f;
    float denominator = cubic - squared * 0.4667f -
                        magnitude * 1.141f + 2.549f;

    return input < 0.0f ? -numerator / denominator : numerator / denominator;
}

static float shape_and_decimate(gp100_sweet_delay_t *delay, float sample)
{
    const float previous = delay->shaper_previous_sample;
    const float delta = (sample - previous) / 3.0f;
    const float shaped[4] = {
        shape(previous),
        shape(previous + delta),
        shape(previous + delta * 2.0f),
        shape(previous + delta * 3.0f)
    };

    delay->shaper_previous_sample = sample;
    return gp100_decimate_four(delay->shaper_history, shaped);
}

static float process_wet(gp100_sweet_delay_t *delay, float input)
{
    static const float write_b[] = {
        0x1.5ea1f4p-8f, 0x1.06f978p-6f, 0x1.06f978p-6f, 0x1.5ea1f4p-8f
    };
    static const float write_a[] = {
        -0x1.1d8a86p+1f, 0x1.b6de48p+0f, -0x1.c350aep-2f
    };
    static const float reconstruction_b[] = {
        0x1.253508p-7f, 0x1.b7cf8cp-6f, 0x1.b7cf8cp-6f, 0x1.253508p-7f
    };
    static const float reconstruction_a[] = {
        -0x1.c22606p+0f, 0x1.a2ce1cp-1f, 0x1.89248ap-7f
    };
    const float length = (delay->current_delay_samples - 882.0f) / 175518.0f;
    float shaped;
    float tone;
    float wet;

    gp100_delay_write(delay->delay_line, GP100_SWEET_DELAY_SAMPLES,
                           &delay->write_index, &delay->delay_full,
                           third_order(&delay->write_filter, limit(input), write_b, write_a));
    wet = gp100_delay_read_linear_valid(
        delay->delay_line, GP100_SWEET_DELAY_SAMPLES,
        delay->write_index, delay->current_delay_samples, delay->delay_full);
    shaped = shape_and_decimate(delay, wet);
    tone = ((shaped * 0x1.6251b8p-4f +
             delay->tone_previous_input * 0x1.6251b8p-4f) -
            delay->tone_previous_output * (-length - 1.0563534f)) /
           (length + 1.2293608f);
    delay->tone_previous_input = shaped;
    delay->tone_previous_output = tone;
    wet = third_order(&delay->reconstruction_filter, tone,
                      reconstruction_b, reconstruction_a);
    return limit(biquad(&delay->wet_filter, wet,
                        0x1.4dbca4p-4f, 0x1.4dbca4p-3f, 0x1.4dbca4p-4f,
                        -0x1.77f31cp+0f, 0x1.96c48cp-1f));
}

static float process_sample(gp100_sweet_delay_t *delay,
                            float input,
                            bool enabled)
{
    float conditioned = biquad(&delay->input_filter, input,
                               -0x1.c7022ap+1f, -0x1.9dce0cp-3f,
                               0x1.ad254ap+1f, -0x1.3381fap-2f,
                               -0x1.2eaff8p-2f);
    float write_value;
    float mix;

    delay->feedback_previous_output =
        delay->last_wet * 0x1.f68124p-1f +
        delay->feedback_previous_input * -0x1.f68124p-1f -
        delay->feedback_previous_output * -0x1.ed0248p-1f;
    delay->feedback_previous_input = delay->last_wet;
    write_value = delay->feedback_previous_output * delay->current_feedback;
    if (enabled) {
        write_value += conditioned;
    }
    delay->last_wet = process_wet(delay, write_value);
    delay->ring_active = true;
    if (delay->current_mix < 0.5f) {
        mix = conditioned + delay->last_wet * delay->current_mix * 2.0f;
    } else if (!enabled) {
        mix = conditioned + delay->last_wet;
    } else {
        mix = delay->last_wet + conditioned * 2.0f * (1.0f - delay->current_mix);
    }
    return biquad(&delay->output_filter, mix,
                  -0x1.784574p-3f, -0x1.ae2bfep-5f,
                  0x1.0cba74p-3f, -0x1.3fab9ep-4f,
                  -0x1.a2450cp-1f);
}

bool gp100_sweet_delay_reset(gp100_sweet_delay_t *delay,
                                  uint8_t mix,
                                  float time,
                                  uint8_t feedback,
                                  bool tempo_sync,
                                  bool trail)
{
    if (delay == NULL ||
        !parameters_valid(mix, time, feedback, tempo_sync, trail, 1u)) {
        return false;
    }
    memset(delay, 0, offsetof(gp100_sweet_delay_t, delay_line));
    delay->target_mix = (float)mix * 0.01f;
    delay->current_mix = delay->target_mix;
    delay->target_feedback = feedback_gain(feedback);
    delay->current_feedback = delay->target_feedback;
    delay->tempo_sync = tempo_sync;
    delay->trail = trail;
    delay->configured = true;
    return true;
}

bool gp100_sweet_delay_process(gp100_sweet_delay_t *delay,
                                    uint8_t mix,
                                    float time,
                                    uint8_t feedback,
                                    bool tempo_sync,
                                    bool trail,
                                    uint16_t tempo_period_ms,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    size_t frame;

    if (delay == NULL || !delay->configured ||
        !parameters_valid(mix, time, feedback, tempo_sync, trail,
                          tempo_period_ms) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    delay->target_mix = (float)mix * 0.01f;
    delay->target_feedback = feedback_gain(feedback);
    delay->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, tempo_sync, tempo_period_ms);
    if (delay->current_delay_samples == 0.0f || tempo_sync) {
        delay->current_delay_samples = delay->target_delay_samples;
    }
    delay->tempo_sync = tempo_sync;
    delay->trail = trail;
    mix_delta = (delay->target_mix - delay->current_mix) *
                SWEET_DELAY_CONTROL_GLIDE;
    feedback_delta = (delay->target_feedback - delay->current_feedback) *
                     SWEET_DELAY_CONTROL_GLIDE;
    time_delta = (delay->target_delay_samples - delay->current_delay_samples) *
                 SWEET_DELAY_CONTROL_GLIDE;

    for (frame = 0u; frame < frame_count; ++frame) {
        float output;

        delay->current_mix += mix_delta;
        delay->current_feedback += feedback_delta;
        if (!tempo_sync) {
            delay->current_delay_samples += time_delta;
        }
        if (!enabled && !trail) {
            if (delay->ring_active) {
                delay->delay_full = false;
                delay->write_index = 0u;
                delay->last_wet = 0.0f;
                delay->ring_active = false;
            }
            continue;
        }
        output = process_sample(delay, interleaved_stereo[frame * 2u], enabled);
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
