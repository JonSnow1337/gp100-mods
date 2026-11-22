#include "effect_processor.h"

#include "effect_catalog.h"

#include <string.h>

static uint8_t control_u8(float value);
static int8_t control_i8(float value);
static bool control_bool(float value);

static bool load_cabinet(gp100_effect_processor_t *processor,
                         uint16_t algorithm_index)
{
    uint32_t flash_offset;

    if (processor->asset_read == NULL ||
        !gp100_cabinet_flash_offset(algorithm_index, &flash_offset) ||
        !processor->asset_read(
            processor->asset_context, flash_offset,
            processor->cabinet_record, sizeof(processor->cabinet_record))) {
        return false;
    }
    return gp100_cabinet_load_record(
        processor->cabinet, processor->cabinet_record,
        sizeof(processor->cabinet_record));
}

static bool load_user_amp(gp100_effect_processor_t *processor,
                          uint16_t algorithm_index)
{
    uint8_t descriptor[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES];
    gp100_sysex_user_amp_record_t record;
    uint32_t descriptor_offset;
    uint32_t blob_offset;
    uint8_t user_index;

    if (processor->asset_read == NULL || algorithm_index < 46u ||
        algorithm_index >= 46u + GP100_SYSEX_USER_AMP_SLOT_COUNT) {
        return false;
    }
    user_index = (uint8_t)(algorithm_index - 46u);
    if (!gp100_user_amp_flash_offsets(
            user_index, &descriptor_offset, &blob_offset) ||
        !processor->asset_read(processor->asset_context, descriptor_offset,
                               descriptor, sizeof(descriptor))) {
        return false;
    }
    if (!gp100_user_amp_decode_descriptor_record(
            user_index, descriptor, &record) || !record.present) {
        return gp100_snap_tone_reset_empty(&processor->amplifier->snap_tone);
    }
    if (!processor->asset_read(processor->asset_context, blob_offset,
                               processor->user_amp_blob,
                               sizeof(processor->user_amp_blob))) {
        return false;
    }
    return gp100_snap_tone_load(
        &processor->amplifier->snap_tone, processor->user_amp_blob,
        sizeof(processor->user_amp_blob));
}

static bool parameter_value(gp100_effect_family_t family,
                            const gp100_effect_slot_state_t *slot,
                            size_t parameter_index,
                            float *value)
{
    uint8_t storage_slot;

    if (slot == NULL || value == NULL ||
        !gp100_effect_parameter_storage_slot(
            family,
            slot->algorithm_index,
            parameter_index,
            &storage_slot)) {
        return false;
    }
    *value = slot->parameters[storage_slot];
    return true;
}

static bool reset_delay(gp100_effect_processor_t *processor,
                        const gp100_effect_slot_state_t *slot,
                        const float parameter[9])
{
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_sweet_delay_reset(
            &processor->delay->sweet, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 1u:
        return gp100_p_echo_reset(
            &processor->delay->p_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 2u:
        return gp100_m_echo_reset(
            &processor->delay->m_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 3u:
        return gp100_t_echo_reset(
            &processor->delay->t_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 4u:
        return gp100_echo_999_reset(
            &processor->delay->echo_999, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 5u:
        return gp100_rev_echo_reset(
            &processor->delay->rev_echo, control_u8(parameter[0]), parameter[2],
            control_u8(parameter[1]), control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 6u:
        return gp100_slapback_reset(
            &processor->delay->slapback, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]));
    case 7u:
        return gp100_vin_rack_reset(
            &processor->delay->vin_rack, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            control_u8(parameter[4]), control_bool(parameter[5]),
            control_bool(parameter[6]));
    case 8u:
        return gp100_sweep_echo_reset(
            &processor->delay->sweep_echo, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            parameter[4], control_bool(parameter[5]),
            control_bool(parameter[6]), control_bool(parameter[7]));
    case 9u:
        return gp100_ping_pong_reset(
            &processor->delay->ping_pong, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_bool(parameter[3]),
            control_bool(parameter[4]));
    case 10u:
        return gp100_m_echo2_reset(
            &processor->delay->m_echo2, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_u8(parameter[3]),
            control_bool(parameter[4]), control_bool(parameter[5]));
    case 11u:
        return gp100_dual_echo_reset(
            &processor->delay->dual_echo, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            control_u8(parameter[4]), parameter[5], control_bool(parameter[6]),
            control_bool(parameter[7]), control_bool(parameter[8]));
    default:
        return false;
    }
}

static bool process_delay(gp100_effect_processor_t *processor,
                          const gp100_effect_slot_state_t *slot,
                          const float parameter[9],
                          float *samples,
                          size_t frame_count)
{
    const bool enabled = slot->enabled;
    const uint16_t period = processor->tempo_period_ms;

    switch (slot->algorithm_index) {
    case 0u:
        return gp100_sweet_delay_process(
            &processor->delay->sweet, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 1u:
        return gp100_p_echo_process(
            &processor->delay->p_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 2u:
        return gp100_m_echo_process(
            &processor->delay->m_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 3u:
        return gp100_t_echo_process(
            &processor->delay->t_echo, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 4u:
        return gp100_echo_999_process(
            &processor->delay->echo_999, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 5u:
        return gp100_rev_echo_process(
            &processor->delay->rev_echo, control_u8(parameter[0]), parameter[2],
            control_u8(parameter[1]), control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 6u:
        return gp100_slapback_process(
            &processor->delay->slapback, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]), samples,
            frame_count, enabled);
    case 7u:
        return gp100_vin_rack_process(
            &processor->delay->vin_rack, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            control_u8(parameter[4]), control_bool(parameter[5]),
            control_bool(parameter[6]), period, samples, frame_count, enabled);
    case 8u:
        return gp100_sweep_echo_process(
            &processor->delay->sweep_echo, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            parameter[4], control_bool(parameter[5]),
            control_bool(parameter[6]), control_bool(parameter[7]), period,
            samples, frame_count, enabled);
    case 9u:
        return gp100_ping_pong_process(
            &processor->delay->ping_pong, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_bool(parameter[3]),
            control_bool(parameter[4]), period, samples, frame_count, enabled);
    case 10u:
        return gp100_m_echo2_process(
            &processor->delay->m_echo2, control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_u8(parameter[3]),
            control_bool(parameter[4]), control_bool(parameter[5]), period,
            samples, frame_count, enabled);
    case 11u:
        return gp100_dual_echo_process(
            &processor->delay->dual_echo, control_u8(parameter[0]),
            control_u8(parameter[1]), parameter[2], control_u8(parameter[3]),
            control_u8(parameter[4]), parameter[5], control_bool(parameter[6]),
            control_bool(parameter[7]), control_bool(parameter[8]), period,
            samples, frame_count, enabled);
    default:
        return false;
    }
}

static bool reset_reverb(gp100_effect_processor_t *processor,
                         const gp100_effect_slot_state_t *slot,
                         const float parameter[9])
{
    memset(processor->reverb, 0, sizeof(*processor->reverb));
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_room_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 1u:
        return gp100_hall_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 2u:
        return gp100_church_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 3u:
        return gp100_plate_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 4u:
        return gp100_spring_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 5u:
        return gp100_n_star_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]));
    case 6u:
        return gp100_deep_sea_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]));
    case 7u:
        return gp100_mod_verb_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_i8(parameter[3]), control_i8(parameter[4]),
            control_bool(parameter[5]));
    case 8u:
        return gp100_clear_sky_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]));
    case 9u:
        return gp100_shimmer_reverb_reset(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_i8(parameter[3]), control_i8(parameter[4]),
            control_bool(parameter[5]));
    default:
        return false;
    }
}

