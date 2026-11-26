#include "audio/effects/effect_chain.h"
#include "tests/fixtures/flash_layout.h"

#include <stdio.h>
#include <string.h>

typedef struct call_log {
    gp100_effect_family_t families[GP100_EFFECT_FAMILY_COUNT];
    size_t count;
    size_t post_count;
    size_t frame_count;
    float cabinet_delta;
} call_log_t;

static void record_post(void *context, float *samples, size_t frame_count);

static int read_factory_preset(const char *flash_path,
                               size_t preset_index,
                               gp100_preset_record_t *preset)
{
    FILE *file = fopen(flash_path, "rb");
    long offset = (long)(GP100_REFERENCE_FACTORY_PRESET_BASE +
                         preset_index * GP100_PRESET_RECORD_SIZE);

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 ||
        fread(preset->bytes, 1u, sizeof(preset->bytes), file) !=
            sizeof(preset->bytes)) {
        fclose(file);
        return 2;
    }
    fclose(file);
    return 0;
}

static void record_effect(void *context,
                          gp100_effect_family_t family,
                          const gp100_effect_slot_state_t *slot,
                          float *samples,
                          size_t frame_count)
{
    call_log_t *log = context;
    (void)slot;
    log->families[log->count++] = family;
    log->frame_count = frame_count;
    if (family == GP100_EFFECT_CABINET) {
        size_t sample;
        for (sample = 0u; sample < frame_count * 2u; ++sample) {
            samples[sample] += log->cabinet_delta;
        }
    }
}

static int test_cabinet_routing(const char *flash_path)
{
    gp100_preset_record_t preset;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime;
    gp100_effect_chain_ops_t ops;
    call_log_t log = {0};
    float samples[4];
    float scratch[2];

    if (read_factory_preset(flash_path, 23u, &preset) != 0 ||
        !gp100_effect_chain_from_preset(&chain, &preset)) {
        return 1;
    }
    gp100_effect_runtime_reset(&runtime);
    log.cabinet_delta = 10.0f;
    ops.context = &log;
    ops.process_effect = record_effect;
    ops.post_process = record_post;

    samples[0] = 1.0f;
    samples[1] = 2.0f;
    samples[2] = 3.0f;
    samples[3] = 4.0f;
    if (!gp100_effect_chain_process_routed_block(
            &chain, &runtime, GP100_CABINET_BYPASS_LEFT,
            samples, 2u, scratch, 2u, &ops) ||
        log.count != GP100_EFFECT_FAMILY_COUNT ||
        log.families[log.count - 1u] != GP100_EFFECT_CABINET ||
        samples[0] != 1.0f || samples[1] != 12.0f ||
        samples[2] != 3.0f || samples[3] != 14.0f ||
        log.post_count != 1u) {
        return 2;
    }

    memset(&log, 0, sizeof(log));
    log.cabinet_delta = 10.0f;
    samples[0] = 1.0f;
    samples[1] = 2.0f;
    samples[2] = 3.0f;
    samples[3] = 4.0f;
    if (!gp100_effect_chain_process_routed_block(
            &chain, &runtime, GP100_CABINET_BYPASS_RIGHT,
            samples, 2u, scratch, 2u, &ops) ||
        samples[0] != 11.0f || samples[1] != 2.0f ||
        samples[2] != 13.0f || samples[3] != 4.0f) {
        return 3;
    }

    memset(&log, 0, sizeof(log));
    log.cabinet_delta = 10.0f;
    if (!gp100_effect_chain_process_routed_block(
            &chain, &runtime, GP100_CABINET_BYPASS_BOTH,
            samples, 2u, NULL, 0u, &ops) ||
        log.count != GP100_EFFECT_FAMILY_COUNT - 1u) {
        return 4;
    }
    if (gp100_effect_chain_process_routed_block(
            &chain, &runtime, GP100_CABINET_BYPASS_LEFT,
            samples, 2u, scratch, 1u, &ops)) {
        return 5;
    }
    return 0;
}

