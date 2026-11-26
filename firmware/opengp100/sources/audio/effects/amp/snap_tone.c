#include "snap_tone.h"

#include <math.h>
#include <string.h>

/* Active lattice coefficients in signal order. The first collapse stage
 * starts six entries into the factory's collapse coefficient bank. */
static const float expand_coefficients[GP100_SNAP_TONE_EXPAND_ORDER] = {
    0x1.769aep-5f, 0x1.547b2cp-2f, 0x1.538f38p-1f, 0x1.de225ap-1f,
    0x1.583e48p-3f, 0x1.024bf4p-1f, 0x1.9b892ap-1f,
    0x1.bc423p-5f, 0x1.985e3cp-2f, 0x1.b9d05ep-1f, 0x1.98fc18p-3f, 0x1.3e0068p-1f
};

static const float collapse_coefficients[GP100_SNAP_TONE_COLLAPSE_ORDER] = {
    0x1.21db7ap-4f, 0x1.06bde6p-1f, 0x1.080aa4p-2f, 0x1.a2776cp-1f,
    0x1.bc2664p-5f, 0x1.88480cp-2f, 0x1.7f585ap-1f, 0x1.930adp-3f,
    0x1.257222p-1f, 0x1.d41e4ep-1f
};

static uint32_t load_u32_le(const uint8_t input[4])
{
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8u) |
           ((uint32_t)input[2] << 16u) |
           ((uint32_t)input[3] << 24u);
}

static float load_f32_le(const uint8_t input[4])
{
    uint32_t bits = load_u32_le(input);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float process_biquad(const double coefficient[5],
                            double state[2],
                            float input)
{
    double next = (double)input * 1000.0 -
        coefficient[3] * state[0] - coefficient[4] * state[1];
    double output = coefficient[0] * next +
        coefficient[1] * state[0] + coefficient[2] * state[1];

    state[1] = state[0];
    state[0] = next;
    return (float)(output * 0.001);
}

static float process_waveshaper(
    const gp100_user_amp_model_layout_t *model,
    float input)
{
    if (input <= 0.0f) {
        return -model->negative_limit *
            (1.0f - expf(input * model->negative_rate));
    }
    return model->positive_limit *
        (1.0f - expf(-model->positive_rate * input));
}

bool gp100_snap_tone_control_targets(
    const float parameters[5],
    gp100_snap_tone_targets_t *targets)
{
    size_t index;

    if (parameters == NULL || targets == NULL) {
        return false;
    }
    for (index = 0u; index < 5u; ++index) {
        if (!isfinite(parameters[index])) {
            return false;
        }
    }
    targets->input_gain = powf(
        10.0f, (parameters[0] * 0.48f - 24.0f) * 0.05f);
    targets->output_gain = powf(
        10.0f, (parameters[1] * 0.48f - 24.0f) * 0.05f);
    for (index = 0u; index < 3u; ++index) {
        targets->tone_db[index] = parameters[index + 2u] * 0.12f - 6.0f;
    }
    return true;
}

static void peaking_coefficients(float frequency_hz,
                                 float gain_db,
                                 gp100_biquad_coefficients_t *output)
{
    const float angle = frequency_hz * (6.2831853071795864769f / 44100.0f);
    const float alpha = sinf(angle) / (2.0f * 0.707000017166f);
    const float amplitude = powf(10.0f, gain_db * 0.025f);
    const float scale = 1.0f / (1.0f + alpha / amplitude);
    const float shared = -2.0f * cosf(angle) * scale;

    output->b0 = (1.0f + alpha * amplitude) * scale;
    output->b1 = shared;
    output->b2 = (1.0f - alpha * amplitude) * scale;
    output->a0 = 1.0f;
    output->a1 = shared;
    output->a2 = (1.0f - alpha / amplitude) * scale;
}

static void high_shelf_coefficients(float frequency_hz,
                                    float gain_db,
                                    gp100_biquad_coefficients_t *output)
{
    const float angle = frequency_hz * (6.2831853071795864769f / 44100.0f);
    const float amplitude = powf(10.0f, gain_db * 0.025f);
    const float cosine = cosf(angle);
    const float alpha = sqrtf(amplitude) * sinf(angle) / 0.707000017166f;
    const float sum = amplitude + 1.0f;
    const float difference = amplitude - 1.0f;
    const float minus = sum - difference * cosine;
    const float plus = sum + difference * cosine;
    const float scale = 1.0f / (minus + alpha);

    output->b0 = amplitude * (plus + alpha) * scale;
    output->b1 = -2.0f * amplitude *
        (difference + sum * cosine) * scale;
    output->b2 = amplitude * (plus - alpha) * scale;
    output->a0 = 1.0f;
    output->a1 = 2.0f * (difference - sum * cosine) * scale;
    output->a2 = (minus - alpha) * scale;
}

bool gp100_snap_tone_tone_coefficients(
    const float tone_db[3],
    gp100_biquad_coefficients_t coefficients[3])
{
    size_t index;

    if (tone_db == NULL || coefficients == NULL) {
        return false;
    }
    for (index = 0u; index < 3u; ++index) {
        if (!isfinite(tone_db[index])) {
            return false;
        }
    }
    peaking_coefficients(125.0f, tone_db[0], &coefficients[0]);
    peaking_coefficients(800.0f, tone_db[1], &coefficients[1]);
    high_shelf_coefficients(1600.0f, tone_db[2], &coefficients[2]);
    return true;
}

static float lattice_branch(float input,
                            const float *coefficients,
                            float *state,
                            size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        float next = state[index] + coefficients[index] * input;
        state[index] = input - coefficients[index] * next;
        input = next;
    }
    return input;
}

