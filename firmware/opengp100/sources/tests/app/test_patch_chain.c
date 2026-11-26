#include "app/firmware_runtime.h"
#include "assets/ui/development_font.h"
#include "audio/dsp/audio_pcm.h"
#include "audio/effects/effect_processor.h"
#include "storage/patch_chain.h"

#include <assert.h>
#include <math.h>
#include <string.h>

typedef struct patch_fixture {
    gp100_preset_record_t slots[2];
    gp100_firmware_runtime_t runtime;
    gp100_firmware_runtime_result_t event;
} patch_fixture_t;

static bool load_patch(void *context, uint16_t index, gp100_preset_record_t *preset)
{
    const patch_fixture_t *fixture = context;
    if (index >= 2u) {
        return false;
    }
    *preset = fixture->slots[index];
    return true;
}

static void dispatch(patch_fixture_t *fixture, uint32_t code)
{
    const gp100_event_t event = {.code = code};
    assert(gp100_firmware_runtime_handle_event(
        &fixture->runtime, &event, load_patch, fixture, &fixture->event));
}

static void button(patch_fixture_t *fixture, uint8_t control, gp100_button_action_t action)
{
    dispatch(fixture, gp100_button_event_code(control, action));
}

static void knob(patch_fixture_t *fixture, uint8_t control, bool forward)
{
    dispatch(fixture, gp100_encoder_event_code(control, forward, 1u));
}

static void empty_patch(gp100_preset_record_t *preset)
{
    assert(gp100_make_development_preset(preset, "Empty"));
    gp100_patch_init(preset);
    assert(gp100_patch_validate(preset));
    assert(gp100_patch_count(preset) == 0u);
}