static bool process_reverb(gp100_effect_processor_t *processor,
                           const gp100_effect_slot_state_t *slot,
                           const float parameter[9],
                           float *samples,
                           size_t frame_count)
{
    const bool enabled = slot->enabled;

    switch (slot->algorithm_index) {
    case 0u:
        return gp100_room_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]), samples, frame_count, enabled);
    case 1u:
        return gp100_hall_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]), samples, frame_count, enabled);
    case 2u:
        return gp100_church_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]), samples, frame_count, enabled);
    case 3u:
        return gp100_plate_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]), samples, frame_count, enabled);
    case 4u:
        return gp100_spring_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_bool(parameter[3]), samples, frame_count, enabled);
    case 5u:
        return gp100_n_star_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]),
            samples, frame_count, enabled);
    case 6u:
        return gp100_deep_sea_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]),
            samples, frame_count, enabled);
    case 7u:
        return gp100_mod_verb_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_i8(parameter[3]), control_i8(parameter[4]),
            control_bool(parameter[5]), samples, frame_count, enabled);
    case 8u:
        return gp100_clear_sky_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_bool(parameter[2]),
            samples, frame_count, enabled);
    case 9u:
        return gp100_shimmer_reverb_process(
            processor->reverb, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_i8(parameter[3]), control_i8(parameter[4]),
            control_bool(parameter[5]), samples, frame_count, enabled);
    default:
        return false;
    }
}

static bool reset_pre_fx(gp100_effect_processor_t *processor,
                         const gp100_effect_slot_state_t *slot,
                         const float parameter[9])
{
    if (slot->algorithm_index == 4u) {
        gp100_ac_sim_t *simulator = &processor->pre_fx->ac_sim;
        const bool same_algorithm = processor->pre_fx_loaded &&
            processor->pre_fx_algorithm == slot->algorithm_index;
        /* The factory initializer leaves both mode fade gains in place. */
        const float fade_in = same_algorithm ? simulator->mode_gain : 0.0f;
        const float fade_out = same_algorithm ? simulator->mode_fade_out_gain : 0.0f;
        if (!gp100_ac_sim_reset(
                simulator, control_u8(parameter[0]), control_u8(parameter[1]),
                control_u8(parameter[2]), control_u8(parameter[3]))) {
            return false;
        }
        simulator->mode_gain = fade_in;
        simulator->mode_fade_out_gain = fade_out;
        return true;
    }
    memset(processor->pre_fx, 0, sizeof(*processor->pre_fx));
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_comp_reset(
            &processor->pre_fx->comp, control_u8(parameter[0]),
            control_u8(parameter[1]));
    case 1u:
        return gp100_comp4_reset(
            &processor->pre_fx->comp4, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]));
    case 2u:
        return gp100_boost_reset(
            &processor->pre_fx->boost, control_u8(parameter[0]),
            control_bool(parameter[1]));
    case 3u:
        return gp100_boost14_reset(
            &processor->pre_fx->boost14, control_u8(parameter[0]));
    case 5u:
        return gp100_touch_wah_reset(
            &processor->pre_fx->touch_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]));
    case 6u:
        return gp100_auto_wah_reset(
            &processor->pre_fx->auto_wah, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]),
            control_u8(parameter[5]), control_bool(parameter[6]));
    case 7u:
        return gp100_vintage_wah_reset(
            &processor->pre_fx->vintage_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]));
    case 8u:
        return gp100_classic_wah_reset(
            &processor->pre_fx->classic_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]));
    case 9u:
        return gp100_octaver_reset(
            &processor->pre_fx->octaver, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]));
    case 10u:
        return gp100_pitch_shifter_reset(
            &processor->pre_fx->pitch_shifter, control_i8(parameter[0]),
            control_i8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]));
    case 11u:
        return gp100_pitch_bender_reset(
            &processor->pre_fx->pitch_bender, control_i8(parameter[0]),
            control_i8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]));
    case 12u:
        return gp100_hammy_reset(
            &processor->pre_fx->hammy, control_u8(parameter[0]),
            control_bool(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]));
    case 13u:
        return gp100_saturate_reset(
            &processor->pre_fx->saturate, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]));
    case 14u:
        return gp100_step_filter_reset(
            &processor->pre_fx->step_filter, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), parameter[4],
            control_bool(parameter[5]));
    case 15u:
        return gp100_ring_modulator_reset(
            &processor->pre_fx->ring_modulator, control_u8(parameter[0]),
            control_u8(parameter[1]), control_i8(parameter[2]),
            control_u8(parameter[3]));
    default:
        return false;
    }
}