bool gp100_snap_tone_lattice_expand(
    const float *input,
    size_t sample_count,
    const float *coefficients,
    float *state,
    size_t order,
    float *output_pairs)
{
    size_t first_count;
    size_t index;

    if ((sample_count != 0u &&
         (input == NULL || output_pairs == NULL || coefficients == NULL ||
          state == NULL)) ||
        sample_count > SIZE_MAX / 2u) {
        return false;
    }
    first_count = order - order / 2u;
    for (index = 0u; index < sample_count; ++index) {
        output_pairs[index * 2u] = lattice_branch(
            input[index], coefficients, state, first_count);
        output_pairs[index * 2u + 1u] = lattice_branch(
            input[index], coefficients + first_count,
            state + first_count, order - first_count);
    }
    return true;
}

bool gp100_snap_tone_lattice_collapse(
    const float *input_pairs,
    size_t sample_count,
    const float *coefficients,
    float *state,
    size_t order,
    float *second_branch_delay,
    float *output)
{
    size_t first_count;
    size_t index;

    if (second_branch_delay == NULL ||
        (sample_count != 0u &&
         (input_pairs == NULL || output == NULL || coefficients == NULL ||
          state == NULL)) ||
        sample_count > SIZE_MAX / 2u) {
        return false;
    }
    first_count = order - order / 2u;
    for (index = 0u; index < sample_count; ++index) {
        float first = lattice_branch(
            input_pairs[index * 2u], coefficients, state, first_count);
        float second = lattice_branch(
            input_pairs[index * 2u + 1u], coefficients + first_count,
            state + first_count, order - first_count);
        output[index] = (*second_branch_delay + first) * 0.5f;
        *second_branch_delay = second;
    }
    return true;
}

