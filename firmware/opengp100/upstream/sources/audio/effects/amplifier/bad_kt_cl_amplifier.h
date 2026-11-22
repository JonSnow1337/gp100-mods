#ifndef GP100_BAD_KT_CL_AMPLIFIER_H
#define GP100_BAD_KT_CL_AMPLIFIER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_bad_kt_cl_filter
{
    float x[2], y[2];
} gp100_bad_kt_cl_filter_t;
typedef struct gp100_bad_kt_cl_amplifier
{
    gp100_bad_kt_cl_filter_t voicing[5], gain_filter, presence_filter;
    float fir_history[40], previous_nonlinear_input;
    float gain, gain_target, presence, presence_target, master, master_target;
    bool configured;
} gp100_bad_kt_cl_amplifier_t;

bool gp100_bad_kt_cl_amplifier_reset(
    gp100_bad_kt_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master);
bool gp100_bad_kt_cl_amplifier_process(
    gp100_bad_kt_cl_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, float *interleaved_stereo, size_t frame_count,
    bool enabled);
#endif