static bool process_pre_fx(gp100_effect_processor_t *processor,
                           const gp100_effect_slot_state_t *slot,
                           const float parameter[9],
                           float *samples,
                           size_t frame_count)
{
    const bool enabled = slot->enabled;

    switch (slot->algorithm_index) {
    case 0u:
        return gp100_comp_process(
            &processor->pre_fx->comp, control_u8(parameter[0]),
            control_u8(parameter[1]), samples, frame_count, enabled);
    case 1u:
        return gp100_comp4_process(
            &processor->pre_fx->comp4, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), samples, frame_count, enabled);
    case 2u:
        return gp100_boost_process(
            &processor->pre_fx->boost, control_u8(parameter[0]),
            control_bool(parameter[1]), samples, frame_count, enabled);
    case 3u:
        return gp100_boost14_process(
            &processor->pre_fx->boost14, control_u8(parameter[0]),
            samples, frame_count, enabled);
    case 4u:
        return gp100_ac_sim_process(
            &processor->pre_fx->ac_sim, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), samples, frame_count, enabled);
    case 5u:
        return gp100_touch_wah_process(
            &processor->pre_fx->touch_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, enabled);
    case 6u:
        return gp100_auto_wah_process(
            &processor->pre_fx->auto_wah, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]),
            control_u8(parameter[5]), control_bool(parameter[6]),
            processor->tempo_period_ms, samples, frame_count, enabled);
    case 7u:
        return gp100_vintage_wah_process(
            &processor->pre_fx->vintage_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, enabled);
    case 8u:
        return gp100_classic_wah_process(
            &processor->pre_fx->classic_wah, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, enabled);
    case 9u:
        return gp100_octaver_process(
            &processor->pre_fx->octaver, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, enabled);
    case 10u:
        return gp100_pitch_shifter_process(
            &processor->pre_fx->pitch_shifter, control_i8(parameter[0]),
            control_i8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, enabled);
    case 11u:
        return gp100_pitch_bender_process(
            &processor->pre_fx->pitch_bender, control_i8(parameter[0]),
            control_i8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, enabled);
    case 12u:
        return gp100_hammy_process(
            &processor->pre_fx->hammy, control_u8(parameter[0]),
            control_bool(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), samples, frame_count, enabled);
    case 13u:
        return gp100_saturate_process(
            &processor->pre_fx->saturate, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), samples, frame_count, enabled);
    case 14u:
        return gp100_step_filter_process(
            &processor->pre_fx->step_filter, control_u8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            control_u8(parameter[3]), parameter[4], control_bool(parameter[5]),
            processor->tempo_period_ms, samples, frame_count, enabled);
    case 15u:
        return gp100_ring_modulator_process(
            &processor->pre_fx->ring_modulator, control_u8(parameter[0]),
            control_u8(parameter[1]), control_i8(parameter[2]),
            control_u8(parameter[3]), samples, frame_count, enabled);
    default:
        return false;
    }
}

static bool reset_noise_reduction(
    gp100_effect_processor_t *processor,
    const gp100_effect_slot_state_t *slot,
    const float parameter[9])
{
    if (slot->algorithm_index > 1u) return false;
    return gp100_noise_gate_reset(
        processor->noise_reduction,
        (gp100_noise_gate_type_t)slot->algorithm_index,
        parameter);
}

static bool process_noise_reduction(
    gp100_effect_processor_t *processor,
    const gp100_effect_slot_state_t *slot,
    const float parameter[9],
    float *samples,
    size_t frame_count)
{
    const float *detector = processor->gate_detector_frames >= frame_count
        ? processor->gate_detector : NULL;
    return gp100_noise_gate_process(
        processor->noise_reduction, parameter, samples,
        detector != NULL ? detector : samples, frame_count, slot->enabled);
}

static bool reset_equalizer(gp100_effect_processor_t *processor,
                            const gp100_effect_slot_state_t *slot,
                            const float parameter[9])
{
    if (slot->algorithm_index > 2u) return false;
    return gp100_equalizer_reset(
        processor->equalizer,
        (gp100_equalizer_type_t)slot->algorithm_index,
        parameter);
}

static bool process_equalizer(gp100_effect_processor_t *processor,
                              const gp100_effect_slot_state_t *slot,
                              const float parameter[9],
                              float *samples,
                              size_t frame_count)
{
    return gp100_equalizer_process(
        processor->equalizer, parameter, samples, frame_count, slot->enabled);
}

static bool reset_distortion(gp100_effect_processor_t *processor,
                             const gp100_effect_slot_state_t *slot,
                             const float parameter[9])
{
    memset(processor->distortion, 0, sizeof(*processor->distortion));
    switch (slot->algorithm_index) {
    case 0u: return gp100_penesas_reset(
        &processor->distortion->penesas, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 1u: return gp100_scream_overdrive_reset(
        &processor->distortion->scream_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_bool(parameter[3]), control_bool(parameter[4]));
    case 2u: return gp100_tube_clipper_reset(
        &processor->distortion->tube_clipper, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]));
    case 3u: return gp100_taichi_overdrive_reset(
        &processor->distortion->taichi_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]));
    case 4u: return gp100_plustortion_reset(
        &processor->distortion->plustortion, control_u8(parameter[0]),
        control_u8(parameter[1]));
    case 5u: return gp100_chief_reset(
        &processor->distortion->chief, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]));
    case 6u: return gp100_green_overdrive_reset(
        &processor->distortion->green_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 7u: return gp100_yellow_overdrive_reset(
        &processor->distortion->yellow_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]));
    case 8u: return gp100_super_overdrive_reset(
        &processor->distortion->super_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 9u: return gp100_blues_overdrive_reset(
        &processor->distortion->blues_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 10u: return gp100_lazaro_reset(
        &processor->distortion->lazaro, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 11u: return gp100_red_haze_reset(
        &processor->distortion->red_haze, control_u8(parameter[0]),
        control_u8(parameter[1]));
    case 12u: return gp100_darktale_reset(
        &processor->distortion->darktale, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 13u: return gp100_flex_overdrive_reset(
        &processor->distortion->flex_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]));
    case 14u: return gp100_sm_distortion_reset(
        &processor->distortion->sm_distortion, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 15u: return gp100_la_charger_reset(
        &processor->distortion->la_charger, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]));
    case 16u: return gp100_bass_distortion_reset(
        &processor->distortion->bass_distortion, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]));
    case 17u: return gp100_boost_reset(
        &processor->distortion->boost, control_u8(parameter[0]),
        control_bool(parameter[1]));
    case 18u: return gp100_boost14_reset(
        &processor->distortion->boost14, control_u8(parameter[0]));
    default: return false;
    }
}

static bool process_distortion(gp100_effect_processor_t *processor,
                               const gp100_effect_slot_state_t *slot,
                               const float parameter[9],
                               float *samples,
                               size_t frames)
{
    const bool enabled = slot->enabled;
    switch (slot->algorithm_index) {
    case 0u: return gp100_penesas_process(
        &processor->distortion->penesas, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 1u: return gp100_scream_overdrive_process(
        &processor->distortion->scream_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_bool(parameter[3]), control_bool(parameter[4]), samples,
        frames, enabled);
    case 2u: return gp100_tube_clipper_process(
        &processor->distortion->tube_clipper, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), samples, frames, enabled);
    case 3u: return gp100_taichi_overdrive_process(
        &processor->distortion->taichi_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), samples, frames, enabled);
    case 4u: return gp100_plustortion_process(
        &processor->distortion->plustortion, control_u8(parameter[0]),
        control_u8(parameter[1]), samples, frames, enabled);
    case 5u: return gp100_chief_process(
        &processor->distortion->chief, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]), samples, frames,
        enabled);
    case 6u: return gp100_green_overdrive_process(
        &processor->distortion->green_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 7u: return gp100_yellow_overdrive_process(
        &processor->distortion->yellow_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), samples, frames, enabled);
    case 8u: return gp100_super_overdrive_process(
        &processor->distortion->super_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 9u: return gp100_blues_overdrive_process(
        &processor->distortion->blues_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 10u: return gp100_lazaro_process(
        &processor->distortion->lazaro, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 11u: return gp100_red_haze_process(
        &processor->distortion->red_haze, control_u8(parameter[0]),
        control_u8(parameter[1]), samples, frames, enabled);
    case 12u: return gp100_darktale_process(
        &processor->distortion->darktale, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 13u: return gp100_flex_overdrive_process(
        &processor->distortion->flex_overdrive, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]), samples, frames,
        enabled);
    case 14u: return gp100_sm_distortion_process(
        &processor->distortion->sm_distortion, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 15u: return gp100_la_charger_process(
        &processor->distortion->la_charger, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]), samples, frames,
        enabled);
    case 16u: return gp100_bass_distortion_process(
        &processor->distortion->bass_distortion, control_u8(parameter[0]),
        control_u8(parameter[1]), control_u8(parameter[2]),
        control_u8(parameter[3]), control_u8(parameter[4]), samples, frames,
        enabled);
    case 17u: return gp100_boost_process(
        &processor->distortion->boost, control_u8(parameter[0]),
        control_bool(parameter[1]), samples, frames, enabled);
    case 18u: return gp100_boost14_process(
        &processor->distortion->boost14, control_u8(parameter[0]), samples,
        frames, enabled);
    default: return false;
    }
}