bool gp100_snap_tone_load(
    gp100_snap_tone_t *state,
    const uint8_t *blob,
    size_t blob_size)
{
    gp100_user_amp_model_layout_t layout;
    size_t index;
    size_t pre_offset;
    size_t post_offset;

    if (state == NULL || blob == NULL ||
        !gp100_user_amp_model_layout(blob, blob_size, &layout) ||
        layout.first_bank_value_count != GP100_SNAP_TONE_PRE_TAPS ||
        layout.second_bank_value_count != GP100_SNAP_TONE_POST_TAPS) {
        return false;
    }
    pre_offset = GP100_USER_AMP_MODEL_HEADER_BYTES +
        (size_t)layout.first_bank_offset_words * 4u;
    post_offset = GP100_USER_AMP_MODEL_HEADER_BYTES +
        (size_t)layout.second_bank_offset_words * 4u;
    memset(state, 0, sizeof(*state));
    state->model = layout;
    for (index = 0u; index < GP100_SNAP_TONE_PRE_TAPS; ++index) {
        state->pre_impulse[index] =
            load_f32_le(blob + pre_offset + index * 4u);
    }
    for (index = 0u; index < GP100_SNAP_TONE_POST_TAPS; ++index) {
        state->post_impulse[index] =
            load_f32_le(blob + post_offset + index * 4u);
    }
    if (!gp100_partitioned_convolution_initialize(
            &state->pre_convolution, state->pre_impulse,
            GP100_SNAP_TONE_PRE_TAPS, state->pre_impulse_spectra,
            sizeof(state->pre_impulse_spectra) /
                sizeof(state->pre_impulse_spectra[0]),
            state->pre_input_spectra,
            sizeof(state->pre_input_spectra) /
                sizeof(state->pre_input_spectra[0])) ||
        !gp100_partitioned_convolution_initialize(
            &state->post_convolution, state->post_impulse,
            GP100_SNAP_TONE_POST_TAPS, state->post_impulse_spectra,
            sizeof(state->post_impulse_spectra) /
                sizeof(state->post_impulse_spectra[0]),
            state->post_input_spectra,
            sizeof(state->post_input_spectra) /
                sizeof(state->post_input_spectra[0]))) {
        return false;
    }
    state->loaded = true;
    state->configured = true;
    return true;
}

bool gp100_snap_tone_reset_empty(gp100_snap_tone_t *state)
{
    if (state == NULL) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->configured = true;
    return true;
}

bool gp100_snap_tone_process_mono(
    gp100_snap_tone_t *state,
    float *samples,
    size_t sample_count)
{
    size_t index;

    if (state == NULL || !state->loaded ||
        sample_count % GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES != 0u ||
        (sample_count != 0u && samples == NULL)) {
        return false;
    }
    for (index = 0u; index < sample_count; ++index) {
        samples[index] = process_biquad(
            state->model.input_biquad, state->input_biquad_state,
            samples[index]);
    }
    if (!gp100_partitioned_convolution_process(
            &state->pre_convolution, samples, sample_count)) {
        return false;
    }
    for (index = 0u; index < sample_count; ++index) {
        float expanded_twice[2];
        float expanded_four_times[4];
        float collapsed_twice[2];
        size_t expanded_index;
        float sample = samples[index];
        gp100_snap_tone_lattice_expand(
            &sample, 1u, expand_coefficients, state->expand_state,
            GP100_SNAP_TONE_EXPAND_FIRST_ORDER, expanded_twice);
        gp100_snap_tone_lattice_expand(
            expanded_twice, 2u,
            expand_coefficients + GP100_SNAP_TONE_EXPAND_FIRST_ORDER,
            state->expand_state + GP100_SNAP_TONE_EXPAND_FIRST_ORDER,
            GP100_SNAP_TONE_EXPAND_SECOND_ORDER,
            expanded_four_times);
        for (expanded_index = 0u; expanded_index < 4u; ++expanded_index) {
            expanded_four_times[expanded_index] = process_waveshaper(
                &state->model, expanded_four_times[expanded_index]);
        }
        gp100_snap_tone_lattice_collapse(
            expanded_four_times, 2u, collapse_coefficients,
            state->collapse_state, GP100_SNAP_TONE_COLLAPSE_FIRST_ORDER,
            &state->collapse_delay[0], collapsed_twice);
        gp100_snap_tone_lattice_collapse(
            collapsed_twice, 1u,
            collapse_coefficients +
                GP100_SNAP_TONE_COLLAPSE_FIRST_ORDER,
            state->collapse_state +
                GP100_SNAP_TONE_COLLAPSE_FIRST_ORDER,
            GP100_SNAP_TONE_COLLAPSE_SECOND_ORDER,
            &state->collapse_delay[1],
            &sample);
        sample = process_biquad(
            state->model.output_biquad, state->output_biquad_state, sample);
        samples[index] = sample;
    }
    return gp100_partitioned_convolution_process(
        &state->post_convolution, samples, sample_count);
}

