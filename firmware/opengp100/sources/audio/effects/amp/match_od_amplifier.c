#include "match_od_amplifier.h"
#include "match_family_internal.h"

static float gain_control(uint8_t gain)
{
    return .3f + .7f * gp100_match_family_gain_control(gain);
}

bool gp100_match_od_amplifier_reset(
    gp100_match_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble)
{
    if (amplifier == NULL || gain > 99u)
        return false;
    return gp100_match_family_reset(&amplifier->core, gain_control(gain),
                                         presence, master, bass, middle,
                                         treble);
}

bool gp100_match_od_amplifier_process(
    gp100_match_od_amplifier_t *amplifier, uint8_t gain, uint8_t presence,
    uint8_t master, uint8_t bass, uint8_t middle, uint8_t treble, float *audio,
    size_t frames, bool enabled)
{
    if (amplifier == NULL || gain > 99u)
        return false;
    return gp100_match_family_process(
        &amplifier->core, gain_control(gain), presence, master, bass, middle,
        treble, 200.0f, 15.0f * .1259f * 4.0f * 1.26f, audio, frames, enabled);
}