static void test_front_panel(void)
{
    static int32_t looper_storage[128];
    patch_fixture_t fixture = {0};
    gp100_firmware_runtime_t *runtime = &fixture.runtime;
    gp100_settings_record_t settings;
    gp100_patch_block_t first, second;
    empty_patch(&fixture.slots[0]);
    empty_patch(&fixture.slots[1]);
    assert(gp100_make_development_settings(&settings));
    assert(gp100_firmware_runtime_init(
        runtime, 0u, &fixture.slots[0], &settings, looper_storage, 128u, 0u, 239u));
    assert(runtime->chain.native && runtime->chain.count == 0u);

    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_CONSTRUCT);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_GROUP);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->chain.count == 1u);
    knob(&fixture, GP100_ENCODER_QUICK_1, true);
    knob(&fixture, GP100_ENCODER_PARA, true);
    assert(runtime->ui.patch_editor.position == 1u);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    assert(runtime->ui.patch_editor.position == 0u);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    knob(&fixture, GP100_ENCODER_PARA, true);
    assert(runtime->ui.patch_editor.position == 0u);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, true);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, true);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->chain.count == 2u);
    assert(gp100_patch_block_at(&runtime->preset, 0u, &first));
    assert(gp100_patch_block_at(&runtime->preset, 1u, &second));
    assert(first.id != second.id && first.family == second.family);
    assert(first.algorithm == 0u && first.footswitches == 1u);
    assert(second.algorithm == 1u && second.footswitches == 0u);
    assert(runtime->ui.mode == GP100_UI_MODE_CONSTRUCT);
    knob(&fixture, GP100_ENCODER_QUICK_3, false);
    assert(runtime->chain.slot_at_position[0] == second.id);
    assert(runtime->chain.slot_at_position[1] == first.id);

    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    assert(runtime->ui.patch_editor.position == 0u);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    assert(runtime->ui.patch_editor.page == 0u);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_CONSTRUCT);
    button(&fixture, GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    assert(runtime->ui.patch_editor.position == 0u);
    button(&fixture, GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_PLAY);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    const gp100_preset_record_t before = runtime->preset;
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_EDIT);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    assert(memcmp(&before, &runtime->preset, sizeof(before)) == 0);
    const float prior_first = second.parameters[0];
    const float prior_second = second.parameters[1];
    knob(&fixture, GP100_ENCODER_QUICK_2, true);
    knob(&fixture, GP100_ENCODER_QUICK_3, false);
    assert(gp100_patch_block_at(&runtime->preset, 0u, &second));
    assert(second.algorithm == 1u && second.parameters[0] == prior_first + 1.0f &&
           second.parameters[1] == prior_second - 1.0f);
    assert(runtime->chain.count == 2u && runtime->chain.slot_at_position[0] == second.id &&
           runtime->chain.slot_at_position[1] == first.id);
    knob(&fixture, GP100_ENCODER_QUICK_1, false);
    assert(!runtime->effects.slots[second.id].enabled);
    assert(runtime->effects.slots[first.id].enabled);

    button(&fixture, GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    memcpy(runtime->ui.save.name, "Two FX", sizeof("Two FX"));
    button(&fixture, GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    assert(fixture.event.live.save_record_ready);
    fixture.slots[0] = fixture.event.live.save_record;
    assert(gp100_patch_validate(&fixture.slots[0]));
    gp100_firmware_runtime_mark_preset_saved(
        runtime, fixture.event.live.save_flash_address, &fixture.slots[0]);
    assert(!runtime->preset_modified);

    knob(&fixture, GP100_ENCODER_PARA, true);
    assert(runtime->ui.logical_preset == 1u && runtime->chain.count == 0u);
    knob(&fixture, GP100_ENCODER_PARA, false);
    assert(runtime->ui.logical_preset == 0u && runtime->chain.count == 2u);
    assert(gp100_settings_set_i16(&runtime->settings, GP100_SETTINGS_FOOTSWITCH_MODE,
                                  GP100_FOOTSWITCH_MODE_STOMP));
    button(&fixture, GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    assert(!runtime->effects.slots[first.id].enabled);
    assert(!runtime->effects.slots[second.id].enabled);
    char name[GP100_PRESET_NAME_SIZE + 1u];
    gp100_preset_name(&runtime->preset, name);
    assert(strcmp(name, "Two FX") == 0);

    /* Opening, browsing and cancelling actions leaves the patch alone. */
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.mode == GP100_UI_MODE_CONSTRUCT);
    assert(runtime->chain.count == 2u);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_ACTION);
    knob(&fixture, GP100_ENCODER_PARA, false);
    knob(&fixture, GP100_ENCODER_PARA, false);
    assert(runtime->ui.patch_editor.action == GP100_PATCH_ACTION_REMOVE);
    button(&fixture, GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    assert(runtime->chain.count == 2u);

    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_GROUP);
    button(&fixture, GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    assert(runtime->ui.patch_editor.picker == GP100_PATCH_PICK_NONE);
    assert(runtime->chain.count == 2u);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    for (size_t i = 0u; i < 3u; ++i) {
        button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    }
    assert(runtime->chain.count == 3u);
    assert(runtime->chain.slot_at_position[1] == second.id);
    const uint8_t inserted = runtime->chain.slot_at_position[0];

    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, true);
    assert(runtime->ui.patch_editor.action == GP100_PATCH_ACTION_REPLACE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, true);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->chain.count == 3u);
    assert(runtime->chain.slot_at_position[0] == inserted);
    assert(runtime->effects.slots[inserted].algorithm_index == 1u);

    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, false);
    knob(&fixture, GP100_ENCODER_PARA, false);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->chain.count == 2u);
    assert(runtime->chain.slot_at_position[0] == second.id);
    assert(runtime->chain.slot_at_position[1] == first.id);
}

