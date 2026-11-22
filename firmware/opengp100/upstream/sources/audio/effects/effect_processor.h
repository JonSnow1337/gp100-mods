#ifndef GP100_EFFECT_PROCESSOR_H
#define GP100_EFFECT_PROCESSOR_H

#include "audio/effects/modulation/a_chorus.h"
#include "audio/effects/pre/acoustic_simulator.h"
#include "audio/effects/pre/auto_wah.h"
#include "audio/effects/modulation/b_chorus.h"
#include "audio/effects/modulation/bias_trem.h"
#include "booster.h"
#include "audio/effects/cabinet/cabinet.h"
#include "audio/effects/pre/classic_wah.h"
#include "audio/effects/reverb/church_reverb.h"
#include "audio/effects/reverb/clear_sky_reverb.h"
#include "audio/effects/reverb/deep_sea_reverb.h"
#include "audio/effects/pre/compressor.h"
#include "audio/effects/modulation/detune.h"
#include "audio/effects/delay/dual_echo.h"
#include "audio/effects/delay/echo_999.h"
#include "audio/effects/equalizer/equalizer.h"
#include "effect_chain.h"
#include "audio/effects/modulation/flanger.h"
#include "audio/effects/modulation/g_chorus.h"
#include "audio/effects/reverb/hall_reverb.h"
#include "audio/effects/pre/hammy.h"
#include "audio/effects/delay/m_echo.h"
#include "audio/effects/delay/m_echo2.h"
#include "audio/effects/reverb/mod_verb_reverb.h"
#include "audio/effects/reverb/n_star_reverb.h"
#include "noise_gate.h"
#include "audio/effects/pre/octaver.h"
#include "audio/effects/modulation/opto_trem.h"
#include "audio/effects/modulation/phaser.h"
#include "audio/effects/reverb/plate_reverb.h"
#include "audio/effects/pre/pitch_bender.h"
#include "audio/effects/pre/pitch_shifter.h"
#include "audio/effects/delay/p_echo.h"
#include "audio/effects/delay/ping_pong.h"
#include "audio/effects/delay/rev_echo.h"
#include "audio/effects/reverb/room_reverb.h"
#include "audio/effects/pre/ring_modulator.h"
#include "audio/effects/pre/saturate.h"
#include "audio/effects/reverb/shimmer_reverb.h"
#include "audio/effects/modulation/sine_trem.h"
#include "audio/effects/delay/slapback.h"
#include "audio/effects/reverb/spring_reverb.h"
#include "audio/effects/amplifier/snap_tone.h"
#include "audio/effects/pre/step_filter.h"
#include "audio/effects/delay/sweep_echo.h"
#include "audio/effects/delay/sweet_delay.h"
#include "audio/effects/delay/t_echo.h"
#include "audio/effects/pre/touch_wah.h"
#include "audio/effects/modulation/triangle_trem.h"
#include "audio/effects/modulation/vibe.h"
#include "audio/effects/modulation/vibrato.h"
#include "audio/effects/pre/vintage_wah.h"
#include "audio/effects/delay/vin_rack.h"
#include "audio/effects/distortion/bass_distortion.h"
#include "audio/effects/distortion/blues_overdrive.h"
#include "audio/effects/distortion/chief.h"
#include "audio/effects/distortion/darktale.h"
#include "audio/effects/distortion/flex_overdrive.h"
#include "audio/effects/distortion/green_overdrive.h"
#include "audio/effects/distortion/la_charger.h"
#include "audio/effects/distortion/lazaro.h"
#include "audio/effects/distortion/penesas.h"
#include "audio/effects/distortion/plustortion.h"
#include "audio/effects/distortion/red_haze.h"
#include "audio/effects/distortion/scream_overdrive.h"
#include "audio/effects/distortion/sm_distortion.h"
#include "audio/effects/distortion/super_overdrive.h"
#include "audio/effects/distortion/taichi_overdrive.h"
#include "audio/effects/distortion/tube_clipper.h"
#include "audio/effects/distortion/yellow_overdrive.h"
#include "audio/effects/amplifier/tweedy_amplifier.h"
#include "audio/effects/amplifier/bellman59n_amplifier.h"
#include "audio/effects/amplifier/dark_twin_amplifier.h"
#include "audio/effects/amplifier/lstar_cl_amplifier.h"
#include "audio/effects/amplifier/foxy30n_amplifier.h"
#include "audio/effects/amplifier/bogsv_cl_amplifier.h"
#include "audio/effects/amplifier/j120_cl_amplifier.h"
#include "audio/effects/amplifier/match_cl_amplifier.h"
#include "audio/effects/amplifier/z38_cl_amplifier.h"
#include "audio/effects/amplifier/knights_cl_amplifier.h"
#include "audio/effects/amplifier/bad_kt_cl_amplifier.h"
#include "audio/effects/amplifier/uk45_amplifier.h"
#include "audio/effects/amplifier/uk50jp_amplifier.h"
#include "audio/effects/amplifier/uk800_amplifier.h"
#include "audio/effects/amplifier/flagman_amplifier.h"
#include "audio/effects/amplifier/z38_od_amplifier.h"
#include "audio/effects/amplifier/bogsv_od_amplifier.h"
#include "audio/effects/amplifier/bellman59b_amplifier.h"
#include "audio/effects/amplifier/foxy30tb_amplifier.h"
#include "audio/effects/amplifier/supdual_od_amplifier.h"
#include "audio/effects/amplifier/match_od_amplifier.h"
#include "audio/effects/amplifier/solo100_od_amplifier.h"
#include "audio/effects/amplifier/lstar_od_amplifier.h"
#include "audio/effects/amplifier/bad_kt_od_amplifier.h"
#include "audio/effects/amplifier/mess2c1_amplifier.h"
#include "audio/effects/amplifier/mess2c2_amplifier.h"
#include "audio/effects/amplifier/knights_od_amplifier.h"
#include "audio/effects/amplifier/dizz_vh_amplifier.h"
#include "audio/effects/amplifier/eagle120_amplifier.h"
#include "audio/effects/amplifier/ev51_amplifier.h"
#include "audio/effects/amplifier/solo100_ld_amplifier.h"
#include "audio/effects/amplifier/mess4_ld_amplifier.h"
#include "audio/effects/amplifier/mess_dualv_amplifier.h"
#include "audio/effects/amplifier/power_ld_amplifier.h"
#include "audio/effects/amplifier/flagman_plus_amplifier.h"
#include "audio/effects/amplifier/juice_r100_amplifier.h"
#include "audio/effects/amplifier/mess_dualm_amplifier.h"
#include "audio/effects/amplifier/bog_bluev_amplifier.h"
#include "audio/effects/amplifier/bog_redm_amplifier.h"
#include "audio/effects/amplifier/uk900_amplifier.h"
#include "audio/effects/amplifier/classic_bass_amplifier.h"
#include "audio/effects/amplifier/bass_pre_amplifier.h"
#include "audio/effects/amplifier/mini_bass_amplifier.h"
#include "audio/effects/amplifier/foxy_bass_amplifier.h"
#include "audio/effects/amplifier/mess_bass_amplifier.h"
#include "audio/effects/amplifier/ac_pre_amplifier.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EFFECT_PROCESSOR_DEFAULT_TEMPO_BPM 120u
#define GP100_EFFECT_PROCESSOR_MODULATION_MASK \
    (UINT16_C(1) << GP100_EFFECT_MODULATION)
