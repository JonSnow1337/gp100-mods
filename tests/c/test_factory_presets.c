/* Host test: every record in a GPPR block written by gp100mods.presets must be accepted by the firmware's own parser and
 * pass OpenGP100's native-preset validation (blocks, CPU/memory budget, CRC).
 * Build: gcc -I<every dir under fw/sources> test_factory_presets.c <fw>/sources/app/factory_presets.c libgp100_core.a -lm
 * Usage: ./a.out block.bin -> "count N" then one line per slot: "slot I ok <blocks>" or "slot I rejected" */
#include "app/factory_presets.h"
#include "storage/patch_chain.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    static uint8_t buf[200000];
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : NULL;
    size_t n;
    uint16_t count, s;

    if (!f) { return 2; }
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    count = gp100_factory_presets_parse(buf, n);
    printf("count %u\n", count);
    for (s = 0; s < count; ++s) {
        gp100_preset_record_t r;
        if (!gp100_factory_presets_get(buf, n, s, &r)) { printf("slot %u rejected\n", s); continue; }
        printf("slot %u %s %zu\n", s, gp100_patch_is_native(&r) && gp100_patch_validate(&r) ? "ok" : "invalid", gp100_patch_count(&r));
    }
    return 0;
}
