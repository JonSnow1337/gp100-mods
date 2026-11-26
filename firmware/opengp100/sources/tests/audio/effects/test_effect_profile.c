#include "app/firmware_runtime.h"
#include "assets/ui/development_font.h"
#include "audio/effects/effect_processor.h"
#include "input/expression_runtime.h"
#include "storage/preset_store.h"
#include "ui/screens/effect_picker.h"
#include "ui/screens/patch_editor.h"
#include "ui/screens/play_screen.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

static void verify_picker(void)
{
    bool listed[GP100_EFFECT_FAMILY_COUNT][64] = {{false}};
    for (gp100_effect_group_t group = 0; group < GP100_EFFECT_GROUP_COUNT; ++group) {
        const size_t count = gp100_effect_picker_count(group);
        for (size_t i = 0u; i < count; ++i) {
            gp100_picker_effect_t effect;
            assert(gp100_effect_picker_at(group, i, &effect));
            assert(gp100_effect_available(effect.family, effect.algorithm));
            assert(!listed[effect.family][effect.algorithm]);
            listed[effect.family][effect.algorithm] = true;
        }
        gp100_picker_effect_t effect;
        assert(!gp100_effect_picker_at(group, count, &effect));
    }
    for (gp100_effect_family_t family = 0; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        for (size_t model = 0u; model < gp100_effect_algorithm_count(family); ++model) {
            gp100_effect_group_t group;
            uint16_t choice;
            assert(gp100_effect_picker_find((gp100_picker_effect_t){family, (uint16_t)model},
                &group, &choice) == gp100_effect_available(family, model));
        }
    }
}

static void verify_unavailable_preset(gp100_effect_family_t family, uint32_t id)
{
    static uint8_t journal[GP100_PRESET_STORE_BYTES];
    static uint8_t legacy[GP100_PRESET_LEGACY_BYTES];
    gp100_preset_record_t preset;
    gp100_preset_record_t loaded;
    assert(gp100_make_development_preset(&preset, "Missing Test"));
    gp100_patch_init(&preset);
    assert(gp100_effect_algorithm_index(family, id) < 0);

    /* Simulate a saved one-block preset created by a build containing this model. */
    preset.bytes[0x0f] = 1u;
    preset.bytes[0x20] = (uint8_t)(family + 1u);
    preset.bytes[0x21] = 3u; /* Enabled and assigned to footswitch 1. */
    for (unsigned i = 0u; i < 4u; ++i) {
        preset.bytes[0x24 + i] = (uint8_t)(id >> (i * 8u));
    }
    preset.bytes[0x2a] = 0x80; /* Parameter zero is 1.0f. */
    preset.bytes[0x2b] = 0x3f;
    preset.bytes[0x240] = 0xa1; /* Retain an expression link to the unknown pedal. */
    preset.bytes[0x242] = 2u;
    assert(gp100_preset_set_expression_pedal_target(&preset, GP100_EXPRESSION_PEDAL_EFFECT));
    gp100_preset_update_crc(&preset);
    assert(gp100_patch_validate(&preset));
    memset(journal, 0xff, sizeof(journal));
    memcpy(legacy, preset.bytes, sizeof(preset.bytes));
    assert(gp100_preset_store_load(journal, legacy, 0u, &loaded));
    gp100_patch_block_t block;
    assert(gp100_patch_block_at(&loaded, 0u, &block));
    assert(block.algorithm == GP100_PATCH_UNKNOWN_ALGORITHM && block.enabled);
    gp100_effect_chain_t chain;
    assert(gp100_effect_chain_from_preset(&chain, &loaded));

    gp100_effect_runtime_t runtime = {0};
    gp100_effect_processor_t processor = {0};
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    assert(gp100_effect_runtime_apply_preset(&runtime, &loaded, actions));
    assert(!runtime.slots[0].algorithm_loaded);
    gp100_patch_expression_t assignment;
    gp100_expression_apply_result_t expression;
    assert(gp100_patch_expression_get(&loaded, &assignment, NULL) && assignment.parameter == 2u);
    assert(gp100_expression_runtime_apply(&loaded, &runtime, 5u, 10u, &expression));
    float samples[] = {0.25f, -0.5f};
    assert(gp100_effect_processor_process(&processor, chain.family_at_position[0], &runtime.slots[0],
                                          samples, 1u) == GP100_EFFECT_PROCESSOR_BYPASSED);
    assert(samples[0] == 0.25f && samples[1] == -0.5f);
    assert(memcmp(preset.bytes, loaded.bytes, sizeof(preset.bytes)) == 0);

    gp100_settings_record_t settings;
    gp100_play_screen_snapshot_t play;
    assert(gp100_make_development_settings(&settings));
    assert(gp100_play_screen_snapshot_from_records(&play, 0u, &loaded, &settings, false, false));
    assert(play.unavailable_effects && !play.stomp_effects[0][0].enabled);
    assert(play.stomp_effects[0][0].algorithm == GP100_PATCH_UNKNOWN_ALGORITHM);
    assert(play.expression_active && !play.expression_pedal.enabled);
    static uint16_t pixels[GP100_UI_LOGICAL_WIDTH * GP100_UI_LOGICAL_HEIGHT];
    gp100_display_canvas_t canvas;
    gp100_edit_screen_theme_t theme;
    gp100_patch_editor_t editor = {0};
    const gp100_bitmap_font_t *font = gp100_development_font_5x7();
    assert(gp100_display_canvas_init(&canvas, pixels, GP100_UI_LOGICAL_WIDTH,
                                     GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH));
    assert(gp100_edit_screen_default_theme(&theme, font, font, font));
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_EDIT, &loaded, 0u,
                                     false, &canvas, &theme));
    block.footswitches = 2u;
    assert(gp100_patch_update(&loaded, &block));
    assert(memcmp(preset.bytes + 0x22u, loaded.bytes + 0x22u, 66u) == 0);
    assert(gp100_patch_validate(&loaded));
}

int main(void)
{
    verify_picker();
    verify_unavailable_preset(GP100_EFFECT_PRE_FX, UINT32_C(0x4f47ff01));
    verify_unavailable_preset((gp100_effect_family_t)99, UINT32_C(0x4f47ff02));
    if (!gp100_effect_available(GP100_EFFECT_PRE_FX, 1u)) {
        uint32_t unused;
        assert(!gp100_effect_algorithm_id(GP100_EFFECT_PRE_FX, 1u, &unused));
        assert(gp100_effect_algorithm_name(GP100_EFFECT_PRE_FX, 1u) == NULL);
        verify_unavailable_preset(GP100_EFFECT_PRE_FX, UINT32_C(0x00000001));
    }
    return 0;
}
