#include "spring_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t spring_profile = {
    {2048.0f, 2187.0f, 3125.0f, 2401.0f},
    0.9f, 0.0f,
    0.0f, 0.0f,
    0.21371379494667053f, 0.21371379494667053f,
    0.31344690918922424f,
    1.0f,
    0.015f, 1.0f,
    false, true, true, false, false, false, false
};

bool gp100_spring_reverb_reset(gp100_spring_reverb_t *reverb,
                                    uint8_t mix,
                                    uint8_t tone,
                                    uint8_t decay,
                                    bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &spring_profile, mix,
                                       0u, decay, tone, 0, 0, trail);
}

bool gp100_spring_reverb_process(gp100_spring_reverb_t *reverb,
                                      uint8_t mix,
                                      uint8_t tone,
                                      uint8_t decay,
                                      bool trail,
                                      float *audio,
                                      size_t frames,
                                      bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &spring_profile, mix,
                                         0u, decay, tone, 0, 0, trail,
                                         audio, frames, enabled);
}
