#ifndef GP100_ACOUSTIC_SIMULATOR_H
#define GP100_ACOUSTIC_SIMULATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_AC_SIM_FILTER_COUNT 25u
#define GP100_AC_SIM_FILTER_ORDER 3u

typedef struct gp100_ac_sim_filter_state {
    float input[GP100_AC_SIM_FILTER_ORDER];
    float output[GP100_AC_SIM_FILTER_ORDER];
} gp100_ac_sim_filter_state_t;

typedef struct gp100_ac_sim {
    float body;
    float body_target;
    float top;
    float top_target;
    float volume;
    float volume_target;
    float mode_gain;
    float mode_fade_out_gain;
    gp100_ac_sim_filter_state_t filter[GP100_AC_SIM_FILTER_COUNT];
    uint8_t body_parameter;
    uint8_t top_parameter;
    uint8_t volume_parameter;
    uint8_t mode;
    uint8_t mode_target;
    bool fading_out;
    bool configured;
} gp100_ac_sim_t;

/* Body, Top, and Volume are 0..99; Mode is 0..3. */
bool gp100_ac_sim_reset(gp100_ac_sim_t *simulator,
                             uint8_t body,
                             uint8_t top,
                             uint8_t volume,
                             uint8_t mode);

bool gp100_ac_sim_process(gp100_ac_sim_t *simulator,
                               uint8_t body,
                               uint8_t top,
                               uint8_t volume,
                               uint8_t mode,
                               float *interleaved_stereo,
                               size_t frame_count,
                               bool enabled);

#ifdef __cplusplus
}
#endif

#endif
