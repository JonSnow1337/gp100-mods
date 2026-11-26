#include "deep_sea_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t deep_sea_profile = {
    {2048.0f, 2187.0f, 2401.0f, 3125.0f},
    0.4f, 0.5f,
    -0.15f, 0.025f,
    0.09723976999521255f, 0.05699034407734871f,
    0.7123792767524719f,
    0.75f,
    0.1f, 1.0f,
    false, false, false, false, false, false, false
};

bool gp100_deep_sea_reverb_reset(gp100_deep_sea_reverb_t *reverb,
                                      uint8_t mix,
                                      uint8_t decay,
                                      bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &deep_sea_profile, mix,
                                       75u, decay, 0u, 0, 0, trail);
}

bool gp100_deep_sea_reverb_process(
    gp100_deep_sea_reverb_t *reverb,
    uint8_t mix,
    uint8_t decay,
    bool trail,
    float *audio,
    size_t frames,
    bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &deep_sea_profile, mix,
                                         75u, decay, 0u, 0, 0, trail,
                                         audio, frames, enabled);
}
