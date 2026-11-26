#include "room_reverb.h"

#include "audio/dsp/delay_line_internal.h"
#include "audio/dsp/pitch_window_internal.h"
#include "reverb_fdn_internal.h"

#include <math.h>
#include <string.h>

#define ROOM_CONTROL_GLIDE (10.0f / 44100.0f)
#define ROOM_LFO_STEP (2048.0f / 44100.0f)
#define ROOM_LFO_SCALE (6.2831853071795864769f / 2048.0f)

typedef struct room_biquad_coefficients {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} room_biquad_coefficients_t;

static const gp100_reverb_fdn_profile_t room_profile = {
    {1369.0f, 1849.0f, 3125.0f, 2809.0f},
    0.75f, 0.0f,
    -0.05f, -0.05f,
    0.125450000166893f, 0.0232948027551174f,
    0.7123792767524719f,
    1.0f,
    0.015f, 1.0f,
    false, false, false, false, false, false, false
};

/* Reachable entries from the shared factory decay table, in ten-thousandths. */
static const uint16_t decay_taper[100] = {
    0, 0, 1093, 1411, 1785, 1996, 2275, 2444, 2677, 2822,
    3027, 3218, 3340, 3514, 3625, 3787, 3890, 4041, 4139, 4281,
    4418, 4507, 4638, 4723, 4848, 4929, 5049, 5127, 5243, 5318,
    5429, 5538, 5610, 5715, 5785, 5887, 5954, 6054, 6119, 6216,
    6311, 6374, 6467, 6529, 6620, 6679, 6768, 6827, 6914, 6971,
    7057, 7141, 7196, 7279, 7333, 7414, 7468, 7548, 7600, 7678,
    7756, 7807, 7883, 7934, 8009, 8058, 8132, 8181, 8253, 8302,
    8373, 8444, 8491, 8561, 8608, 8677, 8723, 8791, 8836, 8904,
    8971, 9015, 9081, 9125, 9190, 9233, 9298, 9340, 9404, 9446,
    9509, 9572, 9614, 9676, 9717, 9778, 9819, 9879, 9920, 9980};

static bool parameters_valid(uint8_t mix, uint8_t predelay, uint8_t decay)
{
    return mix <= 99u && predelay <= 100u && decay <= 99u;
}

static float decay_gain(uint8_t decay,
                        const gp100_reverb_fdn_profile_t *profile)
{
    const float taper = (float)decay_taper[decay] / 10000.0f;

    return taper * profile->decay_scale + profile->decay_offset;
}

static room_biquad_coefficients_t diffusion_coefficients(
    const gp100_reverb_fdn_profile_t *profile,
    float damping,
    float lo_end,
    float hi_end)
{
    const float shelf_a = powf(10.0f,
        profile->dynamic_shelf_filter
            ? -0.3f + hi_end * 0.003f
            : (profile->dynamic_damping
            ? (-0.5f - damping * 5.0f) / 20.0f
            : (profile->dynamic_tone
                ? -(1.0f - damping) * 3.0f / 20.0f
                : profile->shelf_a_exponent)));
    const float shelf_b = powf(10.0f,
        profile->dynamic_shelf_filter
            ? (profile->shimmer_input ? -0.3f : -0.125f) +
              lo_end * 0.003f
            : profile->shelf_b_exponent);
    const float low_warp = profile->dynamic_damping
        ? profile->low_warp_angle : tanf(profile->low_warp_angle);
    const float high_warp = tanf(profile->high_warp_angle);
    const float p0 = 2.0f / (shelf_a + 1.0f);
    const float p1 = 2.0f / (1.0f / shelf_a + 1.0f);
    const float q0 = 2.0f / (shelf_b + 1.0f);
    const float q1 = 2.0f / (1.0f / shelf_b + 1.0f);
    const float denominator = (low_warp + p0) *
                              (high_warp * q0 + 1.0f);
    room_biquad_coefficients_t coefficients;

    if (profile->dynamic_damping) {
        const float dynamic_denominator =
            (low_warp + p0) * 1.0510042f;

        coefficients.b0 = (low_warp + p1) * -0.9484042f /
                          dynamic_denominator;
        coefficients.b1 = ((low_warp + p1) * 1.0515958f +
                            (low_warp - p1) * -0.9484042f) /
                           dynamic_denominator;
        coefficients.b2 = (low_warp - p1) * 1.0515958f /
                          dynamic_denominator;
        coefficients.a1 = ((low_warp + p0) * -0.9489958f +
                            (low_warp - p0) * 1.0510042f) /
                           dynamic_denominator;
        coefficients.a2 = (low_warp - p0) * -0.9489958f /
                          dynamic_denominator;
        return coefficients;
    }

    coefficients.b0 = ((low_warp + p1) *
                       (high_warp * q1 - 1.0f)) / denominator;
    coefficients.b1 = ((low_warp + p1) *
                        (high_warp * q1 + 1.0f) +
                        (low_warp - p1) *
                        (high_warp * q1 - 1.0f)) / denominator;
    coefficients.b2 = ((low_warp - p1) *
                       (high_warp * q1 + 1.0f)) / denominator;
    coefficients.a1 = ((low_warp + p0) *
                        (high_warp * q0 - 1.0f) +
                        (low_warp - p0) *
                        (high_warp * q0 + 1.0f)) / denominator;
    coefficients.a2 = ((low_warp - p0) *
                       (high_warp * q0 - 1.0f)) / denominator;
    return coefficients;
}