static bool reset_amplifier(
    gp100_effect_processor_t *processor,
    const gp100_effect_slot_state_t *slot,
    const float parameter[9],
    bool reuse_asset)
{
    if (slot->algorithm_index >= 46u) {
        if (!reuse_asset) {
            return load_user_amp(processor, slot->algorithm_index);
        }
        if (!processor->amplifier->snap_tone.loaded) {
            return gp100_snap_tone_reset_empty(&processor->amplifier->snap_tone);
        }
        return gp100_snap_tone_load(
            &processor->amplifier->snap_tone, processor->user_amp_blob,
            sizeof(processor->user_amp_blob));
    }
    memset(processor->amplifier, 0, sizeof(*processor->amplifier));
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_tweedy_amplifier_reset(
            &processor->amplifier->tweedy, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]));
    case 1u:
        return gp100_bellman59n_amplifier_reset(
            &processor->amplifier->bellman59n, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 2u:
        return gp100_dark_twin_amplifier_reset(
            &processor->amplifier->dark_twin, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 3u:
        return gp100_lstar_cl_amplifier_reset(
            &processor->amplifier->lstar_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 4u:
        return gp100_foxy30n_amplifier_reset(
            &processor->amplifier->foxy30n, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]));
    case 5u:
        return gp100_bogsv_cl_amplifier_reset(
            &processor->amplifier->bogsv_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 6u:
        return gp100_j120_cl_amplifier_reset(
            &processor->amplifier->j120_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 7u:
        return gp100_match_cl_amplifier_reset(
            &processor->amplifier->match_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 8u:
        return gp100_z38_cl_amplifier_reset(
            &processor->amplifier->z38_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 9u:
        return gp100_knights_cl_amplifier_reset(
            &processor->amplifier->knights_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 10u:
        return gp100_bad_kt_cl_amplifier_reset(
            &processor->amplifier->bad_kt_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]));
    case 11u:
        return gp100_uk45_amplifier_reset(
            &processor->amplifier->uk45, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 12u:
        return gp100_uk50jp_amplifier_reset(
            &processor->amplifier->uk50jp, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]), control_u8(parameter[6]));
    case 13u:
        return gp100_uk800_amplifier_reset(
            &processor->amplifier->uk800, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 14u:
        return gp100_flagman_amplifier_reset(
            &processor->amplifier->flagman, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 15u:
        return gp100_z38_od_amplifier_reset(
            &processor->amplifier->z38_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 16u:
        return gp100_bogsv_od_amplifier_reset(
            &processor->amplifier->bogsv_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 17u:
        return gp100_bellman59b_amplifier_reset(
            &processor->amplifier->bellman59b, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 18u:
        return gp100_foxy30tb_amplifier_reset(
            &processor->amplifier->foxy30tb, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 19u:
        return gp100_supdual_od_amplifier_reset(
            &processor->amplifier->supdual_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 20u:
        return gp100_match_od_amplifier_reset(
            &processor->amplifier->match_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 21u:
        return gp100_solo100_od_amplifier_reset(
            &processor->amplifier->solo100_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 22u:
        return gp100_lstar_od_amplifier_reset(
            &processor->amplifier->lstar_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 23u:
        return gp100_bad_kt_od_amplifier_reset(
            &processor->amplifier->bad_kt_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 24u:
        return gp100_mess2c1_amplifier_reset(
            &processor->amplifier->mess2c1, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 25u:
        return gp100_mess2c2_amplifier_reset(
            &processor->amplifier->mess2c2, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 26u:
        return gp100_knights_od_amplifier_reset(
            &processor->amplifier->knights_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 27u:
        return gp100_dizz_vh_amplifier_reset(
            &processor->amplifier->dizz_vh, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 28u:
        return gp100_eagle120_amplifier_reset(
            &processor->amplifier->eagle120, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 29u:
        return gp100_ev51_amplifier_reset(
            &processor->amplifier->ev51, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 30u:
        return gp100_solo100_ld_amplifier_reset(
            &processor->amplifier->solo100_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 31u:
        return gp100_mess4_ld_amplifier_reset(
            &processor->amplifier->mess4_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 32u:
        return gp100_mess_dualv_amplifier_reset(
            &processor->amplifier->mess_dualv, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 33u:
        return gp100_power_ld_amplifier_reset(
            &processor->amplifier->power_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 34u:
        return gp100_flagman_plus_amplifier_reset(
            &processor->amplifier->flagman_plus, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 35u:
        return gp100_juice_r100_amplifier_reset(
            &processor->amplifier->juice_r100, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 36u:
        return gp100_mess_dualm_amplifier_reset(
            &processor->amplifier->mess_dualm, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 37u:
        return gp100_bog_bluev_amplifier_reset(
            &processor->amplifier->bog_bluev, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 38u:
        return gp100_bog_redm_amplifier_reset(
            &processor->amplifier->bog_redm, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 39u:
        return gp100_uk900_amplifier_reset(
            &processor->amplifier->uk900, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 40u:
        return gp100_classic_bass_amplifier_reset(
            &processor->amplifier->classic_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    case 41u:
        return gp100_bass_pre_amplifier_reset(
            &processor->amplifier->bass_pre, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 42u:
        return gp100_mini_bass_amplifier_reset(
            &processor->amplifier->mini_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]));
    case 43u:
        return gp100_foxy_bass_amplifier_reset(
            &processor->amplifier->foxy_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]));
    case 44u:
        return gp100_mess_bass_amplifier_reset(
            &processor->amplifier->mess_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]));
    case 45u:
        return gp100_ac_pre_amplifier_reset(
            &processor->amplifier->ac_pre, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]));
    default:
        return false;
    }
}

static bool process_amplifier(
    gp100_effect_processor_t *processor,
    const gp100_effect_slot_state_t *slot,
    const float parameter[9],
    float *samples,
    size_t frame_count)
{
    if (slot->algorithm_index >= 46u && slot->algorithm_index <= 50u) {
        return gp100_snap_tone_process_stereo(
            &processor->amplifier->snap_tone, parameter, slot->enabled,
            samples, frame_count);
    }
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_tweedy_amplifier_process(
            &processor->amplifier->tweedy, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, slot->enabled);
    case 1u:
        return gp100_bellman59n_amplifier_process(
            &processor->amplifier->bellman59n, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 2u:
        return gp100_dark_twin_amplifier_process(
            &processor->amplifier->dark_twin, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 3u:
        return gp100_lstar_cl_amplifier_process(
            &processor->amplifier->lstar_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 4u:
        return gp100_foxy30n_amplifier_process(
            &processor->amplifier->foxy30n, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]),
            samples, frame_count, slot->enabled);
    case 5u:
        return gp100_bogsv_cl_amplifier_process(
            &processor->amplifier->bogsv_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 6u:
        return gp100_j120_cl_amplifier_process(
            &processor->amplifier->j120_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 7u:
        return gp100_match_cl_amplifier_process(
            &processor->amplifier->match_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 8u:
        return gp100_z38_cl_amplifier_process(
            &processor->amplifier->z38_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 9u:
        return gp100_knights_cl_amplifier_process(
            &processor->amplifier->knights_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 10u:
        return gp100_bad_kt_cl_amplifier_process(
            &processor->amplifier->bad_kt_cl, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, slot->enabled);
    case 11u:
        return gp100_uk45_amplifier_process(
            &processor->amplifier->uk45, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 12u:
        return gp100_uk50jp_amplifier_process(
            &processor->amplifier->uk50jp, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]), control_u8(parameter[6]),
            samples, frame_count, slot->enabled);
    case 13u:
        return gp100_uk800_amplifier_process(
            &processor->amplifier->uk800, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 14u:
        return gp100_flagman_amplifier_process(
            &processor->amplifier->flagman, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 15u:
        return gp100_z38_od_amplifier_process(
            &processor->amplifier->z38_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 16u:
        return gp100_bogsv_od_amplifier_process(
            &processor->amplifier->bogsv_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 17u:
        return gp100_bellman59b_amplifier_process(
            &processor->amplifier->bellman59b, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 18u:
        return gp100_foxy30tb_amplifier_process(
            &processor->amplifier->foxy30tb, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 19u:
        return gp100_supdual_od_amplifier_process(
            &processor->amplifier->supdual_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 20u:
        return gp100_match_od_amplifier_process(
            &processor->amplifier->match_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 21u:
        return gp100_solo100_od_amplifier_process(
            &processor->amplifier->solo100_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 22u:
        return gp100_lstar_od_amplifier_process(
            &processor->amplifier->lstar_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 23u:
        return gp100_bad_kt_od_amplifier_process(
            &processor->amplifier->bad_kt_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 24u:
        return gp100_mess2c1_amplifier_process(
            &processor->amplifier->mess2c1, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 25u:
        return gp100_mess2c2_amplifier_process(
            &processor->amplifier->mess2c2, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 26u:
        return gp100_knights_od_amplifier_process(
            &processor->amplifier->knights_od, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 27u:
        return gp100_dizz_vh_amplifier_process(
            &processor->amplifier->dizz_vh, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 28u:
        return gp100_eagle120_amplifier_process(
            &processor->amplifier->eagle120, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 29u:
        return gp100_ev51_amplifier_process(
            &processor->amplifier->ev51, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 30u:
        return gp100_solo100_ld_amplifier_process(
            &processor->amplifier->solo100_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 31u:
        return gp100_mess4_ld_amplifier_process(
            &processor->amplifier->mess4_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 32u:
        return gp100_mess_dualv_amplifier_process(
            &processor->amplifier->mess_dualv, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 33u:
        return gp100_power_ld_amplifier_process(
            &processor->amplifier->power_ld, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 34u:
        return gp100_flagman_plus_amplifier_process(
            &processor->amplifier->flagman_plus, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 35u:
        return gp100_juice_r100_amplifier_process(
            &processor->amplifier->juice_r100, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 36u:
        return gp100_mess_dualm_amplifier_process(
            &processor->amplifier->mess_dualm, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 37u:
        return gp100_bog_bluev_amplifier_process(
            &processor->amplifier->bog_bluev, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 38u:
        return gp100_bog_redm_amplifier_process(
            &processor->amplifier->bog_redm, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 39u:
        return gp100_uk900_amplifier_process(
            &processor->amplifier->uk900, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 40u:
        return gp100_classic_bass_amplifier_process(
            &processor->amplifier->classic_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    case 41u:
        return gp100_bass_pre_amplifier_process(
            &processor->amplifier->bass_pre, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 42u:
        return gp100_mini_bass_amplifier_process(
            &processor->amplifier->mini_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, slot->enabled);
    case 43u:
        return gp100_foxy_bass_amplifier_process(
            &processor->amplifier->foxy_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, slot->enabled);
    case 44u:
        return gp100_mess_bass_amplifier_process(
            &processor->amplifier->mess_bass, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]),
            samples, frame_count, slot->enabled);
    case 45u:
        return gp100_ac_pre_amplifier_process(
            &processor->amplifier->ac_pre, control_u8(parameter[0]), control_u8(parameter[1]), control_u8(parameter[2]), control_u8(parameter[3]), control_u8(parameter[4]), control_u8(parameter[5]),
            samples, frame_count, slot->enabled);
    default:
        return false;
    }
}

static uint8_t control_u8(float value)
{
    if (value <= 0.0f) return 0u;
    if (value >= 255.0f) return 255u;
    return (uint8_t)(value + 0.5f);
}

static int8_t control_i8(float value)
{
    if (value <= -128.0f) return INT8_MIN;
    if (value >= 127.0f) return INT8_MAX;
    return (int8_t)(value < 0.0f ? value - 0.5f : value + 0.5f);
}

static bool control_bool(float value)
{
    return value >= 0.5f;
}

static bool reset_modulation(gp100_effect_processor_t *processor,
                             const gp100_effect_slot_state_t *slot,
                             const float parameter[5])
{
    if (slot->algorithm_index == 1u) {
        gp100_g_chorus_t *chorus = &processor->modulation->g_chorus;
        const bool same_algorithm = processor->modulation_loaded &&
            processor->modulation_algorithm == slot->algorithm_index;
        float oldest_input[2] = {0.0f, 0.0f};
        float oldest_output[2] = {0.0f, 0.0f};

        /* The initializer clears only the three newer taps in each history. */
        if (same_algorithm) {
            for (size_t channel = 0u; channel < 2u; ++channel) {
                oldest_input[channel] = chorus->output_filter[channel].x[3];
                oldest_output[channel] = chorus->output_filter[channel].y[3];
            }
        }
        if (!gp100_g_chorus_reset(
                chorus, control_u8(parameter[0]), parameter[1],
                control_u8(parameter[2]), control_bool(parameter[3]))) {
            return false;
        }
        for (size_t channel = 0u; channel < 2u; ++channel) {
            chorus->output_filter[channel].x[3] = oldest_input[channel];
            chorus->output_filter[channel].y[3] = oldest_output[channel];
        }
        return true;
    }
    memset(processor->modulation, 0, sizeof(*processor->modulation));
    switch (slot->algorithm_index) {
    case 0u:
        return gp100_a_chorus_reset(
            &processor->modulation->a_chorus, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 2u:
        return gp100_b_chorus_reset(
            &processor->modulation->b_chorus, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 3u:
        return gp100_detune_reset(
            &processor->modulation->detune, control_i8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]));
    case 4u:
        return gp100_flanger_reset(
            &processor->modulation->flanger, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_u8(parameter[3]), control_bool(parameter[4]));
    case 5u:
        return gp100_vibrato_reset(
            &processor->modulation->vibrato, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]));
    case 6u:
        return gp100_phaser_reset(
            &processor->modulation->phaser, parameter[0],
            control_bool(parameter[1]));
    case 7u:
        return gp100_vibe_reset(
            &processor->modulation->vibe, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]));
    case 8u:
        return gp100_opto_trem_reset(
            &processor->modulation->opto_trem, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]));
    case 9u:
        return gp100_sine_trem_reset(
            &processor->modulation->sine_trem, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]));
    case 10u:
        return gp100_triangle_trem_reset(
            &processor->modulation->triangle_trem,
            control_u8(parameter[0]), parameter[1],
            control_u8(parameter[2]), control_bool(parameter[3]));
    case 11u:
        return gp100_bias_trem_reset(
            &processor->modulation->bias_trem, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), control_u8(parameter[4]));
    default:
        return false;
    }
}

static bool process_modulation(gp100_effect_processor_t *processor,
                               const gp100_effect_slot_state_t *slot,
                               const float parameter[5],
                               float *samples,
                               size_t frame_count)
{
    const bool enabled = slot->enabled;
    const uint16_t period = processor->tempo_period_ms;

    switch (slot->algorithm_index) {
    case 0u:
        return gp100_a_chorus_process(
            &processor->modulation->a_chorus, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), period, samples, frame_count, enabled);
    case 1u:
        return gp100_g_chorus_process(
            &processor->modulation->g_chorus, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), period, samples, frame_count, enabled);
    case 2u:
        return gp100_b_chorus_process(
            &processor->modulation->b_chorus, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), period, samples, frame_count, enabled);
    case 3u:
        return gp100_detune_process(
            &processor->modulation->detune, control_i8(parameter[0]),
            control_u8(parameter[1]), control_u8(parameter[2]),
            samples, frame_count, enabled);
    case 4u:
        return gp100_flanger_process(
            &processor->modulation->flanger, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_u8(parameter[3]), control_bool(parameter[4]), period,
            samples, frame_count, enabled);
    case 5u:
        return gp100_vibrato_process(
            &processor->modulation->vibrato, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]), period,
            samples, frame_count, enabled);
    case 6u:
        return gp100_phaser_process(
            &processor->modulation->phaser, parameter[0],
            control_bool(parameter[1]), period, samples, frame_count, enabled);
    case 7u:
        return gp100_vibe_process(
            &processor->modulation->vibe, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]), period,
            samples, frame_count, enabled);
    case 8u:
        return gp100_opto_trem_process(
            &processor->modulation->opto_trem, control_u8(parameter[0]),
            parameter[1], control_bool(parameter[2]), period,
            samples, frame_count, enabled);
    case 9u:
        return gp100_sine_trem_process(
            &processor->modulation->sine_trem, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), period, samples, frame_count, enabled);
    case 10u:
        return gp100_triangle_trem_process(
            &processor->modulation->triangle_trem, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), period, samples, frame_count, enabled);
    case 11u:
        return gp100_bias_trem_process(
            &processor->modulation->bias_trem, control_u8(parameter[0]),
            parameter[1], control_u8(parameter[2]),
            control_bool(parameter[3]), control_u8(parameter[4]), period,
            samples, frame_count, enabled);
    default:
        return false;
    }
}

void gp100_effect_processor_reset(
    gp100_effect_processor_t *processor)
{
    if (processor == NULL) return;
    memset(processor, 0, sizeof(*processor));
    processor->tempo_period_ms = 60000u /
        GP100_EFFECT_PROCESSOR_DEFAULT_TEMPO_BPM;
}

size_t gp100_effect_processor_state_size(gp100_effect_family_t family)
{
    static const size_t sizes[] = {
        [GP100_EFFECT_PRE_FX] = sizeof(gp100_pre_fx_processor_state_t),
        [GP100_EFFECT_DISTORTION] = sizeof(gp100_distortion_processor_state_t),
        [GP100_EFFECT_AMPLIFIER] = sizeof(gp100_amplifier_processor_state_t),
        [GP100_EFFECT_MODULATION] = sizeof(gp100_modulation_processor_state_t),
        [GP100_EFFECT_DELAY] = sizeof(gp100_delay_processor_state_t),
        [GP100_EFFECT_REVERB] = sizeof(gp100_room_reverb_t),
        [GP100_EFFECT_NOISE_REDUCTION] = sizeof(gp100_noise_gate_t),
        [GP100_EFFECT_EQUALIZER] = sizeof(gp100_equalizer_t),
        [GP100_EFFECT_CABINET] = sizeof(gp100_cabinet_t),
    };
    return (unsigned)family < GP100_EFFECT_FAMILY_COUNT ? sizes[family] : 0u;
}

bool gp100_effect_processor_bind(gp100_effect_processor_t *processor,
                                 gp100_effect_family_t family,
                                 void *state)
{
    if (processor == NULL || state == NULL) {
        return false;
    }
    switch (family) {
    case GP100_EFFECT_PRE_FX: processor->pre_fx = state; break;
    case GP100_EFFECT_DISTORTION: processor->distortion = state; break;
    case GP100_EFFECT_AMPLIFIER: processor->amplifier = state; break;
    case GP100_EFFECT_MODULATION: processor->modulation = state; break;
    case GP100_EFFECT_DELAY: processor->delay = state; break;
    case GP100_EFFECT_REVERB: processor->reverb = state; break;
    case GP100_EFFECT_NOISE_REDUCTION: processor->noise_reduction = state; break;
    case GP100_EFFECT_EQUALIZER: processor->equalizer = state; break;
    case GP100_EFFECT_CABINET: processor->cabinet = state; break;
    default: return false;
    }
    processor->bound_families |= (uint16_t)(1u << family);
    return true;
}

bool gp100_effect_processor_set_tempo_bpm(
    gp100_effect_processor_t *processor,
    uint16_t tempo_bpm)
{
    if (processor == NULL || tempo_bpm < 40u || tempo_bpm > 250u) {
        return false;
    }
    processor->tempo_period_ms =
        (uint16_t)((60000u + tempo_bpm / 2u) / tempo_bpm);
    return true;
}

bool gp100_effect_processor_set_asset_reader(
    gp100_effect_processor_t *processor,
    gp100_effect_asset_read_fn read_asset,
    void *context)
{
    if (processor == NULL) {
        return false;
    }
    processor->asset_read = read_asset;
    processor->asset_context = context;
    processor->cabinet_loaded = false;
    if (processor->cabinet != NULL) {
        gp100_cabinet_reset(processor->cabinet);
    }
    if (processor->amplifier_loaded && processor->amplifier_algorithm >= 46u) {
        processor->amplifier_loaded = false;
        memset(&processor->amplifier->snap_tone, 0,
               sizeof(processor->amplifier->snap_tone));
    }
    return true;
}

void gp100_effect_processor_set_gate_detector(
    gp100_effect_processor_t *processor,
    const float *interleaved_stereo,
    size_t frame_count)
{
    if (processor != NULL) {
        processor->gate_detector = interleaved_stereo;
        processor->gate_detector_frames = interleaved_stereo == NULL ? 0u : frame_count;
    }
}

void gp100_effect_processor_recall_preset(
    gp100_effect_processor_t *processor,
    const gp100_effect_runtime_t *effects)
{
    if (processor != NULL && effects != NULL) {
        uint16_t initialize = 0u;
        for (size_t family = 0u; family < GP100_EFFECT_FAMILY_COUNT;
             ++family) {
            if (effects->slots[family].algorithm_loaded) {
                initialize |= (uint16_t)(UINT16_C(1) << family);
            }
        }
        processor->recall_initialize_mask = initialize;
    }
}

bool gp100_effect_processor_invalidate_cabinet(
    gp100_effect_processor_t *processor,
    uint16_t cabinet_index)
{
    if (processor == NULL || cabinet_index >= GP100_CABINET_COUNT) {
        return false;
    }
    if (processor->cabinet_loaded &&
        processor->cabinet_algorithm == cabinet_index) {
        processor->cabinet_loaded = false;
        gp100_cabinet_reset(processor->cabinet);
    }
    return true;
}

bool gp100_effect_processor_invalidate_user_amp(
    gp100_effect_processor_t *processor,
    uint8_t user_index)
{
    uint16_t algorithm_index;

    if (processor == NULL ||
        user_index >= GP100_SYSEX_USER_AMP_SLOT_COUNT) {
        return false;
    }
    algorithm_index = (uint16_t)(46u + user_index);
    if (processor->amplifier_loaded &&
        processor->amplifier_algorithm == algorithm_index) {
        processor->amplifier_loaded = false;
        memset(&processor->amplifier->snap_tone, 0,
               sizeof(processor->amplifier->snap_tone));
    }
    return true;
}

uint16_t gp100_effect_processor_supported_families(void)
{
    return GP100_EFFECT_PROCESSOR_PRE_FX_MASK |
           GP100_EFFECT_PROCESSOR_DISTORTION_MASK |
           GP100_EFFECT_PROCESSOR_AMPLIFIER_MASK |
           GP100_EFFECT_PROCESSOR_NOISE_REDUCTION_MASK |
           GP100_EFFECT_PROCESSOR_CABINET_MASK |
           GP100_EFFECT_PROCESSOR_EQUALIZER_MASK |
           GP100_EFFECT_PROCESSOR_MODULATION_MASK |
           GP100_EFFECT_PROCESSOR_DELAY_MASK |
           GP100_EFFECT_PROCESSOR_REVERB_MASK;
}

static gp100_effect_processor_result_t process_slot(
    gp100_effect_processor_t *processor,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count,
    bool prepare_only)
{
    float parameter[9] = {0.0f};
    size_t parameter_count;
    size_t index;
    bool reset_required;
    uint16_t family_bit;
    bool preset_recalled;

    if (processor == NULL || slot == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        (unsigned)family >= GP100_EFFECT_FAMILY_COUNT ||
        frame_count > SIZE_MAX / 2u) {
        return GP100_EFFECT_PROCESSOR_INVALID;
    }
    if (family != GP100_EFFECT_PRE_FX &&
        family != GP100_EFFECT_DISTORTION &&
        family != GP100_EFFECT_AMPLIFIER &&
        family != GP100_EFFECT_NOISE_REDUCTION &&
        family != GP100_EFFECT_CABINET &&
        family != GP100_EFFECT_EQUALIZER &&
        family != GP100_EFFECT_MODULATION &&
        family != GP100_EFFECT_DELAY &&
        family != GP100_EFFECT_REVERB) {
        ++processor->unsupported_blocks;
        return GP100_EFFECT_PROCESSOR_UNSUPPORTED;
    }
    if (!slot->algorithm_loaded) {
        ++processor->bypassed_blocks;
        return GP100_EFFECT_PROCESSOR_BYPASSED;
    }
    if ((processor->bound_families & (1u << family)) == 0u) {
        return GP100_EFFECT_PROCESSOR_INVALID;
    }
    if ((family == GP100_EFFECT_CABINET ||
         (family == GP100_EFFECT_AMPLIFIER &&
          slot->algorithm_index >= 46u)) &&
        processor->asset_read == NULL) {
        ++processor->unsupported_blocks;
        return GP100_EFFECT_PROCESSOR_UNSUPPORTED;
    }
    parameter_count = gp100_effect_parameter_count(
        family, slot->algorithm_index);
    if (parameter_count == 0u || parameter_count > 9u) {
        ++processor->processing_errors;
        return GP100_EFFECT_PROCESSOR_ERROR;
    }
    for (index = 0u; index < parameter_count; ++index) {
        if (!parameter_value(family, slot, index, &parameter[index])) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    }
    family_bit = (uint16_t)(UINT16_C(1) << family);
    preset_recalled = (processor->recall_initialize_mask & family_bit) != 0u;
    processor->recall_initialize_mask &= (uint16_t)~family_bit;
    if (family == GP100_EFFECT_PRE_FX) {
        reset_required = preset_recalled || !processor->pre_fx_loaded ||
            processor->pre_fx_algorithm != slot->algorithm_index;
        if (reset_required && !reset_pre_fx(processor, slot, parameter)) {
            processor->pre_fx_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->pre_fx_algorithm = slot->algorithm_index;
            processor->pre_fx_loaded = true;
        }
        if (!prepare_only && !process_pre_fx(processor, slot, parameter,
                            interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_DISTORTION) {
        reset_required = preset_recalled || !processor->distortion_loaded ||
            processor->distortion_algorithm != slot->algorithm_index;
        if (reset_required && !reset_distortion(processor, slot, parameter)) {
            processor->distortion_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->distortion_algorithm = slot->algorithm_index;
            processor->distortion_loaded = true;
        }
        if (!prepare_only && !process_distortion(processor, slot, parameter,
                                interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_AMPLIFIER) {
        reset_required = !processor->amplifier_loaded ||
            processor->amplifier_algorithm != slot->algorithm_index;
        const bool reuse_asset = !reset_required && !slot->enabled;
        reset_required = reset_required || preset_recalled;
        if (reset_required &&
            !reset_amplifier(processor, slot, parameter, reuse_asset)) {
            processor->amplifier_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->amplifier_algorithm = slot->algorithm_index;
            processor->amplifier_loaded = true;
        }
        if (!prepare_only && !process_amplifier(processor, slot, parameter,
                               interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_NOISE_REDUCTION) {
        reset_required = preset_recalled || !processor->noise_reduction_loaded ||
            processor->noise_reduction_algorithm != slot->algorithm_index;
        if (reset_required &&
            !reset_noise_reduction(processor, slot, parameter)) {
            processor->noise_reduction_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->noise_reduction_algorithm = slot->algorithm_index;
            processor->noise_reduction_loaded = true;
        }
        if (!prepare_only && !process_noise_reduction(processor, slot, parameter,
                                     interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_CABINET) {
        reset_required = !processor->cabinet_loaded ||
            processor->cabinet_algorithm != slot->algorithm_index;
        if (reset_required || preset_recalled) {
            const bool loaded = !reset_required && !slot->enabled
                ? gp100_cabinet_load_record(
                    processor->cabinet, processor->cabinet_record,
                    sizeof(processor->cabinet_record))
                : load_cabinet(processor, slot->algorithm_index);
            if (!loaded) {
                processor->cabinet_loaded = false;
                ++processor->processing_errors;
                return GP100_EFFECT_PROCESSOR_ERROR;
            }
            processor->cabinet_algorithm = slot->algorithm_index;
            processor->cabinet_loaded = true;
        }
        if (!prepare_only && !gp100_cabinet_process(
                processor->cabinet, parameter[0], interleaved_stereo,
                frame_count, slot->enabled)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_EQUALIZER) {
        reset_required = preset_recalled || !processor->equalizer_loaded ||
            processor->equalizer_algorithm != slot->algorithm_index;
        if (reset_required && !reset_equalizer(processor, slot, parameter)) {
            processor->equalizer_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->equalizer_algorithm = slot->algorithm_index;
            processor->equalizer_loaded = true;
        }
        if (!prepare_only && !process_equalizer(processor, slot, parameter,
                               interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_MODULATION) {
        reset_required = preset_recalled || !processor->modulation_loaded ||
            processor->modulation_algorithm != slot->algorithm_index;
        if (reset_required && !reset_modulation(processor, slot, parameter)) {
            processor->modulation_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->modulation_algorithm = slot->algorithm_index;
            processor->modulation_loaded = true;
        }
        if (!prepare_only && !process_modulation(processor, slot, parameter,
                                interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else if (family == GP100_EFFECT_DELAY) {
        reset_required = preset_recalled || !processor->delay_loaded ||
            processor->delay_algorithm != slot->algorithm_index;
        if (reset_required && !reset_delay(processor, slot, parameter)) {
            processor->delay_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->delay_algorithm = slot->algorithm_index;
            processor->delay_loaded = true;
        }
        if (!prepare_only && !process_delay(processor, slot, parameter,
                           interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    } else {
        reset_required = preset_recalled || !processor->reverb_loaded ||
            processor->reverb_algorithm != slot->algorithm_index;
        if (reset_required && !reset_reverb(processor, slot, parameter)) {
            processor->reverb_loaded = false;
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
        if (reset_required) {
            processor->reverb_algorithm = slot->algorithm_index;
            processor->reverb_loaded = true;
        }
        if (!prepare_only && !process_reverb(processor, slot, parameter,
                            interleaved_stereo, frame_count)) {
            ++processor->processing_errors;
            return GP100_EFFECT_PROCESSOR_ERROR;
        }
    }
    if (prepare_only) {
        return GP100_EFFECT_PROCESSOR_PROCESSED;
    }
    if (!slot->enabled) {
        ++processor->bypassed_blocks;
        return GP100_EFFECT_PROCESSOR_BYPASSED;
    }
    ++processor->processed_blocks;
    return GP100_EFFECT_PROCESSOR_PROCESSED;
}

bool gp100_effect_processor_prepare(
    gp100_effect_processor_t *processor,
    const gp100_effect_runtime_t *effects)
{
    bool ready = true;

    if (processor == NULL || effects == NULL) {
        return false;
    }
    for (size_t family = 0u; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        const gp100_effect_slot_state_t *slot = &effects->slots[family];
        if (slot->algorithm_loaded &&
            process_slot(processor, (gp100_effect_family_t)family,
                         slot, NULL, 0u, true) !=
                GP100_EFFECT_PROCESSOR_PROCESSED) {
            ready = false;
        }
    }
    return ready;
}

bool gp100_effect_processor_prepare_slot(
    gp100_effect_processor_t *processor,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot)
{
    return process_slot(processor, family, slot, NULL, 0u, true) ==
           GP100_EFFECT_PROCESSOR_PROCESSED;
}

gp100_effect_processor_result_t gp100_effect_processor_process(
    gp100_effect_processor_t *processor,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count)
{
    return process_slot(processor, family, slot, interleaved_stereo,
                        frame_count, false);
}

void gp100_effect_processor_chain_callback(
    void *context,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count)
{
    (void)gp100_effect_processor_process(
        (gp100_effect_processor_t *)context, family, slot,
        interleaved_stereo, frame_count);
}

gp100_effect_chain_ops_t gp100_effect_processor_chain_ops(
    gp100_effect_processor_t *processor)
{
    gp100_effect_chain_ops_t ops;

    ops.context = processor;
    ops.process_effect = processor == NULL
        ? NULL : gp100_effect_processor_chain_callback;
    ops.post_process = NULL;
    return ops;
}
