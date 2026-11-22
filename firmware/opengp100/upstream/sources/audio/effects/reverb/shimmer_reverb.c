#include "shimmer_reverb.h"
#include "reverb_fdn_internal.h"

static const gp100_reverb_fdn_profile_t shimmer_profile = {
    {2048.0f, 2187.0f, 2401.0f, 3125.0f},
    0.45f, 0.5f,
    0.0f, 0.0f,
    0.42742758989334106f, 0.035618964582681656f,
    0.0f,
    0.8f,
    0.05f, 1.0f,
    false, false, false, true, true, true, true
};

bool gp100_shimmer_reverb_reset(
    gp100_shimmer_reverb_t *reverb,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    int8_t lo_end,
    int8_t hi_end,
    bool trail)
{
    return gp100_reverb_fdn_reset(reverb, &shimmer_profile, mix,
                                       predelay, decay, 0u, lo_end, hi_end,
                                       trail);
}

bool gp100_shimmer_reverb_process(
    gp100_shimmer_reverb_t *reverb,
    uint8_t mix,
    uint8_t predelay,
    uint8_t decay,
    int8_t lo_end,
    int8_t hi_end,
    bool trail,
    float *audio,
    size_t frames,
    bool enabled)
{
    return gp100_reverb_fdn_process(reverb, &shimmer_profile, mix,
                                         predelay, decay, 0u, lo_end, hi_end,
                                         trail, audio, frames, enabled);
}
