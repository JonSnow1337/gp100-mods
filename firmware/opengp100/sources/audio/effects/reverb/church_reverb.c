#include "church_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t church_profile = {
    {2187.0f, 2401.0f, 3125.0f, 4096.0f},
    0.55f, 0.4f,
    -0.025f, 0.005f,
    0.16384723782539368f, 0.04487989470362663f,
    0.28495174646377563f,
    0.9f,
    0.015f, 1.0f,
    false, false, false, false, false, false, false
};

bool gp100_church_reverb_reset(gp100_church_reverb_t *reverb,
                                    uint8_t mix,
                                    uint8_t predelay,
                                    uint8_t decay,
                                    bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &church_profile, mix,
                                       predelay, decay, 0u, 0, 0, trail);
}

bool gp100_church_reverb_process(gp100_church_reverb_t *reverb,
                                      uint8_t mix,
                                      uint8_t predelay,
                                      uint8_t decay,
                                      bool trail,
                                      float *audio,
                                      size_t frames,
                                      bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &church_profile, mix,
                                         predelay, decay, 0u, 0, 0, trail,
                                         audio, frames, enabled);
}
