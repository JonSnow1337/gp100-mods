#include "audio/engine/audio_engine.h"
#include "audio/effects/effect_processor.h"
#include "app/preset_runtime.h"
#include "tests/fixtures/flash_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_FRAMES 64u
#define BLOCKS_PER_PRESET 24u
#define SAMPLE_RATE 44100.0f
#define TWO_PI 6.283185307179586f

static bool read_flash(void *opaque, uint32_t offset, uint8_t *target, size_t size)
{
    FILE *flash = opaque;

    return fseek(flash, (long)offset, SEEK_SET) == 0 &&
           fread(target, 1u, size, flash) == size;
}

static void make_input(float *samples, size_t block)
{
    size_t frame;

    for (frame = 0u; frame < BLOCK_FRAMES; ++frame) {
        float time = (float)(block * BLOCK_FRAMES + frame) / SAMPLE_RATE;
        float sample = 0.17f * sinf(TWO_PI * 110.0f * time) +
                       0.10f * sinf(TWO_PI * 164.8138f * time) +
                       0.05f * sinf(TWO_PI * 220.0f * time);

        samples[frame * 2u] = sample;
        samples[frame * 2u + 1u] = sample;
    }
}

static bool play_factory_presets(FILE *flash)
{
    static gp100_effect_processor_t processor;
    gp100_effect_catalog_t catalog = gp100_effect_catalog_core();
    gp100_effect_runtime_t effects;
    gp100_effect_chain_t chain = {0};
    gp100_audio_engine_t engine;
    gp100_ui_runtime_t ui = {0};
    gp100_effect_chain_ops_t ops;
    float gate_detector[BLOCK_FRAMES * 2u];
    uint16_t logical;
    float maximum = 0.0f;

    static gp100_effect_processor_storage_t storage;
    gp100_effect_processor_reset(&processor);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_PRE_FX, &storage.pre_fx);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_DISTORTION, &storage.distortion);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_AMPLIFIER, &storage.amplifier);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_MODULATION, &storage.modulation);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_DELAY, &storage.delay);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_REVERB, &storage.reverb);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_NOISE_REDUCTION, &storage.noise_reduction);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_EQUALIZER, &storage.equalizer);
    gp100_effect_processor_bind(&processor, GP100_EFFECT_CABINET, &storage.cabinet);
    if (!gp100_effect_processor_set_asset_reader(&processor, read_flash, flash)) {
        return false;
    }
    gp100_effect_processor_set_gate_detector(&processor, gate_detector, BLOCK_FRAMES);
    ops = gp100_effect_processor_chain_ops(&processor);
    gp100_effect_runtime_reset(&effects);
    gp100_audio_engine_reset(&engine);
    engine.transition.mode = GP100_AUDIO_MODE_EFFECTS;
    engine.transition.target_mode = GP100_AUDIO_MODE_EFFECTS;
    for (logical = GP100_PRESETS_PER_BANK;
         logical < GP100_TOTAL_LOGICAL_PRESETS; ++logical) {
        gp100_preset_record_t preset;
        gp100_preset_recall_result_t recall;
        uint32_t offset = GP100_REFERENCE_FACTORY_PRESET_BASE +
                          (logical - GP100_PRESETS_PER_BANK) * GP100_PRESET_RECORD_SIZE;
        size_t block;

        if (!read_flash(flash, offset, preset.bytes, sizeof(preset.bytes)) ||
            gp100_preset_validate(&preset) != GP100_PRESET_VALID ||
            !gp100_preset_runtime_recall(logical, &catalog, NULL, &preset, &ui,
                                             &chain, &effects, &engine.output_level, &recall) ||
            !gp100_effect_processor_set_tempo_bpm(
                &processor, gp100_preset_tempo_bpm(&preset))) {
            fprintf(stderr, "Factory preset F%02u failed to recall\n",
                    (unsigned)(logical - GP100_PRESETS_PER_BANK + 1u));
            return false;
        }
        gp100_effect_processor_recall_preset(&processor, &effects);
        for (block = 0u; block < BLOCKS_PER_PRESET; ++block) {
            float samples[BLOCK_FRAMES * 2u];
            float scratch[BLOCK_FRAMES];
            size_t sample;

            make_input(samples, block);
            memcpy(gate_detector, samples, sizeof(gate_detector));
            if (gp100_audio_engine_process_block(
                    &engine, &chain, &effects, samples, BLOCK_FRAMES,
                    scratch, BLOCK_FRAMES, &ops) != GP100_AUDIO_BLOCK_EFFECTS ||
                processor.processing_errors != 0u || processor.unsupported_blocks != 0u) {
                fprintf(stderr, "Factory preset F%02u failed to process block %zu\n",
                        (unsigned)(logical - GP100_PRESETS_PER_BANK + 1u), block);
                return false;
            }
            for (sample = 0u; sample < BLOCK_FRAMES * 2u; ++sample) {
                if (!isfinite(samples[sample])) {
                    fprintf(stderr, "Factory preset F%02u returned a non-finite sample\n",
                            (unsigned)(logical - GP100_PRESETS_PER_BANK + 1u));
                    return false;
                }
                maximum = fmaxf(maximum, fabsf(samples[sample]));
            }
        }
    }
    printf("Processed 24 blocks on each of 99 factory presets. Peak %g.\n", maximum);
    return maximum > 0.0f;
}

int main(int argc, char **argv)
{
    FILE *flash = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool success;

    if (flash == NULL) {
        fprintf(stderr, "Provide the path to the GP-100 flash dump\n");
        return 1;
    }
    success = play_factory_presets(flash);
    fclose(flash);
    return success ? 0 : 1;
}
