#include "starter_preset.h"

#include "app/firmware_runtime.h"
#include "audio/effects/effect_catalog.h"

typedef struct {
    gp100_effect_family_t family;
    uint16_t algorithm;      /* index in the family's catalog */
    bool enabled;
    uint8_t footswitches;    /* bit n = footswitch n+1 toggles this block */
} starter_block_t;

static const starter_block_t k_chain[] = {
    {GP100_EFFECT_DISTORTION, 6u, true, 1u << 0},     /* Green OD   - FS1 */
    {GP100_EFFECT_AMPLIFIER, 1u, true, 0u},           /* Bellman 59N */
    {GP100_EFFECT_CABINET, 31u, true, 0u},            /* Bellman 4x10 */
    {GP100_EFFECT_DELAY, 6u, false, 1u << 1},         /* Slapback   - FS2, off until switched on */
    {GP100_EFFECT_REVERB, 4u, true, 0u},              /* Spring */
};

bool gp100_make_starter_preset(gp100_preset_record_t *preset)
{
    size_t i;

    if (preset == NULL || !gp100_make_development_preset(preset, "Starter")) { return false; }
    gp100_patch_init(preset);
    for (i = 0u; i < sizeof(k_chain) / sizeof(k_chain[0]); ++i) {
        gp100_patch_block_t block;

        if (gp100_patch_insert(preset, i, k_chain[i].family, k_chain[i].algorithm) != GP100_PATCH_OK ||
            !gp100_patch_block_at(preset, i, &block)) {
            return false;
        }
        block.enabled = k_chain[i].enabled;
        block.footswitches = k_chain[i].footswitches;
        if (!gp100_patch_update(preset, &block)) { return false; }
    }
    return gp100_patch_validate(preset) && gp100_patch_count(preset) == sizeof(k_chain) / sizeof(k_chain[0]);
}
