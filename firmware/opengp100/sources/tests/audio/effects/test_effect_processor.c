#include "audio/effects/effect_processor.h"

#include "audio/effects/effect_catalog.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static void load_defaults(gp100_effect_family_t family,
                          gp100_effect_slot_state_t *slot,
                          size_t algorithm)
{
    size_t count = gp100_effect_parameter_count(
        family, algorithm);
    size_t index;

    memset(slot, 0, sizeof(*slot));
    slot->enabled = true;
    slot->algorithm_loaded = true;
    slot->algorithm_index = (uint16_t)algorithm;
    for (index = 0u; index < count; ++index) {
        gp100_parameter_rule_t rule;
        assert(gp100_effect_parameter_rule(
            family, algorithm, index,
            slot->parameters, &rule));
        slot->parameters[rule.storage_slot] = rule.default_value;
    }
}

static void fill_signal(float *samples, size_t frames)
{
    size_t index;
    for (index = 0u; index < frames; ++index) {
        samples[index * 2u] = 0.2f + (float)index * 0.001f;
        samples[index * 2u + 1u] = -0.1f - (float)index * 0.0005f;
    }
}

typedef struct test_asset_reader {
    size_t cabinet_reads;
    size_t user_amp_descriptor_reads;
    size_t user_amp_blob_reads;
    uint32_t last_offset;
    bool user_amp_present;
    bool fail;
} test_asset_reader_t;

static void store_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
    output[2] = (uint8_t)(value >> 16u);
    output[3] = (uint8_t)(value >> 24u);
}

static void store_f32_le(uint8_t *output, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    store_u32_le(output, bits);
}

static void store_f64_le(uint8_t *output, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    store_u32_le(output, (uint32_t)bits);
    store_u32_le(output + 4u, (uint32_t)(bits >> 32u));
}

static void build_test_user_amp_blob(uint8_t *destination, size_t size)
{
    uint16_t crc16;

    assert(size == GP100_SYSEX_USER_AMP_BLOB_BYTES);
    memset(destination, 0, size);
    store_u32_le(destination, GP100_USER_AMP_MODEL_MAGIC);
    store_u32_le(destination + 0x04u, 0x1288u);
    store_u32_le(destination + 0x14u, 0x1200u);
    store_f64_le(destination + 0x18u, 1.0);
    store_f64_le(destination + 0x40u, 1.0);
    store_f32_le(destination + 0x68u, 1.0f);
    store_f32_le(destination + 0x6cu, 1.0f);
    store_f32_le(destination + 0x70u, 1.0f);
    store_f32_le(destination + 0x74u, 1.0f);
    store_u32_le(destination + 0x78u, 0u);
    store_u32_le(destination + 0x7cu, 128u);
    store_u32_le(destination + 0x80u, 128u);
    store_u32_le(destination + 0x84u, 1024u);
    store_f32_le(destination + GP100_USER_AMP_MODEL_HEADER_BYTES,
                 1.0f);
    store_f32_le(destination + GP100_USER_AMP_MODEL_HEADER_BYTES +
                 128u * sizeof(float), 1.0f);
    assert(gp100_user_amp_model_crc16(destination, size, &crc16));
    destination[0x08u] = (uint8_t)(crc16 >> 8u);
    destination[0x09u] = (uint8_t)crc16;
}