#define GP100_EFFECT_PROCESSOR_DELAY_MASK \
    (UINT16_C(1) << GP100_EFFECT_DELAY)
#define GP100_EFFECT_PROCESSOR_REVERB_MASK \
    (UINT16_C(1) << GP100_EFFECT_REVERB)
#define GP100_EFFECT_PROCESSOR_PRE_FX_MASK \
    (UINT16_C(1) << GP100_EFFECT_PRE_FX)
#define GP100_EFFECT_PROCESSOR_NOISE_REDUCTION_MASK \
    (UINT16_C(1) << GP100_EFFECT_NOISE_REDUCTION)
#define GP100_EFFECT_PROCESSOR_EQUALIZER_MASK \
    (UINT16_C(1) << GP100_EFFECT_EQUALIZER)
#define GP100_EFFECT_PROCESSOR_DISTORTION_MASK \
    (UINT16_C(1) << GP100_EFFECT_DISTORTION)
#define GP100_EFFECT_PROCESSOR_AMPLIFIER_MASK \
    (UINT16_C(1) << GP100_EFFECT_AMPLIFIER)
#define GP100_EFFECT_PROCESSOR_CABINET_MASK \
    (UINT16_C(1) << GP100_EFFECT_CABINET)

typedef bool (*gp100_effect_asset_read_fn)(
    void *context,
    uint32_t flash_offset,
    uint8_t *destination,
    size_t size);

