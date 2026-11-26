#include "n_star_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t n_star_profile = {
    {2048.0f, 2187.0f, 2401.0f, 3125.0f},
    0.45f, 0.5f,
    0.005f, 0.0f,
    0.09723976999521255f, 0.05699034407734871f,
    1.1398069858551025f,
    0.73f,
    0.05f, 1.0f,
    false, false, false, false, false, false, false
};

bool gp100_n_star_reverb_reset(gp100_n_star_reverb_t *reverb,
                                    uint8_t mix,
                                    uint8_t decay,
                                    bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &n_star_profile, mix,
                                       75u, decay, 0u, 0, 0, trail);
}

bool gp100_n_star_reverb_process(gp100_n_star_reverb_t *reverb,
                                      uint8_t mix,
                                      uint8_t decay,
                                      bool trail,
                                      float *audio,
                                      size_t frames,
                                      bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &n_star_profile, mix,
                                         75u, decay, 0u, 0, 0, trail,
                                         audio, frames, enabled);
}