static bool read_test_asset(void *context, uint32_t flash_offset,
                            uint8_t *destination, size_t size)
{
    test_asset_reader_t *reader = (test_asset_reader_t *)context;

    uint8_t user_index;

    if (reader == NULL || destination == NULL || reader->fail) {
        return false;
    }
    if (size == GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES &&
        flash_offset >= GP100_USER_AMP_DESCRIPTOR_FLASH_BASE &&
        flash_offset < GP100_USER_AMP_DESCRIPTOR_FLASH_BASE +
            GP100_SYSEX_USER_AMP_SLOT_COUNT * size) {
        uint8_t metadata[GP100_SYSEX_USER_AMP_METADATA_BYTES] = {0};
        user_index = (uint8_t)((flash_offset -
            GP100_USER_AMP_DESCRIPTOR_FLASH_BASE) / size);
        memcpy(metadata, "Test user AMP", 13u);
        assert(gp100_user_amp_build_descriptor_record(
            user_index, reader->user_amp_present, metadata, destination));
        ++reader->user_amp_descriptor_reads;
        reader->last_offset = flash_offset;
        return true;
    }
    if (size == GP100_SYSEX_USER_AMP_BLOB_BYTES &&
        flash_offset >= GP100_USER_AMP_BLOB_FLASH_BASE &&
        flash_offset < GP100_USER_AMP_BLOB_FLASH_BASE +
            GP100_SYSEX_USER_AMP_SLOT_COUNT * size) {
        build_test_user_amp_blob(destination, size);
        ++reader->user_amp_blob_reads;
        reader->last_offset = flash_offset;
        return true;
    }
    if (size != GP100_CABINET_RECORD_SIZE) {
        return false;
    }
    memset(destination, 0, size);
    memcpy(destination, "Test cabinet", 12u);
    /* One Q23 identity tap at record offset 0x20. */
    destination[GP100_CABINET_NAME_SIZE +
                GP100_CABINET_RESERVED_SIZE + 2u] = 0x80u;
    reader->last_offset = flash_offset;
    ++reader->cabinet_reads;
    return true;
}