static void test_native_render(void)
{
    static uint16_t pixels[160u * 128u];
    gp100_display_canvas_t canvas;
    gp100_edit_screen_theme_t theme;
    gp100_preset_record_t preset;
    gp100_patch_editor_t editor = {0};
    const gp100_bitmap_font_t *small = gp100_development_font_5x7();

    assert(gp100_display_canvas_init(&canvas, pixels, 160u, 128u, 160u));
    assert(gp100_edit_screen_default_theme(&theme, gp100_development_font_10x14(),
                                          small, small));
    empty_patch(&preset);
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_EDIT, &preset, 0u,
                                     false, &canvas, &theme));
    assert(pixels[36u * 160u + 1u] == theme.track_rgb565);
    assert(pixels[52u * 160u + 19u] == theme.background_rgb565);
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_CONSTRUCT, &preset, 0u,
                                     false, &canvas, &theme));
    assert(pixels[36u * 160u + 1u] == theme.chain_selection_rgb565);
    assert(pixels[52u * 160u + 19u] == theme.selected_rgb565);
    assert(pixels[86u * 160u] == theme.panel_rgb565);

    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 1u, GP100_EFFECT_PRE_FX, 6u) == GP100_PATCH_OK);
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_EDIT, &preset, 0u,
                                     true, &canvas, &theme));
    assert(pixels[52u * 160u + 99u] == theme.background_rgb565);
    /* Each card uses its own drawing even when the family color is identical. */
    unsigned changed_pixels = 0u;
    for (size_t y = 40u; y < 65u; ++y) {
        for (size_t x = 6u; x < 33u; ++x) {
            changed_pixels += pixels[y * 160u + x] != pixels[y * 160u + x + 40u];
        }
    }
    assert(changed_pixels > 30u);

    editor.picker = GP100_PATCH_PICK_ACTION;
    editor.action = GP100_PATCH_ACTION_REMOVE;
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_CONSTRUCT, &preset, 0u,
                                     true, &canvas, &theme));
    assert(pixels[104u * 160u + 3u] == theme.value_rgb565);
    assert(pixels[98u * 160u + 3u] == theme.panel_rgb565);

    editor.position = 2u;
    editor.picker = GP100_PATCH_PICK_EFFECT;
    editor.group = GP100_EFFECT_GROUP_CAB;
    editor.choice = 40u;
    assert(gp100_patch_editor_render(&editor, GP100_UI_MODE_CONSTRUCT, &preset, 0u,
                                     true, &canvas, &theme));
    assert(pixels[36u * 160u + 81u] == theme.chain_selection_rgb565);
}

static void test_effect_picker_groups(void)
{
    const char *const names[] = {
        "Dynamics", "Drive", "Wah", "Pitch", "Tone", "Modulation", "AMP", "Cab", "Delay", "Reverb"
    };
    size_t choices = 0u;
    for (gp100_effect_group_t group = 0; group < GP100_EFFECT_GROUP_COUNT; ++group) {
        assert(strcmp(gp100_effect_picker_group_name(group), names[group]) == 0);
        const size_t count = gp100_effect_picker_count(group);
        assert(count > 0u);
        choices += count;
        for (size_t i = 0u; i < count; ++i) {
            gp100_picker_effect_t effect;
            gp100_effect_group_t found_group;
            uint16_t found_index;
            uint32_t model;
            assert(gp100_effect_picker_at(group, i, &effect));
            assert(gp100_effect_algorithm_id(effect.family, effect.algorithm, &model));
            assert(gp100_effect_picker_find(effect, &found_group, &found_index));
            assert(found_group == group && found_index == i);
        }
    }
    assert(choices + 2u == GP100_TOTAL_EFFECT_ALGORITHMS);
    for (gp100_effect_family_t family = 0; family < GP100_EFFECT_FAMILY_COUNT; ++family) {
        for (size_t i = 0u; i < gp100_effect_algorithm_count(family); ++i) {
            gp100_effect_group_t group;
            uint16_t index;
            assert(gp100_effect_picker_find((gp100_picker_effect_t){family, (uint16_t)i},
                                             &group, &index));
            if (family == GP100_EFFECT_DISTORTION && (i == 17u || i == 18u)) {
                assert(group == GP100_EFFECT_GROUP_DRIVE && index == i - 17u);
            }
        }
    }
}

