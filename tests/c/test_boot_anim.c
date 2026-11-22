/* Host test: the firmware's GPBA parser/decoder (ui/boot_animation.c) must accept what the Python writer produces and
 * decode identical pixels. Build: gcc -I<fw>/sources/ui test_boot_anim.c <fw>/sources/ui/boot_animation.c
 * Usage: ./a.out file.gpba  -> prints "OK frames=N total_ms=M crc=C" where C = 32-bit sum over all decoded pixels. */
#include "boot_animation.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *f;
    static uint8_t area[GP100_BOOT_ANIM_AREA_BYTES], scratch[160 * 128];
    size_t n;
    gp100_boot_anim_t a;
    uint64_t crc = 0;
    uint32_t t = 0;
    uint16_t i;

    if (argc < 2 || !(f = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: test file.gpba\n"); return 2; }
    n = fread(area, 1, sizeof(area), f);
    fclose(f);
    if (!gp100_boot_anim_parse(area, n, 160, 128, &a)) { puts("REJECTED"); return 1; }
    for (i = 0; i < a.frame_count; ++i) {
        const uint8_t *px;
        size_t k;
        if (!gp100_boot_anim_frame_pixels(&a, i, scratch, &px)) { puts("DECODE FAIL"); return 1; }
        for (k = 0; k < 160u * 128u; ++k) { crc += (uint64_t)px[k] * (k % 251u + 1u) + i; }
        if (gp100_boot_anim_frame_at(&a, t) != i) { puts("FRAME_AT FAIL"); return 1; }
        t += ((uint32_t)a.table[i * 8u + 4] | ((uint32_t)a.table[i * 8u + 5] << 8));
    }
    if (gp100_boot_anim_frame_at(&a, a.total_ms) != a.frame_count) { puts("END FAIL"); return 1; }
    printf("OK frames=%u total_ms=%u crc=%llu\n", a.frame_count, a.total_ms, (unsigned long long)crc);
    return 0;
}