typedef union gp100_pre_fx_processor_state {
    gp100_comp_t comp;
    gp100_comp4_t comp4;
    gp100_boost_t boost;
    gp100_boost14_t boost14;
    gp100_ac_sim_t ac_sim;
    gp100_touch_wah_t touch_wah;
    gp100_auto_wah_t auto_wah;
    gp100_vintage_wah_t vintage_wah;
    gp100_classic_wah_t classic_wah;
    gp100_octaver_t octaver;
    gp100_pitch_shifter_t pitch_shifter;
    gp100_pitch_bender_t pitch_bender;
    gp100_hammy_t hammy;
    gp100_saturate_t saturate;
    gp100_step_filter_t step_filter;
    gp100_ring_modulator_t ring_modulator;
} gp100_pre_fx_processor_state_t;

typedef union gp100_distortion_processor_state {
    gp100_penesas_t penesas;
    gp100_scream_overdrive_t scream_overdrive;
    gp100_tube_clipper_t tube_clipper;
    gp100_taichi_overdrive_t taichi_overdrive;
    gp100_plustortion_t plustortion;
    gp100_chief_t chief;
    gp100_green_overdrive_t green_overdrive;
    gp100_yellow_overdrive_t yellow_overdrive;
    gp100_super_overdrive_t super_overdrive;
    gp100_blues_overdrive_t blues_overdrive;
    gp100_lazaro_t lazaro;
    gp100_red_haze_t red_haze;
    gp100_darktale_t darktale;
    gp100_flex_overdrive_t flex_overdrive;
    gp100_sm_distortion_t sm_distortion;
    gp100_la_charger_t la_charger;
    gp100_bass_distortion_t bass_distortion;
    gp100_boost_t boost;
    gp100_boost14_t boost14;
} gp100_distortion_processor_state_t;

typedef union gp100_amplifier_processor_state {
    gp100_tweedy_amplifier_t tweedy;
    gp100_bellman59n_amplifier_t bellman59n;
    gp100_dark_twin_amplifier_t dark_twin;
    gp100_lstar_cl_amplifier_t lstar_cl;
    gp100_foxy30n_amplifier_t foxy30n;
    gp100_bogsv_cl_amplifier_t bogsv_cl;
    gp100_j120_cl_amplifier_t j120_cl;
    gp100_match_cl_amplifier_t match_cl;
    gp100_z38_cl_amplifier_t z38_cl;
    gp100_knights_cl_amplifier_t knights_cl;
    gp100_bad_kt_cl_amplifier_t bad_kt_cl;
    gp100_uk45_amplifier_t uk45;
    gp100_uk50jp_amplifier_t uk50jp;
    gp100_uk800_amplifier_t uk800;
    gp100_flagman_amplifier_t flagman;
    gp100_z38_od_amplifier_t z38_od;
    gp100_bogsv_od_amplifier_t bogsv_od;
    gp100_bellman59b_amplifier_t bellman59b;
    gp100_foxy30tb_amplifier_t foxy30tb;
    gp100_supdual_od_amplifier_t supdual_od;
    gp100_match_od_amplifier_t match_od;
    gp100_solo100_od_amplifier_t solo100_od;
    gp100_lstar_od_amplifier_t lstar_od;
    gp100_bad_kt_od_amplifier_t bad_kt_od;
    gp100_mess2c1_amplifier_t mess2c1;
    gp100_mess2c2_amplifier_t mess2c2;
    gp100_knights_od_amplifier_t knights_od;
    gp100_dizz_vh_amplifier_t dizz_vh;
    gp100_eagle120_amplifier_t eagle120;
    gp100_ev51_amplifier_t ev51;
    gp100_solo100_ld_amplifier_t solo100_ld;
    gp100_mess4_ld_amplifier_t mess4_ld;
    gp100_mess_dualv_amplifier_t mess_dualv;
    gp100_power_ld_amplifier_t power_ld;
    gp100_flagman_plus_amplifier_t flagman_plus;
    gp100_juice_r100_amplifier_t juice_r100;
    gp100_mess_dualm_amplifier_t mess_dualm;
    gp100_bog_bluev_amplifier_t bog_bluev;
    gp100_bog_redm_amplifier_t bog_redm;
    gp100_uk900_amplifier_t uk900;
    gp100_classic_bass_amplifier_t classic_bass;
    gp100_bass_pre_amplifier_t bass_pre;
    gp100_mini_bass_amplifier_t mini_bass;
    gp100_foxy_bass_amplifier_t foxy_bass;
    gp100_mess_bass_amplifier_t mess_bass;
    gp100_ac_pre_amplifier_t ac_pre;
    gp100_snap_tone_t snap_tone;
} gp100_amplifier_processor_state_t;

