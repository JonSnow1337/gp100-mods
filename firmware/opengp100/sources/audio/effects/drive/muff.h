#ifndef GP100_MUFF_H
#define GP100_MUFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_muff_stage {
    float input_cap;
    float feedback_cap;
    float diode_cap;
} gp100_muff_stage_t;

typedef struct gp100_muff {
    gp100_muff_stage_t stage[3];
    float decimator[40];
    float previous_input;
    float sustain_cap;
    float tone_high_cap;
    float tone_low_cap;
    float recovery_cap;
    float output_cap;
    float sustain;
    float tone;
    float volume;
    bool enabled;
} gp100_muff_t;

void gp100_muff_reset(gp100_muff_t *effect, const float controls[3]);
void gp100_muff_process(gp100_muff_t *effect, const float controls[3],
                             float *audio, size_t frames, bool enabled);

#endif
