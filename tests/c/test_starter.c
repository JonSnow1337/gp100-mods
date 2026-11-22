/* Host test for patch 0003: the built-in starter preset must pass OpenGP100's own chain validation, and the firmware's feature-flag
 * parser must agree with the Python writer.
 * Build: gcc -I<every dir under fw/sources> test_starter.c <fw>/sources/app/starter_preset.c <fw>/sources/app/feature_flags.c libgp100_core.a -lm
 * Usage: ./a.out            -> prints the starter chain
 *        ./a.out block.bin  -> prints "flags=N" for the 64-byte block in the file */
#include "app/feature_flags.h"
#include "app/starter_preset.h"
#include "audio/effects/effect_catalog.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc > 1) {
        uint8_t blk[64] = {0};
        size_t n;
        FILE *f = fopen(argv[1], "rb");
        if (!f) { return 2; }
        n = fread(blk, 1, sizeof blk, f);
        fclose(f);
        printf("flags=%u\n", gp100_feature_flags_parse(blk, n));
        return 0;
    }
    {
        gp100_preset_record_t p;
        size_t i;
        if (!gp100_make_starter_preset(&p) || !gp100_patch_validate(&p)) { puts("STARTER FAILED"); return 1; }
        printf("starter native=%d blocks=%zu\n", gp100_patch_is_native(&p), gp100_patch_count(&p));
        for (i = 0; i < gp100_patch_count(&p); ++i) {
            gp100_patch_block_t b;
            gp100_patch_block_at(&p, i, &b);
            printf("block %zu %s/%s enabled=%d fs=%u\n", i, gp100_effect_family_name(b.family),
                   gp100_effect_algorithm_name(b.family, b.algorithm), b.enabled, b.footswitches);
        }
    }
    return 0;
}