int main(void)
{
    gp100_effect_processor_t processor;
    gp100_effect_slot_state_t slot;
    gp100_effect_chain_ops_t ops;
    float samples[64u * 2u];
    float before[64u * 2u];
    test_asset_reader_t asset_reader = {.user_amp_present = true};
    size_t algorithm;

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
    assert(processor.tempo_period_ms == 500u);
    assert(gp100_effect_processor_supported_families() ==
           (GP100_EFFECT_PROCESSOR_PRE_FX_MASK |
            GP100_EFFECT_PROCESSOR_DISTORTION_MASK |
            GP100_EFFECT_PROCESSOR_AMPLIFIER_MASK |
            GP100_EFFECT_PROCESSOR_NOISE_REDUCTION_MASK |
            GP100_EFFECT_PROCESSOR_CABINET_MASK |
            GP100_EFFECT_PROCESSOR_EQUALIZER_MASK |
            GP100_EFFECT_PROCESSOR_MODULATION_MASK |
            GP100_EFFECT_PROCESSOR_DELAY_MASK |
            GP100_EFFECT_PROCESSOR_REVERB_MASK));
    assert(!gp100_effect_processor_set_tempo_bpm(&processor, 39u));
    assert(gp100_effect_processor_set_tempo_bpm(&processor, 123u));
    assert(processor.tempo_period_ms == 488u);
    assert(gp100_effect_processor_set_tempo_bpm(&processor, 120u));

    ops = gp100_effect_processor_chain_ops(&processor);
    assert(ops.context == &processor);
    assert(ops.process_effect != NULL);
    assert(ops.post_process == NULL);

    memset(&slot, 0, sizeof(slot));
    fill_signal(samples, 64u);
    memcpy(before, samples, sizeof(samples));
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_MODULATION, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
    assert(memcmp(samples, before, sizeof(samples)) == 0);

    load_defaults(GP100_EFFECT_CABINET, &slot, 0u);
    fill_signal(samples, 64u);
    memcpy(before, samples, sizeof(samples));
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_CABINET, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_UNSUPPORTED);
    assert(memcmp(samples, before, sizeof(samples)) == 0);

    load_defaults(GP100_EFFECT_AMPLIFIER, &slot, 46u);
    fill_signal(samples, 64u);
    memcpy(before, samples, sizeof(samples));
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_AMPLIFIER, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_UNSUPPORTED);
    assert(memcmp(samples, before, sizeof(samples)) == 0);
    assert(!gp100_effect_processor_set_asset_reader(
        NULL, read_test_asset, &asset_reader));
    assert(gp100_effect_processor_set_asset_reader(
        &processor, read_test_asset, &asset_reader));

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_PRE_FX);
         ++algorithm) {
        load_defaults(GP100_EFFECT_PRE_FX, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_PRE_FX, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.pre_fx_loaded);
        assert(processor.pre_fx_algorithm == algorithm);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_PRE_FX, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        if (algorithm == GP100_PRE_FX_LIMITTER) {
            /* Let the lookahead crossfade finish, then check settled bypass. */
            for (size_t block = 0u; block < 2u; ++block) {
                memcpy(samples, before, sizeof(samples));
                assert(gp100_effect_processor_process(
                           &processor, GP100_EFFECT_PRE_FX, &slot,
                           samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
            }
        }
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_DISTORTION);
         ++algorithm) {
        load_defaults(GP100_EFFECT_DISTORTION, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_DISTORTION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.distortion_algorithm == algorithm);
        slot.enabled = false;
        memcpy(before, samples, sizeof(samples));
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_DISTORTION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    /* The final five catalog slots load the shared SnapTone model core. */
    for (algorithm = 0u; algorithm < 46u; ++algorithm) {
        load_defaults(GP100_EFFECT_AMPLIFIER, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.amplifier_loaded);
        assert(processor.amplifier_algorithm == algorithm);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }
    for (algorithm = 46u; algorithm < 51u; ++algorithm) {
        uint32_t descriptor_offset;
        uint32_t blob_offset;

        load_defaults(GP100_EFFECT_AMPLIFIER, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.amplifier_loaded);
        assert(processor.amplifier_algorithm == algorithm);
        assert(processor.amplifier->snap_tone.loaded);
        assert(gp100_user_amp_flash_offsets(
            (uint8_t)(algorithm - 46u), &descriptor_offset, &blob_offset));
        assert(asset_reader.last_offset == blob_offset);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
    }
    assert(asset_reader.user_amp_descriptor_reads == 5u);
    assert(asset_reader.user_amp_blob_reads == 5u);
    assert(gp100_effect_processor_invalidate_user_amp(&processor, 3u));
    assert(processor.amplifier_loaded);
    assert(gp100_effect_processor_invalidate_user_amp(&processor, 4u));
    assert(!processor.amplifier_loaded);
    slot.enabled = true;
    fill_signal(samples, 64u);
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_AMPLIFIER, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(asset_reader.user_amp_descriptor_reads == 6u);
    assert(asset_reader.user_amp_blob_reads == 6u);
    assert(!gp100_effect_processor_invalidate_user_amp(NULL, 0u));
    assert(!gp100_effect_processor_invalidate_user_amp(
        &processor, GP100_SYSEX_USER_AMP_SLOT_COUNT));

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_CABINET);
         ++algorithm) {
        uint32_t expected_offset;

        load_defaults(GP100_EFFECT_CABINET, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_CABINET, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.cabinet_loaded);
        assert(processor.cabinet_algorithm == algorithm);
        assert(strcmp(processor.cabinet->name, "Test cabinet") == 0);
        assert(gp100_cabinet_flash_offset(
            algorithm, &expected_offset));
        assert(asset_reader.last_offset == expected_offset);
        assert(asset_reader.cabinet_reads == algorithm + 1u);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_CABINET, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
        assert(asset_reader.cabinet_reads == algorithm + 1u);
    }
    assert(processor.cabinet_loaded && processor.cabinet_algorithm == 59u);
    assert(gp100_effect_processor_invalidate_cabinet(
        &processor, 58u));
    assert(processor.cabinet_loaded);
    assert(gp100_effect_processor_invalidate_cabinet(
        &processor, 59u));
    assert(!processor.cabinet_loaded && !processor.cabinet->configured);
    slot.enabled = true;
    fill_signal(samples, 64u);
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_CABINET, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(processor.cabinet_loaded && asset_reader.cabinet_reads == 61u);
    assert(!gp100_effect_processor_invalidate_cabinet(NULL, 59u));
    assert(!gp100_effect_processor_invalidate_cabinet(
        &processor, GP100_CABINET_COUNT));

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_MODULATION);
         ++algorithm) {
        load_defaults(GP100_EFFECT_MODULATION, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_MODULATION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.modulation_loaded);
        assert(processor.modulation_algorithm == algorithm);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_MODULATION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_NOISE_REDUCTION);
         ++algorithm) {
        load_defaults(GP100_EFFECT_NOISE_REDUCTION, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_NOISE_REDUCTION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.noise_reduction_algorithm == algorithm);
        slot.enabled = false;
        memcpy(before, samples, sizeof(samples));
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_NOISE_REDUCTION, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    {
        float detector[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        float wet[4] = {1.0f, 1.0f, 1.0f, 0.0f};

        load_defaults(GP100_EFFECT_NOISE_REDUCTION, &slot, 0u);
        slot.parameters[0] = 80.0f;
        gp100_effect_processor_set_gate_detector(&processor, detector, 2u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_NOISE_REDUCTION, &slot,
                   wet, 2u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(wet[0] == 0.0f && wet[1] == 0.0f);
        assert(wet[2] > 0.0f && wet[2] == wet[3]);
        gp100_effect_processor_set_gate_detector(&processor, NULL, 0u);
    }

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_EQUALIZER);
         ++algorithm) {
        load_defaults(GP100_EFFECT_EQUALIZER, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_EQUALIZER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.equalizer_algorithm == algorithm);
        slot.enabled = false;
        memcpy(before, samples, sizeof(samples));
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_EQUALIZER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_DELAY);
         ++algorithm) {
        load_defaults(GP100_EFFECT_DELAY, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_DELAY, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.delay_loaded);
        assert(processor.delay_algorithm == algorithm);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_DELAY, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }

    for (algorithm = 0u;
         algorithm < gp100_effect_algorithm_count(
             GP100_EFFECT_REVERB);
         ++algorithm) {
        load_defaults(GP100_EFFECT_REVERB, &slot, algorithm);
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_REVERB, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(processor.reverb_loaded);
        assert(processor.reverb_algorithm == algorithm);
        assert(isfinite(samples[0]));
        assert(isfinite(samples[127]));

        memcpy(before, samples, sizeof(samples));
        slot.enabled = false;
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_REVERB, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(memcmp(samples, before, sizeof(samples)) == 0);
    }
    assert(processor.processed_blocks == GP100_TOTAL_EFFECT_ALGORITHMS + 3u);
    assert(processor.bypassed_blocks == GP100_TOTAL_EFFECT_ALGORITHMS + 3u);
    assert(processor.unsupported_blocks == 2u);
    assert(processor.processing_errors == 0u);

    asset_reader.user_amp_present = false;
    assert(gp100_effect_processor_invalidate_user_amp(&processor, 4u));
    load_defaults(GP100_EFFECT_AMPLIFIER, &slot, 50u);
    {
        const unsigned blob_reads = asset_reader.user_amp_blob_reads;
        fill_signal(samples, 64u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, &slot,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        for (size_t sample = 0u; sample < 128u; ++sample) {
            assert(samples[sample] == 0.0f);
        }
        assert(processor.amplifier_loaded);
        assert(!processor.amplifier->snap_tone.loaded);
        assert(asset_reader.user_amp_blob_reads == blob_reads);
        assert(processor.processing_errors == 0u);
    }

    asset_reader.user_amp_present = true;
    assert(gp100_effect_processor_invalidate_user_amp(&processor, 4u));
    asset_reader.fail = true;
    load_defaults(GP100_EFFECT_CABINET, &slot, 0u);
    fill_signal(samples, 64u);
    memcpy(before, samples, sizeof(samples));
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_CABINET, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_ERROR);
    assert(memcmp(samples, before, sizeof(samples)) == 0);
    assert(!processor.cabinet_loaded);
    assert(processor.processing_errors == 1u);

    {
        gp100_effect_runtime_t recalled = {0};
        gp100_effect_slot_state_t *amp =
            &recalled.slots[GP100_EFFECT_AMPLIFIER];
        gp100_effect_slot_state_t *cab =
            &recalled.slots[GP100_EFFECT_CABINET];
        unsigned amp_reads;
        unsigned cab_reads;
        const uint16_t recall_mask = GP100_EFFECT_PROCESSOR_AMPLIFIER_MASK |
            GP100_EFFECT_PROCESSOR_CABINET_MASK;

        asset_reader.fail = false;
        load_defaults(GP100_EFFECT_AMPLIFIER, amp, 50u);
        load_defaults(GP100_EFFECT_CABINET, cab, 0u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, amp,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_CABINET, cab,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        amp_reads = asset_reader.user_amp_blob_reads;
        cab_reads = asset_reader.cabinet_reads;
        cab->enabled = false;
        gp100_effect_processor_recall_preset(&processor, &recalled);
        assert(processor.recall_initialize_mask == recall_mask);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_CABINET, cab,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_BYPASSED);
        assert(asset_reader.cabinet_reads == cab_reads);
        cab->enabled = true;
        gp100_effect_processor_recall_preset(&processor, &recalled);
        assert(processor.recall_initialize_mask == recall_mask);
        const uint32_t processed_before_prepare = processor.processed_blocks;
        const uint32_t bypassed_before_prepare = processor.bypassed_blocks;
        assert(gp100_effect_processor_prepare(&processor, &recalled));
        assert(processor.recall_initialize_mask == 0u);
        assert(processor.processed_blocks == processed_before_prepare);
        assert(processor.bypassed_blocks == bypassed_before_prepare);
        assert(asset_reader.user_amp_blob_reads == amp_reads + 1u);
        assert(asset_reader.cabinet_reads == cab_reads + 1u);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_AMPLIFIER, amp,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(gp100_effect_processor_process(
                   &processor, GP100_EFFECT_CABINET, cab,
                   samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
        assert(asset_reader.user_amp_blob_reads == amp_reads + 1u);
        assert(asset_reader.cabinet_reads == cab_reads + 1u);
        assert(processor.recall_initialize_mask == 0u);
    }

    /* Swp Echo's editor dependencies and its DSP sync inputs are crossed. */
    load_defaults(GP100_EFFECT_DELAY, &slot, 8u);
    slot.parameters[5] = 1.0f;
    slot.parameters[4] = 4.0f;
    fill_signal(samples, 64u);
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_DELAY, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(processor.delay->sweep_echo.time_sync);
    assert(!processor.delay->sweep_echo.sweep_sync);
    assert(processor.delay->sweep_echo.target_delay_samples == 22050.0f);

    slot.parameters[5] = 0.0f;
    slot.parameters[6] = 1.0f;
    slot.parameters[2] = 4.0f;
    slot.parameters[4] = 52.0f;
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_DELAY, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(!processor.delay->sweep_echo.time_sync);
    assert(processor.delay->sweep_echo.sweep_sync);
    assert(processor.delay->sweep_echo.target_delay_samples == 882.0f);
    assert(processor.delay->sweep_echo.target_sweep_step == 1.0f / 22050.0f);

    load_defaults(GP100_EFFECT_DELAY, &slot, 10u);
    slot.parameters[1] = 4.0f;
    slot.parameters[5] = 1.0f;
    fill_signal(samples, 64u);
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_DELAY, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(processor.delay->m_echo2.tempo_sync);
    assert(!processor.delay->m_echo2.trail);
    assert(processor.delay->m_echo2.target_delay_samples == 22050.0f);

    slot.parameters[1] = 100.0f;
    slot.parameters[5] = 0.0f;
    slot.parameters[6] = 1.0f;
    assert(gp100_effect_processor_process(
               &processor, GP100_EFFECT_DELAY, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_PROCESSED);
    assert(!processor.delay->m_echo2.tempo_sync);
    assert(processor.delay->m_echo2.trail);
    assert(processor.delay->m_echo2.target_delay_samples == 4410.0f);

    assert(gp100_effect_processor_process(
               NULL, GP100_EFFECT_MODULATION, &slot,
               samples, 64u) == GP100_EFFECT_PROCESSOR_INVALID);
    assert(gp100_effect_processor_chain_ops(NULL).process_effect == NULL);
    return 0;
}