static void test_picker_cross_family_replace(void)
{
    static int32_t looper_storage[128];
    patch_fixture_t fixture = {0};
    gp100_settings_record_t settings;
    gp100_patch_block_t block;
    empty_patch(&fixture.slots[0]);
    assert(gp100_make_development_settings(&settings));
    assert(gp100_firmware_runtime_init(&fixture.runtime, 0u, &fixture.slots[0], &settings,
                                       looper_storage, 128u, 0u, 239u));

    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    assert(fixture.runtime.ui.patch_editor.group == GP100_EFFECT_GROUP_DYNAMICS);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, false);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(gp100_patch_block_at(&fixture.runtime.preset, 0u, &block));
    assert(block.family == GP100_EFFECT_NOISE_REDUCTION && block.algorithm == 1u);
    const uint8_t original_id = block.id;

    knob(&fixture, GP100_ENCODER_QUICK_2, true);
    assert(fixture.runtime.ui.patch_editor.group == GP100_EFFECT_GROUP_DYNAMICS);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    gp100_picker_effect_t selected;
    assert(gp100_effect_picker_at(fixture.runtime.ui.patch_editor.group,
                                  fixture.runtime.ui.patch_editor.choice, &selected));
    assert(selected.family == GP100_EFFECT_NOISE_REDUCTION && selected.algorithm == 1u);
    knob(&fixture, GP100_ENCODER_PARA, true);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(gp100_patch_block_at(&fixture.runtime.preset, 0u, &block));
    assert(block.id == original_id && block.family == GP100_EFFECT_PRE_FX && block.algorithm == 0u);
}

static void test_stomp_thumbnails(void)
{
    static uint16_t pixels[160u * 128u];
    gp100_display_canvas_t canvas;
    gp100_play_screen_theme_t theme;
    gp100_play_screen_snapshot_t screen;
    gp100_preset_record_t preset;
    gp100_settings_record_t settings;
    uint16_t logical_preset = 0u;
    const gp100_bitmap_font_t *font = gp100_development_font_5x7();

    assert(gp100_display_canvas_init(&canvas, pixels, 160u, 128u, 160u));
    assert(gp100_play_screen_default_theme(&theme, font, font, font));
    empty_patch(&preset);
    assert(gp100_make_development_settings(&settings));
    assert(gp100_settings_set_i16(&settings, GP100_SETTINGS_FOOTSWITCH_MODE,
                                  GP100_FOOTSWITCH_MODE_STOMP));
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 1u, GP100_EFFECT_PRE_FX, 5u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 2u, GP100_EFFECT_AMPLIFIER, 0u) == GP100_PATCH_OK);
    for (size_t i = 0u; i < 3u; ++i) {
        gp100_patch_block_t block;
        assert(gp100_patch_block_at(&preset, i, &block));
        block.footswitches = i < 2u ? 1u : 2u;
        block.enabled = i != 1u;
        assert(gp100_patch_update(&preset, &block));
    }

    assert(gp100_play_screen_snapshot_from_records(
        &screen, logical_preset, &preset, &settings, false, false));
    assert(screen.stomp_count[0] == 2u && screen.stomp_count[1] == 1u);
    assert(screen.stomp_effects[0][0].algorithm == 0u &&
           screen.stomp_effects[0][1].algorithm == 5u);
    assert(gp100_play_screen_render(&canvas, &screen, &theme));
    assert(pixels[86u * 160u + 43u] == theme.accent_rgb565);
    assert(pixels[86u * 160u + 71u] != theme.accent_rgb565);
    assert(pixels[86u * 160u + 135u] == theme.accent_rgb565);

    assert(gp100_play_footswitch(&settings, &preset, &logical_preset, 0u) ==
           GP100_PLAY_ACTION_PRESET_CHANGED);
    assert(gp100_play_screen_snapshot_from_records(
        &screen, logical_preset, &preset, &settings, false, false));
    assert(gp100_play_screen_render(&canvas, &screen, &theme));
    assert(pixels[86u * 160u + 43u] != theme.accent_rgb565);
    assert(pixels[86u * 160u + 71u] == theme.accent_rgb565);
    assert(pixels[86u * 160u + 135u] == theme.accent_rgb565);

    empty_patch(&preset);
    for (size_t i = 0u; i < GP100_PATCH_MAX_BLOCKS; ++i) {
        gp100_patch_block_t block;
        assert(gp100_patch_insert(&preset, i, GP100_EFFECT_PRE_FX, 3u) == GP100_PATCH_OK);
        assert(gp100_patch_block_at(&preset, i, &block));
        block.footswitches = 3u;
        assert(gp100_patch_update(&preset, &block));
    }
    assert(gp100_play_screen_snapshot_from_records(
        &screen, logical_preset, &preset, &settings, false, false));
    assert(screen.stomp_count[0] == 8u && screen.stomp_count[1] == 8u);
    assert(gp100_play_screen_render(&canvas, &screen, &theme));
    assert(pixels[90u * 160u + 78u] == theme.accent_rgb565);
    assert(pixels[90u * 160u + 79u] == theme.track_rgb565);
    assert(pixels[90u * 160u + 156u] == theme.accent_rgb565);
}