typedef union gp100_modulation_processor_state {
    gp100_a_chorus_t a_chorus;
    gp100_g_chorus_t g_chorus;
    gp100_b_chorus_t b_chorus;
    gp100_detune_t detune;
    gp100_flanger_t flanger;
    gp100_vibrato_t vibrato;
    gp100_phaser_t phaser;
    gp100_vibe_t vibe;
    gp100_opto_trem_t opto_trem;
    gp100_sine_trem_t sine_trem;
    gp100_triangle_trem_t triangle_trem;
    gp100_bias_trem_t bias_trem;
} gp100_modulation_processor_state_t;

typedef union gp100_delay_processor_state {
    gp100_sweet_delay_t sweet;
    gp100_p_echo_t p_echo;
    gp100_m_echo_t m_echo;
    gp100_t_echo_t t_echo;
    gp100_echo_999_t echo_999;
    gp100_rev_echo_t rev_echo;
    gp100_slapback_t slapback;
    gp100_vin_rack_t vin_rack;
    gp100_sweep_echo_t sweep_echo;
    gp100_ping_pong_t ping_pong;
    gp100_m_echo2_t m_echo2;
    gp100_dual_echo_t dual_echo;
} gp100_delay_processor_state_t;

typedef struct gp100_effect_processor_storage {
    gp100_pre_fx_processor_state_t pre_fx;
    gp100_distortion_processor_state_t distortion;
    gp100_amplifier_processor_state_t amplifier;
    gp100_modulation_processor_state_t modulation;
    gp100_delay_processor_state_t delay;
    gp100_room_reverb_t reverb;
    gp100_noise_gate_t noise_reduction;
    gp100_equalizer_t equalizer;
    gp100_cabinet_t cabinet;
} gp100_effect_processor_storage_t;

typedef struct gp100_effect_processor {
    gp100_pre_fx_processor_state_t *pre_fx;
    gp100_distortion_processor_state_t *distortion;
    gp100_amplifier_processor_state_t *amplifier;
    gp100_modulation_processor_state_t *modulation;
    gp100_delay_processor_state_t *delay;
    gp100_room_reverb_t *reverb;
    gp100_noise_gate_t *noise_reduction;
    gp100_equalizer_t *equalizer;
    gp100_cabinet_t *cabinet;
    uint16_t bound_families;
    uint8_t cabinet_record[GP100_CABINET_RECORD_SIZE];
    uint8_t user_amp_blob[GP100_SYSEX_USER_AMP_BLOB_BYTES];
    gp100_effect_asset_read_fn asset_read;
    void *asset_context;
    const float *gate_detector;
    size_t gate_detector_frames;
    uint16_t tempo_period_ms;
    uint16_t recall_initialize_mask;
    uint16_t pre_fx_algorithm;
    uint16_t distortion_algorithm;
    uint16_t amplifier_algorithm;
    uint16_t modulation_algorithm;
    uint16_t delay_algorithm;
    uint16_t reverb_algorithm;
    uint16_t noise_reduction_algorithm;
    uint16_t equalizer_algorithm;
    uint16_t cabinet_algorithm;
    bool modulation_loaded;
    bool pre_fx_loaded;
    bool distortion_loaded;
    bool amplifier_loaded;
    bool delay_loaded;
    bool reverb_loaded;
    bool noise_reduction_loaded;
    bool equalizer_loaded;
    bool cabinet_loaded;
    uint32_t processed_blocks;
    uint32_t bypassed_blocks;
    uint32_t unsupported_blocks;
    uint32_t processing_errors;
} gp100_effect_processor_t;

