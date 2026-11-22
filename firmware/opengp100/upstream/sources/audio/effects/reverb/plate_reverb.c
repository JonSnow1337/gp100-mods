#include "plate_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t plate_profile = {
    {2048.0f, 2401.0f, 3125.0f, 3481.0f},
    0.55f, 0.4f,
    0.0f, 0.0f,
    0.1727f, 0.0f,
    1.111311674118042f,
    0.9f,
    0.015f, 1.0f,
    true, false, false, false, false, false, false
};

bool gp100_plate_reverb_reset(gp100_plate_reverb_t *reverb,
                                   uint8_t mix,
                                   uint8_t decay,
                                   uint8_t damping,
                                   bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &plate_profile, mix,
                                       50u, decay, damping, 0, 0, trail);
}

bool gp100_plate_reverb_process(gp100_plate_reverb_t *reverb,
                                     uint8_t mix,
                                     uint8_t decay,
                                     uint8_t damping,
                                     bool trail,
                                     float *audio,
                                     size_t frames,
                                     bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &plate_profile, mix,
                                         50u, decay, damping, 0, 0, trail,
                                         audio, frames, enabled);
}