static float move_toward(float current, float target, float step)
{
    if (current < target) {
        current += step;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= step;
        return current < target ? target : current;
    }
    return target;
}

static float process_tone_section(
    const gp100_biquad_coefficients_t *coefficient,
    float state[2],
    float input)
{
    float next = input - coefficient->a1 * state[0] -
        coefficient->a2 * state[1];
    float output = coefficient->b0 * next +
        coefficient->b1 * state[0] + coefficient->b2 * state[1];

    state[1] = state[0];
    state[0] = next;
    return output;
}

bool gp100_snap_tone_process_stereo(
    gp100_snap_tone_t *state,
    const float parameters[5],
    bool enabled,
    float *interleaved_stereo,
    size_t frame_count)
{
    gp100_snap_tone_targets_t target;
    float model_samples[GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES];
    size_t band;
    size_t block_start;

    if (state == NULL || !state->configured ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        frame_count % GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES != 0u ||
        !gp100_snap_tone_control_targets(parameters, &target)) {
        return false;
    }
    if (!state->wrapper_configured) {
        state->current_input_gain = target.input_gain;
        state->current_output_gain = target.output_gain;
        memcpy(state->current_tone_db, target.tone_db,
               sizeof(state->current_tone_db));
        state->current_wet = 1.0f;
        if (!gp100_snap_tone_tone_coefficients(
                state->current_tone_db, state->tone_coefficients)) {
            return false;
        }
        state->wrapper_configured = true;
    }
    state->current_input_gain = move_toward(
        state->current_input_gain, target.input_gain, 0.05f);
    state->current_output_gain = move_toward(
        state->current_output_gain, target.output_gain, 0.05f);
    for (band = 0u; band < 3u; ++band) {
        if (fabsf(target.tone_db[band] - state->current_tone_db[band]) >
            0.01f) {
            state->current_tone_db[band] = move_toward(
                state->current_tone_db[band], target.tone_db[band], 0.1f);
        }
    }
    if (!gp100_snap_tone_tone_coefficients(
            state->current_tone_db, state->tone_coefficients)) {
        return false;
    }
    for (block_start = 0u; block_start < frame_count;
         block_start += GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES) {
        size_t frame;

        if (state->loaded) {
            for (frame = 0u;
                 frame < GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
                 ++frame) {
                model_samples[frame] =
                    interleaved_stereo[(block_start + frame) * 2u] *
                    state->current_input_gain;
            }
            if (!gp100_snap_tone_process_mono(
                    state, model_samples,
                    GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES)) {
                return false;
            }
        } else {
            memset(model_samples, 0, sizeof(model_samples));
        }
        for (frame = 0u;
             frame < GP100_PARTITIONED_CONVOLUTION_BLOCK_SAMPLES;
             ++frame) {
            size_t sample_index = (block_start + frame) * 2u;
            float dry_left = interleaved_stereo[sample_index];
            float dry_right = interleaved_stereo[sample_index + 1u];
            float model_sample = model_samples[frame];

            for (band = 0u; band < 3u; ++band) {
                model_sample = process_tone_section(
                    &state->tone_coefficients[band], state->tone_state[band],
                    model_sample);
            }
            model_sample *= state->current_output_gain;
            state->current_wet = move_toward(
                state->current_wet, enabled ? 1.0f : 0.0f, 0.01f);
            interleaved_stereo[sample_index] =
                ((1.0f - state->current_wet) * dry_left +
                 state->current_wet * model_sample) * 0.8f;
            interleaved_stereo[sample_index + 1u] =
                ((1.0f - state->current_wet) * dry_right +
                 state->current_wet * model_sample) * 0.8f;
        }
    }
    return true;
}