typedef enum gp100_effect_processor_result {
    GP100_EFFECT_PROCESSOR_INVALID = -1,
    GP100_EFFECT_PROCESSOR_BYPASSED = 0,
    GP100_EFFECT_PROCESSOR_PROCESSED = 1,
    GP100_EFFECT_PROCESSOR_UNSUPPORTED = 2,
    GP100_EFFECT_PROCESSOR_ERROR = 3
} gp100_effect_processor_result_t;

void gp100_effect_processor_reset(
    gp100_effect_processor_t *processor);

/* Attach caller-owned state for one family. Each native block binds
 * independent state, even when several blocks use the same family. */
size_t gp100_effect_processor_state_size(gp100_effect_family_t family);
bool gp100_effect_processor_bind(gp100_effect_processor_t *processor,
                                 gp100_effect_family_t family,
                                 void *state);
bool gp100_effect_processor_prepare_slot(
    gp100_effect_processor_t *processor,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot);

bool gp100_effect_processor_set_tempo_bpm(
    gp100_effect_processor_t *processor,
    uint16_t tempo_bpm);

/* Installs the bounded reader used for flash-backed cabinet and user-AMP
 * assets. Changing the reader invalidates any selected flash-backed effect. */
bool gp100_effect_processor_set_asset_reader(
    gp100_effect_processor_t *processor,
    gp100_effect_asset_read_fn read_asset,
    void *context);

/* Bind a caller-owned copy of the input before the ordered effects run.
 * Keep it valid while processing. A NULL buffer restores detection from
 * the block passed to the noise gate. */
void gp100_effect_processor_set_gate_detector(
    gp100_effect_processor_t *processor,
    const float *interleaved_stereo,
    size_t frame_count);

/* Reinitialize each loaded slot at its next processing block. Enabled CAB
 * and user AMP slots refresh their flash assets. */
void gp100_effect_processor_recall_preset(
    gp100_effect_processor_t *processor,
    const gp100_effect_runtime_t *effects);

/* Invalidates a rewritten cabinet only when it is currently selected. The
 * next process call reloads the record and starts with cleared FIR history. */
bool gp100_effect_processor_invalidate_cabinet(
    gp100_effect_processor_t *processor,
    uint16_t cabinet_index);

/* Invalidates a rewritten user AMP only when it is currently selected. The
 * next process call reloads its descriptor and VTSI model from flash. */
bool gp100_effect_processor_invalidate_user_amp(
    gp100_effect_processor_t *processor,
    uint8_t user_index);

uint16_t gp100_effect_processor_supported_families(void);

/* Initialize loaded slots without rendering samples or advancing their state.
 * Call with exclusive access to the processor before starting audio. */
bool gp100_effect_processor_prepare(
    gp100_effect_processor_t *processor,
    const gp100_effect_runtime_t *effects);

gp100_effect_processor_result_t gp100_effect_processor_process(
    gp100_effect_processor_t *processor,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count);

/* Adapter for gp100_effect_chain_ops_t. Errors are retained in the
 * processor counters because the recovered chain callback has a void ABI. */
void gp100_effect_processor_chain_callback(
    void *context,
    gp100_effect_family_t family,
    const gp100_effect_slot_state_t *slot,
    float *interleaved_stereo,
    size_t frame_count);

gp100_effect_chain_ops_t gp100_effect_processor_chain_ops(
    gp100_effect_processor_t *processor);

#ifdef __cplusplus
}
#endif

#endif