static void test_limits(void)
{
    gp100_preset_record_t preset;
    empty_patch(&preset);
    for (size_t i = 0; i < GP100_PATCH_MAX_BLOCKS; ++i) {
        assert(gp100_patch_insert(&preset, i, GP100_EFFECT_DISTORTION, 18u) ==
               GP100_PATCH_OK);
    }
    const gp100_preset_record_t full = preset;
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_MODULATION, 0u) ==
           GP100_PATCH_FULL);
    assert(memcmp(&preset, &full, sizeof(preset)) == 0);
    assert(gp100_patch_remove(&preset, 3u));
    assert(gp100_patch_insert(&preset, 3u, GP100_EFFECT_MODULATION, 0u) ==
           GP100_PATCH_OK);

    empty_patch(&preset);
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_DELAY, 11u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 1u, GP100_EFFECT_DELAY, 11u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 2u, GP100_EFFECT_DELAY, 11u) ==
           GP100_PATCH_MEMORY_LIMIT);

    empty_patch(&preset);
    for (size_t i = 0; i < 4; ++i) {
        assert(gp100_patch_insert(&preset, i, GP100_EFFECT_CABINET, 0u) ==
               GP100_PATCH_OK);
    }
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_CABINET, 0u) ==
           GP100_PATCH_CPU_LIMIT);
    assert(gp100_patch_insert(&preset, 4u, GP100_EFFECT_PRE_FX, 9u) ==
           GP100_PATCH_OK);
    gp100_patch_block_t octaver;
    assert(gp100_patch_block_at(&preset, 4u, &octaver));
    octaver.footswitches = 2u;
    assert(gp100_patch_update(&preset, &octaver));
    const gp100_preset_record_t before = preset;
    assert(gp100_patch_replace(&preset, 4u, GP100_EFFECT_DISTORTION, 0u) ==
           GP100_PATCH_CPU_LIMIT);
    assert(memcmp(&preset, &before, sizeof(preset)) == 0);
    assert(gp100_patch_replace(&preset, 4u, GP100_EFFECT_PRE_FX, 5u) ==
           GP100_PATCH_OK);
    gp100_patch_block_t wah;
    assert(gp100_patch_block_at(&preset, 4u, &wah));
    assert(wah.id == octaver.id && wah.footswitches == 2u && wah.algorithm == 5u);

    empty_patch(&preset);
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_PRE_FX, 9u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 1u, GP100_EFFECT_PRE_FX, 5u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 2u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 3u, GP100_EFFECT_DISTORTION, 1u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 4u, GP100_EFFECT_DISTORTION, 12u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 5u, GP100_EFFECT_AMPLIFIER, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 6u, GP100_EFFECT_CABINET, 0u) == GP100_PATCH_OK);
}

typedef struct processor_fixture {
    gp100_effect_processor_t effects[2];
    unsigned seen;
} processor_fixture_t;

static void process(void *context, gp100_effect_family_t family,
                    const gp100_effect_slot_state_t *slot, float *samples, size_t frames)
{
    processor_fixture_t *fixture = context;
    assert(slot->instance_id < 2u && family == GP100_EFFECT_PRE_FX);
    fixture->seen |= 1u << slot->instance_id;
    assert(gp100_effect_processor_process(&fixture->effects[slot->instance_id],
                                          family, slot, samples, frames) ==
           GP100_EFFECT_PROCESSOR_PROCESSED);
}