static room_biquad_coefficients_t output_coefficients(
    const gp100_reverb_fdn_profile_t *profile)
{
    const float angle = profile->output_angle;
    const float sine = sinf(angle);
    const float cosine = cosf(angle);
    const float a0 = 1.0f + sine / 1.414f;
    room_biquad_coefficients_t coefficients;

    coefficients.b0 = (1.0f - cosine) * 0.5f / a0;
    coefficients.b1 = (1.0f - cosine) / a0;
    coefficients.b2 = coefficients.b0;
    coefficients.a1 = -2.0f * cosine / a0;
    coefficients.a2 = (1.0f - sine / 1.414f) / a0;
    return coefficients;
}

static float biquad_process(gp100_room_biquad_state_t *state,
                            room_biquad_coefficients_t coefficients,
                            float input)
{
    float output = input * coefficients.b0 + state->x1 * coefficients.b1 +
                   state->x2 * coefficients.b2 -
                   state->y1 * coefficients.a1 -
                   state->y2 * coefficients.a2;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float fdn_delay(gp100_room_reverb_t *reverb,
                       size_t line,
                       const gp100_reverb_fdn_profile_t *profile,
                       float input)
{
    if (input > 1.7f) {
        input = 1.7f;
    } else if (input < -1.7f) {
        input = -1.7f;
    }
    reverb->fdn_line[line][reverb->fdn_write[line]] = input;
    reverb->fdn_write[line]++;
    if (reverb->fdn_write[line] == GP100_ROOM_FDN_LINE_SAMPLES) {
        reverb->fdn_write[line] = 0u;
    }
    return gp100_delay_read_linear(
        reverb->fdn_line[line], GP100_ROOM_FDN_LINE_SAMPLES,
        reverb->fdn_write[line], profile->delay_samples[line]);
}

static float predelay_process(
    gp100_room_reverb_t *reverb,
    const gp100_reverb_fdn_profile_t *profile,
    float input)
{
    float lfo = sinf(reverb->lfo_phase * ROOM_LFO_SCALE);
    float delay = reverb->current_predelay * 4410.0f +
                  (1.0f - lfo * profile->predelay_mod_depth) * 2205.0f;
    float delayed;

    reverb->predelay[reverb->predelay_write] = input;
    reverb->predelay_write++;
    if (reverb->predelay_write == GP100_ROOM_PREDELAY_SAMPLES) {
        reverb->predelay_write = 0u;
    }
    delayed = gp100_delay_read_linear(
        reverb->predelay, GP100_ROOM_PREDELAY_SAMPLES,
        reverb->predelay_write, delay);
    return (input + delayed) * 0.5f;
}

static float shimmer_pitch_process(gp100_room_reverb_t *reverb,
                                   size_t channel,
                                   float input)
{
    float shifted = 0.0f;
    size_t grain;

    reverb->pitch_buffer[channel][reverb->pitch_write[channel]] = input;
    reverb->pitch_write[channel]++;
    if (reverb->pitch_write[channel] == GP100_SHIMMER_PITCH_SAMPLES) {
        reverb->pitch_write[channel] = 0u;
    }
    for (grain = 0u; grain < 2u; ++grain) {
        uint32_t phase = reverb->pitch_phase[channel][grain];
        size_t offset;
        size_t current;
        size_t previous;
        float fraction;
        float sample;
        float window;

        phase = (phase + GP100_PITCH_PHASE_LIMIT - 256u) %
                GP100_PITCH_PHASE_LIMIT;
        reverb->pitch_phase[channel][grain] = phase;
        offset = (size_t)(phase >> 8u);
        current = (reverb->pitch_write[channel] +
                   GP100_SHIMMER_PITCH_SAMPLES - offset) %
                  GP100_SHIMMER_PITCH_SAMPLES;
        previous = current == 0u ? GP100_SHIMMER_PITCH_SAMPLES - 1u
                                 : current - 1u;
        fraction = (float)(phase & 0xffu) / 256.0f;
        sample = reverb->pitch_buffer[channel][current] *
                 (1.0f - fraction) +
                 reverb->pitch_buffer[channel][previous] * fraction;
        window = gp100_pitch_window_value(reverb->pitch_window, phase);
        shifted += sample * window;
    }
    shifted = shifted * 1.5f / 32768.0f;
    input = shifted * 0.5347844362258911f +
            reverb->pitch_previous_input[channel] * 0.5347844362258911f -
            reverb->pitch_previous_output[channel] * 0.06956888735294342f;
    reverb->pitch_previous_input[channel] = shifted;
    reverb->pitch_previous_output[channel] = input;
    return input;
}

static float mix_sample(float dry, float wet, float mix,
                        bool enabled, bool trail)
{
    if (mix < 0.5f) {
        return dry + wet * mix * 2.0f;
    }
    if (!enabled && trail) {
        return dry + wet;
    }
    return wet + dry * 2.0f * (1.0f - mix);
}

static void clear_network(gp100_room_reverb_t *reverb, bool pitch_shift)
{
    memset(reverb->predelay, 0, sizeof(reverb->predelay));
    memset(reverb->fdn_line, 0, sizeof(reverb->fdn_line));
    memset(reverb->diffusion_filter, 0, sizeof(reverb->diffusion_filter));
    memset(reverb->output_filter, 0, sizeof(reverb->output_filter));
    memset(reverb->feedback_state, 0, sizeof(reverb->feedback_state));
    memset(reverb->fdn_write, 0, sizeof(reverb->fdn_write));
    reverb->predelay_write = 0u;
    reverb->lfo_phase = 0.0f;
    reverb->history_written = false;
    if (pitch_shift) {
        memset(reverb->pitch_buffer, 0, sizeof(reverb->pitch_buffer));
        memset(reverb->pitch_previous_input, 0, sizeof(reverb->pitch_previous_input));
        memset(reverb->pitch_previous_output, 0, sizeof(reverb->pitch_previous_output));
        memset(reverb->pitch_write, 0, sizeof(reverb->pitch_write));
        reverb->pitch_phase[0][0] = 0u;
        reverb->pitch_phase[1][0] = 0u;
        reverb->pitch_phase[0][1] = GP100_PITCH_PHASE_LIMIT / 2u;
        reverb->pitch_phase[1][1] = GP100_PITCH_PHASE_LIMIT / 2u;
    }
}

bool gp100_reverb_fdn_reset(
    gp100_room_reverb_t *reverb,
    const gp100_reverb_fdn_profile_t *profile,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    uint8_t damping,
    int8_t lo_end,
    int8_t hi_end,
    bool trail)
{
    if (reverb == NULL || !parameters_valid(mix, predelay, decay) ||
        ((profile->dynamic_damping || profile->dynamic_tone) &&
         damping > 99u) ||
        (profile->dynamic_shelf_filter &&
         (lo_end < -50 || lo_end > 50 ||
          hi_end < -50 || hi_end > 50))) {
        return false;
    }
    memset(reverb, 0, sizeof(*reverb));
    reverb->target_mix = reverb->current_mix = (float)mix * 0.01f;
    reverb->target_predelay = reverb->current_predelay =
        (float)predelay * 0.01f;
    reverb->decay = decay_gain(decay, profile);
    reverb->damping = (float)damping * 0.01f;
    reverb->lo_end = (float)lo_end;
    reverb->hi_end = (float)hi_end;
    if (profile->shimmer_input) {
        reverb->pitch_phase[0][1] = GP100_PITCH_PHASE_LIMIT / 2u;
        reverb->pitch_phase[1][1] = GP100_PITCH_PHASE_LIMIT / 2u;
        gp100_pitch_build_window(reverb->pitch_window);
    }
    reverb->trail = trail;
    reverb->bypass_cleared = profile->shimmer_input;
    reverb->configured = true;
    return true;
}

bool gp100_reverb_fdn_process(
    gp100_room_reverb_t *reverb,
    const gp100_reverb_fdn_profile_t *profile,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    uint8_t damping,
    int8_t lo_end,
    int8_t hi_end,
    bool trail,
    float *audio,
    size_t frames,
    bool enabled)
{
    room_biquad_coefficients_t diffusion;
    const room_biquad_coefficients_t output = output_coefficients(profile);
    float mix_delta;
    float predelay_delta;
    float feedback;
    size_t frame;

    if (reverb == NULL || !reverb->configured ||
        !parameters_valid(mix, predelay, decay) ||
        ((profile->dynamic_damping || profile->dynamic_tone) &&
         damping > 99u) ||
        (profile->dynamic_shelf_filter &&
         (lo_end < -50 || lo_end > 50 ||
          hi_end < -50 || hi_end > 50)) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    reverb->target_mix = (float)mix * 0.01f;
    reverb->target_predelay = (float)predelay * 0.01f;
    reverb->decay = decay_gain(decay, profile);
    reverb->damping = (float)damping * 0.01f;
    reverb->lo_end = (float)lo_end;
    reverb->hi_end = (float)hi_end;
    diffusion = diffusion_coefficients(profile, reverb->damping,
                                         reverb->lo_end, reverb->hi_end);
    reverb->trail = trail;
    mix_delta = (reverb->target_mix - reverb->current_mix) *
                ROOM_CONTROL_GLIDE;
    predelay_delta = (reverb->target_predelay -
                      reverb->current_predelay) * ROOM_CONTROL_GLIDE;
    feedback = reverb->decay * (profile->full_matrix ? 0.5f : 0.7071f);
    if (frames != 0u && (enabled || trail)) {
        reverb->history_written = true;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float dry_left = audio[frame * 2u];
        float dry_right = audio[frame * 2u + 1u];
        float injection = 0.0f;
        float delayed[4];
        float filtered[4];
        float wet_left;
        float wet_right;

        reverb->current_mix += mix_delta;
        reverb->current_predelay += predelay_delta;
        reverb->lfo_phase += profile->predelay_mod_rate * ROOM_LFO_STEP;
        if (reverb->lfo_phase >= 2048.0f) {
            reverb->lfo_phase -= 2048.0f;
        }
        if (!enabled && !trail) {
            if (!reverb->bypass_cleared) {
                if (reverb->history_written) {
                    clear_network(reverb, profile->shimmer_input);
                }
                /* Preserve the phase reset even when the histories are fresh. */
                reverb->lfo_phase = 0.0f;
                reverb->bypass_cleared = true;
            }
            continue;
        }
        reverb->bypass_cleared = false;
        if (enabled) {
            float input = (dry_left + dry_right) * 0.5f;

            if (profile->shimmer_input) {
                const float shifted_left = shimmer_pitch_process(
                    reverb, 0u, dry_left + reverb->diffusion_filter[0].y1);
                const float shifted_right = shimmer_pitch_process(
                    reverb, 1u, dry_right + reverb->diffusion_filter[1].y1);

                input = ((dry_left * 0.6f + shifted_left * 0.4f) +
                         (dry_right * 0.6f + shifted_right * 0.4f)) * 0.5f;
            }
            injection = predelay_process(reverb, profile,
                                         input) * 0.3f;
        }
        if (profile->full_matrix) {
            const float s0 = reverb->feedback_state[0];
            const float s1 = reverb->feedback_state[1];
            const float s2 = reverb->feedback_state[2];
            const float s3 = reverb->feedback_state[3];

            delayed[0] = fdn_delay(reverb, 0u, profile,
                                   feedback * (s0 + s1 + s2 + s3) +
                                   injection);
            delayed[1] = fdn_delay(reverb, 1u, profile,
                                   feedback * (s0 - s1 + s2 - s3) +
                                   injection);
            delayed[2] = fdn_delay(reverb, 2u, profile,
                                   feedback * (s0 + s1 - s2 - s3) +
                                   injection);
            delayed[3] = fdn_delay(reverb, 3u, profile,
                                   feedback * (s0 - s1 - s2 + s3) +
                                   injection);
        } else {
            delayed[0] = fdn_delay(reverb, 0u, profile,
                                   feedback * (reverb->feedback_state[1] +
                                               reverb->feedback_state[2]) +
                                   injection);
            delayed[1] = fdn_delay(reverb, 1u, profile,
                                   -feedback * (reverb->feedback_state[0] +
                                                reverb->feedback_state[3]) +
                                   injection);
            delayed[2] = fdn_delay(reverb, 2u, profile,
                                   feedback * (reverb->feedback_state[0] -
                                               reverb->feedback_state[3]) +
                                   injection);
            delayed[3] = fdn_delay(reverb, 3u, profile,
                                   feedback * (reverb->feedback_state[1] -
                                               reverb->feedback_state[2]) +
                                   injection);
        }
        if (profile->filter_output_sum) {
            memcpy(reverb->feedback_state, delayed, sizeof(delayed));
            wet_left = biquad_process(&reverb->diffusion_filter[0],
                                      diffusion, delayed[0] + delayed[2]);
            wet_right = biquad_process(&reverb->diffusion_filter[1],
                                       diffusion,
                                       delayed[1] + delayed[3]);
            wet_left *= profile->left_output_gain;
        } else {
            filtered[0] = biquad_process(&reverb->diffusion_filter[0],
                                         diffusion, delayed[0]);
            filtered[1] = biquad_process(&reverb->diffusion_filter[1],
                                         diffusion, delayed[1]);
            filtered[2] = biquad_process(&reverb->diffusion_filter[2],
                                         diffusion, delayed[2]);
            filtered[3] = biquad_process(&reverb->diffusion_filter[3],
                                         diffusion, delayed[3]);
            memcpy(reverb->feedback_state, filtered, sizeof(filtered));
            wet_left = (filtered[0] + filtered[2]) *
                       profile->left_output_gain;
            wet_right = filtered[1] + filtered[3];
        }
        if (!profile->skip_output_filter) {
            wet_left = biquad_process(&reverb->output_filter[0], output,
                                      wet_left);
            wet_right = biquad_process(&reverb->output_filter[1], output,
                                       wet_right);
        }
        audio[frame * 2u] = mix_sample(dry_left, wet_left,
                                       reverb->current_mix, enabled, trail);
        audio[frame * 2u + 1u] = mix_sample(dry_right, wet_right,
                                            reverb->current_mix,
                                            enabled, trail);
    }
    return true;
}

bool gp100_room_reverb_reset(gp100_room_reverb_t *reverb,
                                  uint8_t mix,
                                  uint8_t predelay,
                                  uint8_t decay,
                                  bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &room_profile, mix,
                                       predelay, decay, 0u, 0, 0, trail);
}

bool gp100_room_reverb_process(gp100_room_reverb_t *reverb,
                                    uint8_t mix,
                                    uint8_t predelay,
                                    uint8_t decay,
                                    bool trail,
                                    float *audio,
                                    size_t frames,
                                    bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &room_profile, mix,
                                         predelay, decay, 0u, 0, 0, trail,
                                         audio, frames, enabled);
}
