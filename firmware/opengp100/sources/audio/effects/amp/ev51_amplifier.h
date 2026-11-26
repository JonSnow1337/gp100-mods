#ifndef GP100_EV51_AMPLIFIER_H
#define GP100_EV51_AMPLIFIER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_ev51_filter
{
    float x[3];
    float y[3];
} gp100_ev51_filter_t;

typedef struct gp100_ev51_amplifier
{
    gp100_ev51_filter_t input_filter;
    gp100_ev51_filter_t voicing_filter;
    gp100_ev51_filter_t gain_filter;
    gp100_ev51_filter_t anti_alias_filter;
    gp100_ev51_filter_t tone_filter;
    gp100_ev51_filter_t presence_filter;
    float fir_history[40];
    float previous_nonlinear_input;
    float gain;
    float gain_target;
    float master;
    float master_target;
    float bass;
    float bass_target;
    float middle;
    float middle_target;
    float treble;
    float treble_target;
    float presence;
    float presence_target;
    bool configured;
} gp100_ev51_amplifier_t;

bool gp100_ev51_amplifier_reset(gp100_ev51_amplifier_t *amplifier,
                                     uint8_t gain, uint8_t master, uint8_t bass,
                                     uint8_t middle, uint8_t treble,
                                     uint8_t presence);

bool gp100_ev51_amplifier_process(gp100_ev51_amplifier_t *amplifier,
                                       uint8_t gain, uint8_t master,
                                       uint8_t bass, uint8_t middle,
                                       uint8_t treble, uint8_t presence,
                                       float *interleaved_stereo,
                                       size_t frame_count, bool enabled);

#endif