static void test_independent_processors(void)
{
    static gp100_pre_fx_processor_state_t states[2];
    gp100_preset_record_t preset;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime = {0};
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    processor_fixture_t fixture = {0};
    const gp100_effect_chain_ops_t ops = {.context = &fixture, .process_effect = process};
    float audio[] = {1000000.0f, -1000000.0f};
    empty_patch(&preset);
    assert(gp100_patch_insert(&preset, 0u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&preset, 1u, GP100_EFFECT_PRE_FX, 2u) == GP100_PATCH_OK);
    assert(gp100_effect_chain_from_preset(&chain, &preset));
    assert(gp100_effect_runtime_apply_preset(&runtime, &preset, actions));
    for (size_t i = 0; i < 2; ++i) {
        gp100_effect_processor_reset(&fixture.effects[i]);
        assert(gp100_effect_processor_bind(&fixture.effects[i], GP100_EFFECT_PRE_FX,
                                           &states[i]));
        assert(gp100_effect_processor_prepare_slot(
            &fixture.effects[i], GP100_EFFECT_PRE_FX, &runtime.slots[i]));
    }
    assert(gp100_effect_chain_process_block(&chain, &runtime,
                                           GP100_EFFECT_CHAIN_ALL_FAMILIES,
                                           audio, 1u, &ops));
    assert(fixture.seen == 3u && fixture.effects[0].pre_fx_algorithm == 0u &&
           fixture.effects[1].pre_fx_algorithm == 2u);
}

static void test_limitter_chain(void)
{
    static gp100_pre_fx_processor_state_t states[2];
    gp100_preset_record_t preset;
    gp100_effect_chain_t chain;
    gp100_effect_runtime_t runtime = {0};
    uint32_t actions[GP100_EFFECT_FAMILY_COUNT];
    processor_fixture_t fixture = {0};
    const gp100_effect_chain_ops_t ops = {.context = &fixture, .process_effect = process};
    float audio[64u * 2u];

    empty_patch(&preset);
    for (size_t i = 0u; i < 2u; ++i) {
        gp100_patch_block_t block;
        assert(gp100_patch_insert(&preset, i, GP100_EFFECT_PRE_FX, GP100_PRE_FX_LIMITTER) ==
               GP100_PATCH_OK);
        assert(gp100_patch_block_at(&preset, i, &block));
        assert(block.enabled && block.parameters[0] == 0.0f &&
               block.parameters[1] == 0.0f && block.parameters[2] == 15.0f);
        block.parameters[0] = i == 0u ? 0.0f : -12.0f;
        block.parameters[1] = i == 0u ? -6.0f : -12.0f;
        block.parameters[2] = i == 0u ? 10.0f : 30.0f;
        assert(gp100_patch_update(&preset, &block));
    }
    assert(gp100_patch_validate(&preset));
    assert(gp100_effect_chain_from_preset(&chain, &preset));
    assert(gp100_effect_runtime_apply_preset(&runtime, &preset, actions));
    for (size_t i = 0u; i < 2u; ++i) {
        gp100_effect_processor_reset(&fixture.effects[i]);
        assert(gp100_effect_processor_bind(&fixture.effects[i], GP100_EFFECT_PRE_FX, &states[i]));
        assert(gp100_effect_processor_prepare_slot(&fixture.effects[i], GP100_EFFECT_PRE_FX,
                                                   &runtime.slots[i]));
    }
    const float ceiling = powf(10.0f, -12.0f / 20.0f) * GP100_EFFECT_DSP_FULL_SCALE;
    for (size_t block = 0u; block < 4u; ++block) {
        for (size_t i = 0u; i < 64u; ++i) {
            audio[2u * i] = 2.0f * GP100_EFFECT_DSP_FULL_SCALE;
            audio[2u * i + 1u] = -0.5f * GP100_EFFECT_DSP_FULL_SCALE;
        }
        assert(gp100_effect_chain_process_block(&chain, &runtime,
                                               GP100_EFFECT_CHAIN_ALL_FAMILIES,
                                               audio, 64u, &ops));
        for (size_t i = 0u; i < 64u; ++i) {
            const float expected = block * 64u + i < 2u * GP100_LIMITER_LOOKAHEAD_FRAMES
                ? 0.0f : ceiling;
            assert(fabsf(audio[2u * i] - expected) < 1e-6f);
            assert(fabsf(audio[2u * i + 1u] + 0.25f * expected) < 1e-6f);
        }
    }
    assert(fixture.seen == 3u && states[0].limitter.config.release_db_per_sec == 10 &&
           states[1].limitter.config.release_db_per_sec == 30);
}