static void record_post(void *context, float *samples, size_t frame_count)
{
    call_log_t *log = context;
    (void)samples;
    log->post_count++;
    log->frame_count = frame_count;
}

static int test_non_identity_chain(const char *flash_path)
{
    static const gp100_effect_family_t expected[] = {
        GP100_EFFECT_AMPLIFIER,
        GP100_EFFECT_PRE_FX,
        GP100_EFFECT_DISTORTION,
        GP100_EFFECT_NOISE_REDUCTION,
        GP100_EFFECT_CABINET,
        GP100_EFFECT_EQUALIZER,
        GP100_EFFECT_MODULATION,
        GP100_EFFECT_DELAY,
        GP100_EFFECT_REVERB
    };
    gp100_preset_record_t preset;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime;
    gp100_effect_chain_ops_t ops;
    call_log_t log = {0};
    float samples[16] = {0.0f};
    size_t index;
    size_t position;

    if (read_factory_preset(flash_path, 23u, &preset) != 0 ||
        !gp100_effect_chain_from_preset(&chain, &preset)) {
        return 1;
    }
    for (index = 0u; index < GP100_EFFECT_FAMILY_COUNT; ++index) {
        gp100_effect_family_t family;
        if (!gp100_effect_chain_family_at(&chain, index, &family) ||
            family != expected[index] ||
            !gp100_effect_chain_position_of(&chain, family,
                                                  &position) ||
            position != index) {
            return 2;
        }
    }

    gp100_effect_runtime_reset(&runtime);
    ops.context = &log;
    ops.process_effect = record_effect;
    ops.post_process = record_post;
    if (!gp100_effect_chain_process_block(
            &chain, &runtime, GP100_EFFECT_CHAIN_ALL_FAMILIES,
            samples, 8u, &ops) ||
        log.count != GP100_EFFECT_FAMILY_COUNT ||
        log.post_count != 1u || log.frame_count != 8u ||
        memcmp(log.families, expected, sizeof(expected)) != 0) {
        return 3;
    }

    memset(&log, 0, sizeof(log));
    if (!gp100_effect_chain_process_block(
            &chain, &runtime,
            (uint16_t)(GP100_EFFECT_CHAIN_ALL_FAMILIES &
                       ~(UINT16_C(1) << GP100_EFFECT_CABINET)),
            samples, 8u, &ops) ||
        log.count != GP100_EFFECT_FAMILY_COUNT - 1u ||
        log.families[0] != GP100_EFFECT_AMPLIFIER ||
        log.families[1] != GP100_EFFECT_PRE_FX ||
        log.families[3] != GP100_EFFECT_NOISE_REDUCTION ||
        log.families[4] != GP100_EFFECT_EQUALIZER ||
        log.post_count != 1u) {
        return 4;
    }
    return 0;
}

static int test_invalid_chain(const char *flash_path)
{
    gp100_preset_record_t preset;
    gp100_effect_chain_t chain;
    gp100_effect_chain_t unchanged;

    if (read_factory_preset(flash_path, 0u, &preset) != 0 ||
        !gp100_effect_chain_from_preset(&chain, &preset)) {
        return 1;
    }
    unchanged = chain;
    preset.bytes[0x10u] = preset.bytes[0x0eu];
    preset.bytes[0x11u] = preset.bytes[0x0fu];
    if (gp100_effect_chain_from_preset(&chain, &preset) ||
        memcmp(&chain, &unchanged, sizeof(chain)) != 0) {
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FLASH_DUMP\n", argv[0]);
        return 2;
    }
    result = test_non_identity_chain(argv[1]);
    if (result != 0) {
        fprintf(stderr, "non-identity chain test failed: %d\n", result);
        return 10 + result;
    }
    result = test_invalid_chain(argv[1]);
    if (result != 0) {
        fprintf(stderr, "invalid chain test failed: %d\n", result);
        return 20 + result;
    }
    result = test_cabinet_routing(argv[1]);
    if (result != 0) {
        fprintf(stderr, "cabinet routing test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
