#include "rhythm/rhythm_assets.h"

#include <stdio.h>
#include <stdlib.h>

static int test_factory_image(const char *path)
{
    gp100_rhythm_assets_t assets;
    uint8_t *image;
    long image_size;
    FILE *file = fopen(path, "rb");

    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (image_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return 1;
    }
    image = malloc((size_t)image_size);
    if (image == NULL ||
        fread(image, 1u, (size_t)image_size, file) != (size_t)image_size) {
        free(image);
        fclose(file);
        return 2;
    }
    fclose(file);

    if ((size_t)image_size <
            GP100_RHYTHM_PATTERN_BANK_OFFSET +
                GP100_RHYTHM_PATTERN_BANK_SIZE ||
        !gp100_rhythm_assets_parse(
            &assets,
            image + GP100_RHYTHM_PATTERN_BANK_OFFSET,
            GP100_RHYTHM_PATTERN_BANK_SIZE,
            image + GP100_RHYTHM_SAMPLE_BANK_OFFSET,
            GP100_RHYTHM_SAMPLE_BANK_SIZE) ||
        assets.patterns[0].events[0] != UINT16_C(0x885a) ||
        assets.patterns[99].events[0] != UINT16_C(0x8246) ||
        assets.samples[0].block_count != 143u ||
        assets.samples[12].block_count != 306u ||
        gp100_rhythm_assets_parse(
            &assets,
            image + GP100_RHYTHM_PATTERN_BANK_OFFSET,
            GP100_RHYTHM_PATTERN_BANK_SIZE - 1u,
            image + GP100_RHYTHM_SAMPLE_BANK_OFFSET,
            GP100_RHYTHM_SAMPLE_BANK_SIZE) ||
        gp100_rhythm_assets_parse(
            &assets,
            image + GP100_RHYTHM_PATTERN_BANK_OFFSET,
            GP100_RHYTHM_PATTERN_BANK_SIZE,
            image + GP100_RHYTHM_SAMPLE_BANK_OFFSET,
            GP100_RHYTHM_SAMPLE_BANK_SIZE - 1u)) {
        free(image);
        return 3;
    }
    free(image);
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s app_v210_sdram.bin\n", argv[0]);
        return 2;
    }
    result = test_factory_image(argv[1]);
    if (result != 0) {
        fprintf(stderr, "factory rhythm asset test failed: %d\n", result);
        return 10 + result;
    }
    return 0;
}