static void open_expression_editor(patch_fixture_t *fixture)
{
    button(fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    knob(fixture, GP100_ENCODER_PARA, false);
    button(fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(fixture->runtime.ui.patch_editor.picker == GP100_PATCH_PICK_EXPRESSION);
}

static void test_expression_assignment(void)
{
    static int32_t looper_storage[128];
    static uint16_t pixels[GP100_UI_LOGICAL_WIDTH * GP100_UI_LOGICAL_HEIGHT];
    patch_fixture_t fixture = {0};
    gp100_firmware_runtime_t *runtime = &fixture.runtime;
    gp100_settings_record_t settings;
    gp100_patch_expression_t assignment;
    gp100_patch_block_t first;
    gp100_patch_block_t second;
    gp100_display_canvas_t canvas;
    gp100_edit_screen_theme_t theme;
    const gp100_bitmap_font_t *font = gp100_development_font_5x7();

    empty_patch(&fixture.slots[0]);
    empty_patch(&fixture.slots[1]);
    assert(gp100_patch_insert(&fixture.slots[0], 0u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_patch_insert(&fixture.slots[0], 1u, GP100_EFFECT_PRE_FX, 0u) == GP100_PATCH_OK);
    assert(gp100_make_development_settings(&settings));
    assert(gp100_firmware_runtime_init(runtime, 0u, &fixture.slots[0], &settings,
                                       looper_storage, 128u, 0u, 239u));
    assert(gp100_patch_block_at(&runtime->preset, 0u, &first));
    assert(gp100_patch_block_at(&runtime->preset, 1u, &second));
    gp100_patch_block_t unlinked = second;
    unlinked.footswitches = GP100_PATCH_FOOTSWITCH_EXP;
    assert(!gp100_patch_update(&runtime->preset, &unlinked));
    dispatch(&fixture, GP100_EXPRESSION_POSITION_EVENT_PREFIX | 3u);
    assert(runtime->volume_pedal_percent == 0u);
    dispatch(&fixture, GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 250u);
    assert(runtime->volume_pedal_percent == 100u && !runtime->preset_modified);
    button(&fixture, GP100_BUTTON_EDIT, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_HOLD);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    knob(&fixture, GP100_ENCODER_PARA, true);

    open_expression_editor(&fixture);
    knob(&fixture, GP100_ENCODER_QUICK_1, true);
    button(&fixture, GP100_BUTTON_EXIT, GP100_BUTTON_RELEASE);
    assert(!gp100_patch_expression_get(&runtime->preset, NULL, NULL));
    assert(!runtime->preset_modified);

    open_expression_editor(&fixture);
    knob(&fixture, GP100_ENCODER_QUICK_1, true);
    knob(&fixture, GP100_ENCODER_QUICK_2, true);
    knob(&fixture, GP100_ENCODER_QUICK_3, false);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(runtime->preset_modified && gp100_patch_validate(&runtime->preset));
    assert(gp100_patch_expression_get(&runtime->preset, &assignment, NULL));
    assert(assignment.block_id == second.id && assignment.parameter == 0u);
    assert(assignment.heel == 1.0f && assignment.toe == 98.0f);

    knob(&fixture, GP100_ENCODER_QUICK_1, false);
    assert(gp100_patch_block_at(&runtime->preset, 1u, &second));
    assert(second.footswitches == GP100_PATCH_FOOTSWITCH_EXP);
    knob(&fixture, GP100_ENCODER_PARA, false);
    knob(&fixture, GP100_ENCODER_QUICK_1, false);
    assert(gp100_patch_block_at(&runtime->preset, 0u, &first));
    assert(first.footswitches == 3u);
    knob(&fixture, GP100_ENCODER_QUICK_1, true);
    knob(&fixture, GP100_ENCODER_PARA, true);

    assert(gp100_display_canvas_init(&canvas, pixels, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, GP100_UI_LOGICAL_WIDTH));
    assert(gp100_edit_screen_default_theme(&theme, NULL, font, font));
    assert(gp100_patch_editor_render(&runtime->ui.patch_editor, runtime->ui.mode,
        &runtime->preset, 0u, true, &canvas, &theme));
    assert(pixels[38u * GP100_UI_LOGICAL_WIDTH + 57u] == theme.selected_rgb565);
    assert(pixels[38u * GP100_UI_LOGICAL_WIDTH + 17u] != theme.selected_rgb565);

    knob(&fixture, GP100_ENCODER_QUICK_3, false);
    assert(gp100_patch_block_at(&runtime->preset, 0u, &second));
    assert(second.id == assignment.block_id && second.footswitches == GP100_PATCH_FOOTSWITCH_EXP);
    dispatch(&fixture, GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 250u);
    assert(!runtime->effects.slots[second.id].enabled);
    assert(runtime->effects.slots[first.id].enabled);
    assert(runtime->effects.slots[second.id].parameters[0] == assignment.toe);
    assert(runtime->effects.slots[first.id].parameters[0] == first.parameters[0]);
    dispatch(&fixture, GP100_EXPRESSION_POSITION_EVENT_PREFIX | 3u);
    assert(runtime->effects.slots[second.id].parameters[0] == assignment.heel);
    dispatch(&fixture, GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 250u);
    assert(runtime->effects.slots[second.id].enabled);
    assert(runtime->effects.slots[second.id].parameters[0] == second.parameters[0]);

    gp100_preset_record_t saved;
    button(&fixture, GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    button(&fixture, GP100_BUTTON_SAVE, GP100_BUTTON_RELEASE);
    assert(fixture.event.live.save_record_ready);
    saved = fixture.event.live.save_record;
    assert(gp100_patch_validate(&saved));
    assert(gp100_patch_expression_get(&saved, &assignment, NULL));
    assert(assignment.block_id == second.id);
    fixture.slots[0] = saved;
    assert(gp100_ui_runtime_set_mode(&runtime->ui, GP100_UI_MODE_PLAY,
                                     &runtime->preset, &runtime->settings));
    button(&fixture, GP100_BUTTON_FOOTSWITCH_PLUS, GP100_BUTTON_RELEASE);
    assert(!gp100_patch_expression_get(&runtime->preset, NULL, NULL));
    button(&fixture, GP100_BUTTON_FOOTSWITCH_MINUS, GP100_BUTTON_RELEASE);
    assert(gp100_patch_expression_get(&runtime->preset, &assignment, NULL));
    assert(assignment.block_id == second.id && assignment.heel == 1.0f && assignment.toe == 98.0f);
    assert(gp100_patch_block_at(&runtime->preset, 0u, &second));
    assert(second.footswitches == GP100_PATCH_FOOTSWITCH_EXP);

    assert(gp100_ui_runtime_set_mode(&runtime->ui, GP100_UI_MODE_CONSTRUCT,
                                     &runtime->preset, &runtime->settings));
    open_expression_editor(&fixture);
    knob(&fixture, GP100_ENCODER_QUICK_1, false);
    button(&fixture, GP100_BUTTON_PARA, GP100_BUTTON_RELEASE);
    assert(!gp100_patch_expression_get(&runtime->preset, NULL, NULL));
    assert(gp100_patch_block_at(&runtime->preset, 0u, &second));
    assert(second.footswitches == 0u);
    dispatch(&fixture, GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | 250u);
    gp100_expression_pedal_target_t target;
    assert(gp100_preset_expression_pedal_target(&runtime->preset, &target));
    assert(target == GP100_EXPRESSION_PEDAL_VOLUME && runtime->effects.slots[second.id].enabled);

    gp100_preset_record_t removed = saved;
    assert(gp100_patch_remove(&removed, 0u));
    assert(!gp100_patch_expression_get(&removed, NULL, NULL));
    assert(gp100_patch_replace(&saved, 0u, GP100_EFFECT_PRE_FX, 1u) == GP100_PATCH_OK);
    assert(!gp100_patch_expression_get(&saved, NULL, NULL));
    assert(gp100_patch_block_at(&saved, 0u, &second) && second.footswitches == 0u);
}

int main(void)
{
    test_front_panel();
    test_native_render();
    test_effect_picker_groups();
    test_picker_cross_family_replace();
    test_stomp_thumbnails();
    test_limits();
    test_independent_processors();
    test_limitter_chain();
    test_expression_assignment();
    return 0;
}
