#include "global_screen.h"

#include "audio/effects/dynamics/output_limiter.h"
#include "ui/ui_palette.h"

#include <string.h>

static const char *const menu_names[GP100_GLOBAL_MENU_ITEMS] = {
    "I/O",
    "Tap Tempo Mode",
    "EXP Setting",
    "USB Audio",
    "Footswitch Mode",
    "Global EQ",
    "Output Limiter",
    "Language",
    "Factory Reset",
    "About"
};

static const int32_t control_center[GP100_GLOBAL_CONTROL_SLOTS] = {
    27, 80, 133
};

static const struct limiter_control {
    const char *label;
    gp100_settings_field_t field;
    int16_t minimum;
    int16_t maximum;
} limiter_controls[GP100_GLOBAL_CONTROL_SLOTS] = {
    {"Threshold", GP100_SETTINGS_LIMITER_THRESHOLD_DB,
     GP100_LIMITER_MIN_THRESHOLD_DB, GP100_LIMITER_MAX_THRESHOLD_DB},
    {"Ceiling", GP100_SETTINGS_LIMITER_CEILING_DB,
     GP100_LIMITER_MIN_CEILING_DB, GP100_LIMITER_MAX_CEILING_DB},
    {"Release", GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC,
     GP100_LIMITER_MIN_RELEASE_DB_PER_SEC, GP100_LIMITER_MAX_RELEASE_DB_PER_SEC}
};

static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (destination == NULL || capacity == 0u || source == NULL) {
        return false;
    }
    length = strlen(source);
    if (length >= capacity) {
        length = capacity - 1u;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

const char *gp100_global_menu_item_name(
    gp100_global_menu_item_t item)
{
    if ((unsigned)item >= GP100_GLOBAL_MENU_ITEMS) {
        return NULL;
    }
    return menu_names[(size_t)item];
}

void gp100_global_controller_init(
    gp100_global_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    controller->view = GP100_GLOBAL_VIEW_MENU;
    controller->menu_item = GP100_GLOBAL_IO;
    controller->expression_item = GP100_EXPRESSION_VOLUME_RANGE;
}

unsigned gp100_global_controller_factory_page(
    const gp100_global_controller_t *controller)
{
    if (controller == NULL) {
        return 0u;
    }
    switch (controller->view) {
    case GP100_GLOBAL_VIEW_MENU:
        if ((unsigned)controller->menu_item >= GP100_GLOBAL_MENU_ITEMS) {
            return 0u;
        }
        if (controller->menu_item == GP100_GLOBAL_OUTPUT_LIMITER) {
            return 0u;
        }
        return (unsigned)controller->menu_item +
               (controller->menu_item < GP100_GLOBAL_OUTPUT_LIMITER ? 1u : 0u);
    case GP100_GLOBAL_VIEW_EXPRESSION:
        if ((unsigned)controller->expression_item >=
            GP100_EXPRESSION_MENU_ITEMS) {
            return 0u;
        }
        return (unsigned)controller->expression_item + 12u;
    case GP100_GLOBAL_VIEW_LANGUAGE:
        return 14u;
    case GP100_GLOBAL_VIEW_FACTORY_RESET:
        return 15u;
    case GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION:
        return 16u;
    default:
        return 0u;
    }
}

gp100_global_navigation_result_t gp100_global_controller_move(
    gp100_global_controller_t *controller,
    bool forward)
{
    if (controller == NULL) {
        return GP100_GLOBAL_NAVIGATION_INVALID;
    }
    if (controller->view == GP100_GLOBAL_VIEW_MENU) {
        unsigned item = (unsigned)controller->menu_item;
        if (item >= GP100_GLOBAL_MENU_ITEMS) {
            return GP100_GLOBAL_NAVIGATION_INVALID;
        }
        if ((forward && item + 1u >= GP100_GLOBAL_MENU_ITEMS) ||
            (!forward && item == 0u)) {
            return GP100_GLOBAL_NAVIGATION_UNCHANGED;
        }
        controller->menu_item = (gp100_global_menu_item_t)(
            forward ? item + 1u : item - 1u);
        controller->page_index = 0u;
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    }
    if (controller->view == GP100_GLOBAL_VIEW_EXPRESSION) {
        unsigned item = (unsigned)controller->expression_item;
        if (item >= GP100_EXPRESSION_MENU_ITEMS) {
            return GP100_GLOBAL_NAVIGATION_INVALID;
        }
        if ((forward && item + 1u >= GP100_EXPRESSION_MENU_ITEMS) ||
            (!forward && item == 0u)) {
            return GP100_GLOBAL_NAVIGATION_UNCHANGED;
        }
        controller->expression_item = (gp100_expression_menu_item_t)(
            forward ? item + 1u : item - 1u);
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    }
    return GP100_GLOBAL_NAVIGATION_UNCHANGED;
}

gp100_global_navigation_result_t gp100_global_controller_activate(
    gp100_global_controller_t *controller)
{
    if (controller == NULL) {
        return GP100_GLOBAL_NAVIGATION_INVALID;
    }
    if (controller->view == GP100_GLOBAL_VIEW_MENU) {
        if ((unsigned)controller->menu_item >= GP100_GLOBAL_MENU_ITEMS) {
            return GP100_GLOBAL_NAVIGATION_INVALID;
        }
        switch (controller->menu_item) {
        case GP100_GLOBAL_EXPRESSION_SETTING:
            controller->view = GP100_GLOBAL_VIEW_EXPRESSION;
            controller->expression_item = GP100_EXPRESSION_VOLUME_RANGE;
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        case GP100_GLOBAL_USB_AUDIO:
            controller->page_index = (controller->page_index + 1u) % 2u;
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        case GP100_GLOBAL_EQ:
            controller->page_index = (controller->page_index + 1u) % 5u;
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        case GP100_GLOBAL_LANGUAGE:
            controller->view = GP100_GLOBAL_VIEW_LANGUAGE;
            memset(&controller->language, 0, sizeof(controller->language));
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        case GP100_GLOBAL_FACTORY_RESET:
            controller->view = GP100_GLOBAL_VIEW_FACTORY_RESET;
            gp100_factory_reset_init(&controller->factory_reset);
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        default:
            return GP100_GLOBAL_NAVIGATION_UNCHANGED;
        }
    }
    if (controller->view == GP100_GLOBAL_VIEW_EXPRESSION) {
        if ((unsigned)controller->expression_item >=
            GP100_EXPRESSION_MENU_ITEMS) {
            return GP100_GLOBAL_NAVIGATION_INVALID;
        }
        if (controller->expression_item ==
            GP100_EXPRESSION_CALIBRATE) {
            controller->view =
                GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION;
            gp100_expression_calibration_init(
                &controller->expression_calibration);
            controller->calibration_raw = 0u;
            controller->calibration_peak = 0u;
            return GP100_GLOBAL_NAVIGATION_UPDATED;
        }
    }
    return GP100_GLOBAL_NAVIGATION_UNCHANGED;
}

gp100_global_navigation_result_t gp100_global_controller_exit(
    gp100_global_controller_t *controller)
{
    if (controller == NULL) {
        return GP100_GLOBAL_NAVIGATION_INVALID;
    }
    switch (controller->view) {
    case GP100_GLOBAL_VIEW_MENU:
        return GP100_GLOBAL_NAVIGATION_EXIT_TO_PLAY;
    case GP100_GLOBAL_VIEW_EXPRESSION:
        controller->view = GP100_GLOBAL_VIEW_MENU;
        controller->menu_item = GP100_GLOBAL_EXPRESSION_SETTING;
        controller->page_index = 0u;
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    case GP100_GLOBAL_VIEW_LANGUAGE:
        controller->view = GP100_GLOBAL_VIEW_MENU;
        controller->menu_item = GP100_GLOBAL_LANGUAGE;
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    case GP100_GLOBAL_VIEW_FACTORY_RESET:
        controller->view = GP100_GLOBAL_VIEW_MENU;
        controller->menu_item = GP100_GLOBAL_FACTORY_RESET;
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    case GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION:
        controller->view = GP100_GLOBAL_VIEW_EXPRESSION;
        controller->expression_item = GP100_EXPRESSION_CALIBRATE;
        return GP100_GLOBAL_NAVIGATION_UPDATED;
    default:
        return GP100_GLOBAL_NAVIGATION_INVALID;
    }
}

bool gp100_global_controller_snapshot(
    const gp100_global_controller_t *controller,
    gp100_global_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    const char *firmware_version)
{
    if (controller == NULL ||
        controller->view != GP100_GLOBAL_VIEW_MENU) {
        return false;
    }
    return gp100_global_screen_snapshot_from_settings(
        snapshot, settings, controller->menu_item,
        controller->page_index, firmware_version);
}

static bool adjust_integer_setting(
    gp100_settings_record_t *settings,
    gp100_settings_field_t field,
    int16_t minimum,
    int16_t maximum,
    bool increase)
{
    int16_t value;

    if (!gp100_settings_get_i16(settings, field, &value)) {
        return false;
    }
    if (increase) {
        value = value < minimum ? minimum
                : value < maximum ? (int16_t)(value + 1) : maximum;
    } else {
        value = value > maximum ? maximum
                : value > minimum ? (int16_t)(value - 1) : minimum;
    }
    return gp100_settings_set_i16(settings, field, value);
}

static bool set_boolean_setting(
    gp100_settings_record_t *settings,
    gp100_settings_field_t field,
    bool enabled)
{
    return gp100_settings_set_i16(
        settings, field, enabled ? 1 : 0);
}

bool gp100_global_adjust_panel_setting(
    gp100_settings_record_t *settings,
    gp100_global_menu_item_t item,
    size_t page_index,
    size_t control,
    bool increase)
{
    bool adjusted = false;

    if (settings == NULL || control >= GP100_GLOBAL_CONTROL_SLOTS) {
        return false;
    }
    switch (item) {
    case GP100_GLOBAL_IO:
        if (page_index != 0u) {
            return false;
        }
        if (control == 0u) {
            adjusted = adjust_integer_setting(
                settings, GP100_SETTINGS_INPUT_LEVEL_DB,
                -20, 20, increase);
        } else {
            adjusted = set_boolean_setting(
                settings,
                control == 1u ? GP100_SETTINGS_NO_CAB_LEFT
                              : GP100_SETTINGS_NO_CAB_RIGHT,
                increase);
        }
        break;
    case GP100_GLOBAL_TAP_TEMPO_MODE:
        if (page_index != 0u) {
            return false;
        }
        adjusted = set_boolean_setting(
            settings,
            control == 0u ? GP100_SETTINGS_TAP_SYNC_PRE
            : control == 1u ? GP100_SETTINGS_TAP_SYNC_MODULATION
                            : GP100_SETTINGS_TAP_SYNC_DELAY,
            increase);
        break;
    case GP100_GLOBAL_USB_AUDIO:
        if (page_index == 0u) {
            if (control == 0u) {
                adjusted = adjust_integer_setting(
                    settings, GP100_SETTINGS_USB_RECORD_LEVEL_DB,
                    -20, 20, increase);
            } else if (control == 1u) {
                adjusted = adjust_integer_setting(
                    settings, GP100_SETTINGS_USB_MONITOR_LEVEL_DB,
                    -20, 6, increase);
            } else {
                adjusted = set_boolean_setting(
                    settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT,
                    increase);
            }
        } else if (page_index == 1u && control == 0u) {
            adjusted = set_boolean_setting(
                settings, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT,
                increase);
        } else {
            return false;
        }
        break;
    case GP100_GLOBAL_FOOTSWITCH_MODE:
        if (page_index != 0u || control != 1u) {
            return false;
        }
        adjusted = set_boolean_setting(
            settings, GP100_SETTINGS_FOOTSWITCH_MODE, increase);
        break;
    default:
        return false;
    }
    if (adjusted) {
        gp100_settings_update_crc(settings);
    }
    return adjusted;
}

bool gp100_global_adjust_equalizer(
    gp100_settings_record_t *settings,
    size_t page_index,
    size_t control,
    bool increase,
    uint8_t acceleration)
{
    gp100_sysex_global_state_t equalizer;
    gp100_global_eq_parameter_rule_t rule;
    size_t parameter;
    size_t first;
    float minimum;
    float maximum;
    float step;
    float value;
    float delta;
    unsigned scale_shift;
    unsigned effective_acceleration;
    unsigned step_count;

    if (settings == NULL || page_index >= 5u ||
        control >= GP100_GLOBAL_CONTROL_SLOTS ||
        !gp100_settings_get_global_eq(settings, &equalizer)) {
        return false;
    }
    first = page_index == 0u ? 0u : 3u + (page_index - 1u) * 3u;
    parameter = first + control;
    if (parameter >= GP100_SYSEX_GLOBAL_PARAMETER_COUNT) {
        return false;
    }
    if (!gp100_global_eq_parameter_rule(
            (gp100_global_eq_parameter_t)parameter, &rule)) {
        return false;
    }
    minimum = rule.minimum;
    maximum = rule.maximum;
    step = rule.step;
    memcpy(&value, &equalizer.parameters[parameter], sizeof(value));
    step_count = (unsigned)((maximum - minimum) / step);
    scale_shift = step_count >= 1001u ? 6u : step_count >= 81u ? 1u : 0u;
    effective_acceleration = acceleration == 0u ? 1u
                             : acceleration > 8u ? 5u
                             : (unsigned)acceleration;
    delta = step * (float)(
        1u + ((effective_acceleration - 1u) << scale_shift));
    if (increase) {
        value += delta;
        if (value > maximum) {
            value = maximum;
        }
    } else {
        value -= delta;
        if (value < minimum) {
            value = minimum;
        }
    }
    memcpy(&equalizer.parameters[parameter], &value, sizeof(value));
    if (!gp100_settings_set_global_eq(settings, &equalizer)) {
        return false;
    }
    gp100_settings_update_crc(settings);
    return true;
}

static gp100_global_event_result_t adjust_limiter(
    gp100_settings_record_t *settings, const gp100_ui_control_event_t *event)
{
    const struct limiter_control *control = &limiter_controls[event->control];
    const int step = event->amount != 0u ? (int)event->amount : 1;
    int16_t value;
    if (!gp100_settings_get_i16(settings, control->field, &value)) {
        return GP100_GLOBAL_EVENT_INVALID;
    }
    int next = value + (event->action == GP100_BUTTON_PRESS ? step : -step);
    if (next < control->minimum) {
        next = control->minimum;
    } else if (next > control->maximum) {
        next = control->maximum;
    }
    if (next == value) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    (void)gp100_settings_set_i16(settings, control->field, (int16_t)next);
    return GP100_GLOBAL_EVENT_SETTINGS_CHANGED;
}

static gp100_global_event_result_t navigation_event_result(
    gp100_global_navigation_result_t result,
    gp100_ui_mode_t *next_mode)
{
    switch (result) {
    case GP100_GLOBAL_NAVIGATION_UPDATED:
        return GP100_GLOBAL_EVENT_NAVIGATED;
    case GP100_GLOBAL_NAVIGATION_EXIT_TO_PLAY:
        *next_mode = GP100_UI_MODE_PLAY;
        return GP100_GLOBAL_EVENT_MODE_CHANGED;
    case GP100_GLOBAL_NAVIGATION_UNCHANGED:
        return GP100_GLOBAL_EVENT_UNHANDLED;
    default:
        return GP100_GLOBAL_EVENT_INVALID;
    }
}

static bool initialize_language_controller(
    gp100_global_controller_t *controller,
    const gp100_settings_record_t *settings)
{
    gp100_language_catalog_t catalog;
    int16_t current;

    if (controller->language.catalog.count != 0u) return true;
    if (!gp100_settings_get_i16(
            settings, GP100_SETTINGS_LANGUAGE_INDEX, &current) ||
        current < 0 || current > UINT8_MAX) {
        current = 0;
    }
    gp100_language_catalog_from_mask(0u, &catalog);
    return gp100_language_screen_init(
        &controller->language, &catalog, (uint8_t)current);
}

static gp100_global_event_result_t handle_language_event(
    gp100_global_controller_t *controller,
    gp100_settings_record_t *settings,
    const gp100_ui_control_event_t *decoded,
    gp100_ui_mode_t *next_mode)
{
    int step;

    if (!initialize_language_controller(controller, settings)) {
        return GP100_GLOBAL_EVENT_INVALID;
    }
    if (decoded->kind == GP100_UI_CONTROL_ENCODER &&
        decoded->control == GP100_ENCODER_PARA &&
        !controller->language.committed) {
        step = decoded->amount == 0u ? 1 : (int)decoded->amount;
        if (decoded->action != GP100_BUTTON_PRESS) step = -step;
        return gp100_language_screen_move(
                   &controller->language, step)
            ? GP100_GLOBAL_EVENT_NAVIGATED
            : GP100_GLOBAL_EVENT_INVALID;
    }
    if (decoded->kind != GP100_UI_CONTROL_BUTTON ||
        decoded->action != GP100_BUTTON_RELEASE ||
        (decoded->control != GP100_BUTTON_PARA &&
         decoded->control != GP100_BUTTON_EXIT)) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    if (decoded->control == GP100_BUTTON_PARA &&
        !controller->language.committed) {
        return gp100_language_screen_commit(
                   &controller->language, settings, NULL)
            ? GP100_GLOBAL_EVENT_SETTINGS_CHANGED
            : GP100_GLOBAL_EVENT_INVALID;
    }
    return navigation_event_result(
        gp100_global_controller_exit(controller), next_mode);
}

static gp100_global_event_result_t handle_factory_reset_event(
    gp100_global_controller_t *controller,
    const gp100_ui_control_event_t *decoded,
    gp100_ui_mode_t *next_mode)
{
    bool reset_requested;
    bool exit_requested;

    if (decoded->kind == GP100_UI_CONTROL_ENCODER &&
        decoded->control == GP100_ENCODER_PARA &&
        controller->factory_reset.phase ==
            GP100_FACTORY_RESET_CONFIRM) {
        return gp100_factory_reset_select(
                   &controller->factory_reset,
                   decoded->action == GP100_BUTTON_PRESS)
            ? GP100_GLOBAL_EVENT_NAVIGATED
            : GP100_GLOBAL_EVENT_INVALID;
    }
    if (decoded->kind != GP100_UI_CONTROL_BUTTON ||
        decoded->action != GP100_BUTTON_RELEASE ||
        (decoded->control != GP100_BUTTON_PARA &&
         decoded->control != GP100_BUTTON_EXIT)) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    if (decoded->control == GP100_BUTTON_EXIT) {
        if (controller->factory_reset.phase !=
                GP100_FACTORY_RESET_CONFIRM) {
            return GP100_GLOBAL_EVENT_UNHANDLED;
        }
        return navigation_event_result(
            gp100_global_controller_exit(controller), next_mode);
    }
    if (controller->factory_reset.phase ==
            GP100_FACTORY_RESET_COMPLETE) {
        return navigation_event_result(
            gp100_global_controller_exit(controller), next_mode);
    }
    if (!gp100_factory_reset_activate(
            &controller->factory_reset,
            &reset_requested, &exit_requested)) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    if (reset_requested) {
        return GP100_GLOBAL_EVENT_FACTORY_RESET_REQUESTED;
    }
    if (exit_requested) {
        return navigation_event_result(
            gp100_global_controller_exit(controller), next_mode);
    }
    return GP100_GLOBAL_EVENT_INVALID;
}

static gp100_global_event_result_t handle_calibration_event(
    gp100_global_controller_t *controller,
    const gp100_ui_control_event_t *decoded,
    gp100_ui_mode_t *next_mode)
{
    bool committed;
    uint16_t observation;

    if (decoded->kind == GP100_UI_CONTROL_EXPRESSION_POSITION ||
        decoded->kind == GP100_UI_CONTROL_EXPRESSION_ACTIVATE) {
        controller->calibration_raw = decoded->sample;
        if (decoded->sample > controller->calibration_peak) {
            controller->calibration_peak = decoded->sample;
        }
        return GP100_GLOBAL_EVENT_NAVIGATED;
    }
    if (decoded->kind != GP100_UI_CONTROL_BUTTON ||
        decoded->action != GP100_BUTTON_RELEASE ||
        (decoded->control != GP100_BUTTON_PARA &&
         decoded->control != GP100_BUTTON_EXIT)) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    if (decoded->control == GP100_BUTTON_EXIT) {
        return navigation_event_result(
            gp100_global_controller_exit(controller), next_mode);
    }
    const gp100_expression_calibration_step_t step =
        controller->expression_calibration.step;
    if (step == GP100_EXPRESSION_CALIBRATE_COMPLETE) {
        return navigation_event_result(
            gp100_global_controller_exit(controller), next_mode);
    }
    observation = step == GP100_EXPRESSION_CALIBRATE_SWITCH
        ? controller->calibration_peak
        : controller->calibration_raw;
    if (!gp100_expression_calibration_advance(
            &controller->expression_calibration,
            observation, &committed)) {
        return GP100_GLOBAL_EVENT_INVALID;
    }
    if (step == GP100_EXPRESSION_CALIBRATE_TOE) {
        controller->calibration_peak = controller->calibration_raw;
    }
    return committed ? GP100_GLOBAL_EVENT_CALIBRATION_COMMIT
                     : GP100_GLOBAL_EVENT_NAVIGATED;
}

gp100_global_event_result_t
gp100_global_controller_handle_expression_event(
    const gp100_global_controller_t *controller,
    gp100_preset_record_t *preset,
    const gp100_event_t *event)
{
    gp100_ui_control_event_t decoded;
    int step;
    bool changed = false;

    if (controller == NULL || preset == NULL || event == NULL) {
        return GP100_GLOBAL_EVENT_INVALID;
    }
    if (controller->view != GP100_GLOBAL_VIEW_EXPRESSION ||
        !gp100_decode_ui_control_event(event, &decoded) ||
        decoded.kind != GP100_UI_CONTROL_ENCODER ||
        decoded.control >= GP100_ENCODER_PARA) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    step = decoded.amount == 0u ? 1 : (int)decoded.amount;
    if (decoded.action != GP100_BUTTON_PRESS) step = -step;
    switch (controller->expression_item) {
    case GP100_EXPRESSION_VOLUME_RANGE:
        if (decoded.control == GP100_ENCODER_QUICK_1 ||
            decoded.control == GP100_ENCODER_QUICK_3) {
            changed = gp100_expression_adjust_volume_range(
                preset,
                decoded.control == GP100_ENCODER_QUICK_3,
                step);
        } else if (decoded.control == GP100_ENCODER_QUICK_2) {
            changed = gp100_preset_set_volume_pedal_position(
                preset, step > 0 ? GP100_VOLUME_PEDAL_POST
                                 : GP100_VOLUME_PEDAL_PRE);
        }
        break;
    case GP100_EXPRESSION_CALIBRATE:
        break;
    default:
        return GP100_GLOBAL_EVENT_INVALID;
    }
    if (!changed) return GP100_GLOBAL_EVENT_UNHANDLED;
    gp100_preset_update_crc(preset);
    return GP100_GLOBAL_EVENT_PRESET_CHANGED;
}

gp100_global_event_result_t gp100_global_controller_handle_event(
    gp100_global_controller_t *controller,
    gp100_settings_record_t *settings,
    const gp100_event_t *event,
    gp100_ui_mode_t *next_mode)
{
    gp100_ui_control_event_t decoded;
    gp100_global_screen_snapshot_t snapshot;
    gp100_global_navigation_result_t navigation;
    bool adjusted;

    if (controller == NULL || settings == NULL || event == NULL ||
        next_mode == NULL ||
        !gp100_decode_ui_control_event(event, &decoded)) {
        return GP100_GLOBAL_EVENT_INVALID;
    }
    if (controller->view == GP100_GLOBAL_VIEW_FACTORY_RESET &&
        (controller->factory_reset.phase ==
             GP100_FACTORY_RESET_REQUESTED ||
         controller->factory_reset.phase ==
             GP100_FACTORY_RESET_PROGRESS)) {
        return handle_factory_reset_event(controller, &decoded, next_mode);
    }
    if (decoded.kind == GP100_UI_CONTROL_FOOTSWITCH_PAIR &&
        decoded.action == GP100_BUTTON_REPEAT) {
        *next_mode = GP100_UI_MODE_LOOPER;
        return GP100_GLOBAL_EVENT_MODE_CHANGED;
    }
    if (decoded.kind == GP100_UI_CONTROL_BUTTON) {
        if ((decoded.control == GP100_BUTTON_FOOTSWITCH_MINUS ||
             decoded.control == GP100_BUTTON_FOOTSWITCH_PLUS) &&
            decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_PLAY;
            return GP100_GLOBAL_EVENT_MODE_CHANGED;
        }
        if (decoded.control == GP100_BUTTON_GLOBAL &&
            decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_GLOBAL;
            return GP100_GLOBAL_EVENT_MODE_CHANGED;
        }
        if (decoded.control == GP100_BUTTON_DRUM) {
            if (decoded.action == GP100_BUTTON_RELEASE) {
                return GP100_GLOBAL_EVENT_DRUM_TOGGLE_REQUESTED;
            }
            if (decoded.action == GP100_BUTTON_HOLD) {
                *next_mode = GP100_UI_MODE_DRUM;
                return GP100_GLOBAL_EVENT_MODE_CHANGED;
            }
        }
        if (decoded.control == GP100_BUTTON_EDIT &&
            decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_EDIT;
            return GP100_GLOBAL_EVENT_MODE_CHANGED;
        }
        if (decoded.control == GP100_BUTTON_SAVE &&
            decoded.action == GP100_BUTTON_RELEASE) {
            *next_mode = GP100_UI_MODE_SAVE;
            return GP100_GLOBAL_EVENT_MODE_CHANGED;
        }
    }
    if (controller->view == GP100_GLOBAL_VIEW_LANGUAGE) {
        return handle_language_event(
            controller, settings, &decoded, next_mode);
    }
    if (controller->view == GP100_GLOBAL_VIEW_FACTORY_RESET) {
        return handle_factory_reset_event(controller, &decoded, next_mode);
    }
    if (controller->view ==
            GP100_GLOBAL_VIEW_EXPRESSION_CALIBRATION) {
        return handle_calibration_event(controller, &decoded, next_mode);
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER &&
        decoded.control == GP100_ENCODER_PARA) {
        navigation = gp100_global_controller_move(
            controller, decoded.action == GP100_BUTTON_PRESS);
        return navigation_event_result(navigation, next_mode);
    }
    if (decoded.kind == GP100_UI_CONTROL_ENCODER &&
        decoded.control < GP100_ENCODER_PARA) {
        if (controller->view != GP100_GLOBAL_VIEW_MENU ||
            !gp100_global_controller_snapshot(
                controller, &snapshot, settings, NULL) ||
            !snapshot.controls[decoded.control].present) {
            return GP100_GLOBAL_EVENT_UNHANDLED;
        }
        if (controller->menu_item == GP100_GLOBAL_OUTPUT_LIMITER) {
            return adjust_limiter(settings, &decoded);
        }
        if (controller->menu_item == GP100_GLOBAL_EQ) {
            adjusted = gp100_global_adjust_equalizer(
                settings, controller->page_index, decoded.control,
                decoded.action == GP100_BUTTON_PRESS,
                decoded.amount);
        } else {
            adjusted = gp100_global_adjust_panel_setting(
                settings, controller->menu_item, controller->page_index,
                decoded.control,
                decoded.action == GP100_BUTTON_PRESS);
        }
        return adjusted ? GP100_GLOBAL_EVENT_SETTINGS_CHANGED
                        : GP100_GLOBAL_EVENT_INVALID;
    }
    if (decoded.kind != GP100_UI_CONTROL_BUTTON ||
        decoded.action != GP100_BUTTON_RELEASE) {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    if (decoded.control == GP100_BUTTON_PARA) {
        if (controller->view == GP100_GLOBAL_VIEW_MENU &&
            controller->menu_item == GP100_GLOBAL_OUTPUT_LIMITER) {
            int16_t enabled;
            if (!gp100_settings_get_i16(settings, GP100_SETTINGS_LIMITER_ENABLED, &enabled) ||
                !set_boolean_setting(settings, GP100_SETTINGS_LIMITER_ENABLED, enabled == 0)) {
                return GP100_GLOBAL_EVENT_INVALID;
            }
            return GP100_GLOBAL_EVENT_SETTINGS_CHANGED;
        }
        navigation = gp100_global_controller_activate(controller);
    } else if (decoded.control == GP100_BUTTON_EXIT) {
        navigation = gp100_global_controller_exit(controller);
    } else {
        return GP100_GLOBAL_EVENT_UNHANDLED;
    }
    return navigation_event_result(navigation, next_mode);
}

static bool read_setting(const gp100_settings_record_t *settings,
                         gp100_settings_field_t field,
                         int16_t *value)
{
    return gp100_settings_get_i16(settings, field, value);
}

static void append_character(char *text, size_t capacity, char character)
{
    size_t length = strlen(text);
    if (length + 1u < capacity) {
        text[length] = character;
        text[length + 1u] = '\0';
    }
}

static void format_unsigned(char *output, size_t capacity, unsigned value)
{
    char reversed[11];
    size_t count = 0u;
    size_t index;

    if (capacity == 0u) {
        return;
    }
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && count < sizeof(reversed));
    index = 0u;
    while (count != 0u && index + 1u < capacity) {
        output[index++] = reversed[--count];
    }
    output[index] = '\0';
}

static void format_db(char *output, size_t capacity, int value)
{
    unsigned magnitude;

    if (capacity == 0u) {
        return;
    }
    output[0] = '\0';
    if (value < 0) {
        append_character(output, capacity, '-');
        magnitude = (unsigned)(-value);
    } else {
        if (value > 0) {
            append_character(output, capacity, '+');
        }
        magnitude = (unsigned)value;
    }
    {
        char digits[11];
        const char *cursor;
        format_unsigned(digits, sizeof(digits), magnitude);
        for (cursor = digits; *cursor != '\0'; ++cursor) {
            append_character(output, capacity, *cursor);
        }
    }
    append_character(output, capacity, 'd');
    append_character(output, capacity, 'B');
}

static void format_frequency(char *output, size_t capacity, float value)
{
    if (!(value >= 1.0f)) {
        (void)copy_text(output, capacity, "OFF");
        return;
    }
    format_unsigned(output, capacity, (unsigned)(value + 0.5f));
    append_character(output, capacity, 'H');
    append_character(output, capacity, 'z');
}

static void format_equalizer_cut_frequency(
    char *output,
    size_t capacity,
    size_t parameter,
    float value)
{
    if ((parameter == GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY &&
         value == 19.0f) ||
        (parameter == GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY &&
         value == 20001.0f)) {
        (void)copy_text(output, capacity, "OFF");
        return;
    }
    format_frequency(output, capacity, value);
}

static void format_tenth(char *output, size_t capacity, float value)
{
    unsigned scaled;

    if (value < 0.0f) {
        value = -value;
        (void)copy_text(output, capacity, "-");
    } else {
        output[0] = '\0';
    }
    scaled = (unsigned)(value * 10.0f + 0.5f);
    {
        char whole[11];
        format_unsigned(whole, sizeof(whole), scaled / 10u);
        for (const char *cursor = whole; *cursor != '\0'; ++cursor) {
            append_character(output, capacity, *cursor);
        }
    }
    append_character(output, capacity, '.');
    append_character(output, capacity, (char)('0' + scaled % 10u));
}

static bool set_control(gp100_global_control_t *control,
                        const char *label,
                        const char *value,
                        float raw_value,
                        float minimum,
                        float maximum)
{
    if (!copy_text(control->label, sizeof(control->label), label) ||
        !copy_text(control->value, sizeof(control->value), value)) {
        return false;
    }
    control->raw_value = raw_value;
    control->minimum = minimum;
    control->maximum = maximum;
    control->present = true;
    return true;
}

static float word_as_float(uint32_t word)
{
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool fill_io(gp100_global_screen_snapshot_t *snapshot,
                    const gp100_settings_record_t *settings)
{
    int16_t input;
    int16_t left;
    int16_t right;
    char value[20];

    if (!read_setting(settings, GP100_SETTINGS_INPUT_LEVEL_DB, &input) ||
        !read_setting(settings, GP100_SETTINGS_NO_CAB_LEFT, &left) ||
        !read_setting(settings, GP100_SETTINGS_NO_CAB_RIGHT, &right)) {
        return false;
    }
    format_db(value, sizeof(value), input);
    return set_control(&snapshot->controls[0], "Input\nLevel", value,
                       (float)input, -20.0f, 20.0f) &&
           set_control(&snapshot->controls[1], "No CAB\nMode(L)",
                       left != 0 ? "ON" : "OFF", (float)(left != 0),
                       0.0f, 1.0f) &&
           set_control(&snapshot->controls[2], "No CAB\nMode(R)",
                       right != 0 ? "ON" : "OFF", (float)(right != 0),
                       0.0f, 1.0f);
}

static bool fill_tap(gp100_global_screen_snapshot_t *snapshot,
                     const gp100_settings_record_t *settings)
{
    static const gp100_settings_field_t fields[3] = {
        GP100_SETTINGS_TAP_SYNC_PRE,
        GP100_SETTINGS_TAP_SYNC_MODULATION,
        GP100_SETTINGS_TAP_SYNC_DELAY
    };
    static const char *const labels[3] = {"PRE", "MOD", "DLY"};
    size_t index;

    for (index = 0u; index < 3u; ++index) {
        int16_t value;
        if (!read_setting(settings, fields[index], &value) ||
            !set_control(&snapshot->controls[index], labels[index],
                         value != 0 ? "OPEN" : "CLOSE",
                         (float)(value != 0), 0.0f, 1.0f)) {
            return false;
        }
    }
    return true;
}

static bool fill_usb(gp100_global_screen_snapshot_t *snapshot,
                     const gp100_settings_record_t *settings)
{
    int16_t record_level;
    int16_t monitor_level;
    int16_t left;
    int16_t right;
    char record_text[20];
    char monitor_text[20];

    if (!read_setting(settings, GP100_SETTINGS_USB_RECORD_LEVEL_DB,
                      &record_level) ||
        !read_setting(settings, GP100_SETTINGS_USB_MONITOR_LEVEL_DB,
                      &monitor_level) ||
        !read_setting(settings, GP100_SETTINGS_USB_RECORD_SOURCE_LEFT,
                      &left) ||
        !read_setting(settings, GP100_SETTINGS_USB_RECORD_SOURCE_RIGHT,
                      &right)) {
        return false;
    }
    if (snapshot->page_index == 1u) {
        return set_control(&snapshot->controls[0], "RecMode\n(R)",
                           right == 0 ? "DRY" : "EFFECT",
                           (float)(right != 0), 0.0f, 1.0f);
    }
    format_db(record_text, sizeof(record_text), record_level);
    format_db(monitor_text, sizeof(monitor_text), monitor_level);
    return set_control(&snapshot->controls[0], "Rec\nLevel", record_text,
                       (float)record_level, -20.0f, 20.0f) &&
           set_control(&snapshot->controls[1], "Monitor\nLevel", monitor_text,
                       (float)monitor_level, -20.0f, 6.0f) &&
           set_control(&snapshot->controls[2], "RecMode\n(L)",
                       left == 0 ? "DRY" : "EFFECT",
                       (float)(left != 0), 0.0f, 1.0f);
}

static bool fill_global_eq(gp100_global_screen_snapshot_t *snapshot,
                           const gp100_settings_record_t *settings)
{
    gp100_sysex_global_state_t equalizer;
    size_t first = snapshot->page_index == 0u
                       ? 0u : 3u + (snapshot->page_index - 1u) * 3u;
    size_t index;

    if (!gp100_settings_get_global_eq(settings, &equalizer)) {
        return false;
    }
    for (index = 0u; index < 3u; ++index) {
        size_t parameter = first + index;
        float value = word_as_float(equalizer.parameters[parameter]);
        char label[20];
        char text[20];

        if (parameter == GP100_GLOBAL_EQ_ENABLE) {
            (void)copy_text(label, sizeof(label), "ON/OFF");
            (void)copy_text(text, sizeof(text), value != 0.0f ? "ON" : "OFF");
        } else if (parameter <= GP100_GLOBAL_EQ_HIGH_CUT_FREQUENCY) {
            (void)copy_text(label, sizeof(label),
                            parameter == GP100_GLOBAL_EQ_LOW_CUT_FREQUENCY
                                ? "L-CUT FREQ" : "H-CUT FREQ");
            format_equalizer_cut_frequency(
                text, sizeof(text), parameter, value);
        } else {
            unsigned band = (unsigned)((parameter - 3u) / 3u) + 1u;
            unsigned kind = (unsigned)((parameter - 3u) % 3u);
            label[0] = 'B';
            label[1] = (char)('0' + band);
            label[2] = ' ';
            label[3] = '\0';
            (void)copy_text(label + 3u, sizeof(label) - 3u,
                            kind == 0u ? "FREQ" : kind == 1u ? "Q" : "GAIN");
            if (kind == 0u) {
                format_frequency(text, sizeof(text), value);
            } else if (kind == 1u) {
                format_tenth(text, sizeof(text), value);
            } else {
                format_db(text, sizeof(text), (int)(value >= 0.0f
                                                        ? value + 0.5f
                                                        : value - 0.5f));
            }
        }
        if (!set_control(&snapshot->controls[index], label, text, value,
                         0.0f, 1.0f)) {
            return false;
        }
    }
    return true;
}

static bool fill_limiter(gp100_global_screen_snapshot_t *snapshot,
                         const gp100_settings_record_t *settings)
{
    int16_t enabled;
    if (!read_setting(settings, GP100_SETTINGS_LIMITER_ENABLED, &enabled)) {
        return false;
    }
    snapshot->limiter_enabled = enabled != 0;
    for (size_t index = 0u; index < GP100_GLOBAL_CONTROL_SLOTS; ++index) {
        const struct limiter_control *control = &limiter_controls[index];
        int16_t value;
        char text[12];
        if (!read_setting(settings, control->field, &value)) {
            return false;
        }
        if (control->field == GP100_SETTINGS_LIMITER_RELEASE_DB_PER_SEC) {
            format_unsigned(text, sizeof(text), (unsigned)value);
            append_character(text, sizeof(text), 'd');
            append_character(text, sizeof(text), 'B');
            append_character(text, sizeof(text), '/');
            append_character(text, sizeof(text), 's');
        } else {
            format_db(text, sizeof(text), value);
        }
        if (!set_control(&snapshot->controls[index], control->label, text,
                         (float)value, (float)control->minimum, (float)control->maximum)) {
            return false;
        }
    }
    return true;
}

bool gp100_global_screen_snapshot_from_settings(
    gp100_global_screen_snapshot_t *snapshot,
    const gp100_settings_record_t *settings,
    gp100_global_menu_item_t selected_item,
    size_t page_index,
    const char *firmware_version)
{
    gp100_global_screen_snapshot_t next;
    int16_t value;

    if (snapshot == NULL || settings == NULL ||
        (unsigned)selected_item >= GP100_GLOBAL_MENU_ITEMS) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.selected_item = selected_item;
    next.page_count = selected_item == GP100_GLOBAL_USB_AUDIO
                          ? 2u
                          : selected_item == GP100_GLOBAL_EQ ? 5u : 1u;
    if (page_index >= next.page_count) {
        return false;
    }
    next.page_index = page_index;
    if ((size_t)selected_item < GP100_GLOBAL_VISIBLE_ITEMS) {
        next.first_visible_item = 0u;
    } else if ((size_t)selected_item + 1u >= GP100_GLOBAL_MENU_ITEMS) {
        next.first_visible_item = GP100_GLOBAL_MENU_ITEMS -
                                  GP100_GLOBAL_VISIBLE_ITEMS;
    } else {
        next.first_visible_item = (size_t)selected_item - 1u;
    }

    switch (selected_item) {
    case GP100_GLOBAL_IO:
        if (!fill_io(&next, settings)) return false;
        break;
    case GP100_GLOBAL_TAP_TEMPO_MODE:
        if (!fill_tap(&next, settings)) return false;
        break;
    case GP100_GLOBAL_EXPRESSION_SETTING:
        (void)copy_text(next.detail, sizeof(next.detail), "Enter Setting");
        break;
    case GP100_GLOBAL_USB_AUDIO:
        if (!fill_usb(&next, settings)) return false;
        break;
    case GP100_GLOBAL_FOOTSWITCH_MODE:
        if (!read_setting(settings, GP100_SETTINGS_FOOTSWITCH_MODE,
                          &value) ||
            !set_control(&next.controls[1], "Mode",
                         value == GP100_FOOTSWITCH_MODE_STOMP
                             ? "STOMP" : "PATCH",
                         (float)value, 0.0f, 1.0f)) return false;
        break;
    case GP100_GLOBAL_EQ:
        if (!fill_global_eq(&next, settings)) return false;
        break;
    case GP100_GLOBAL_OUTPUT_LIMITER:
        if (!fill_limiter(&next, settings)) return false;
        break;
    case GP100_GLOBAL_LANGUAGE:
        (void)copy_text(next.detail, sizeof(next.detail), "Enter Setting");
        break;
    case GP100_GLOBAL_FACTORY_RESET:
        (void)copy_text(next.detail, sizeof(next.detail), "Enter Setting");
        break;
    case GP100_GLOBAL_ABOUT:
        (void)copy_text(next.detail, sizeof(next.detail),
                        firmware_version != NULL ? firmware_version : "2.1.0");
        break;
    }
    *snapshot = next;
    return true;
}

bool gp100_global_screen_default_theme(
    gp100_global_screen_theme_t *theme,
    const gp100_bitmap_font_t *large_font,
    const gp100_bitmap_font_t *regular_font,
    const gp100_bitmap_font_t *small_font)
{
    if (theme == NULL || regular_font == NULL) {
        return false;
    }
    theme->large_font = large_font != NULL ? large_font : regular_font;
    theme->regular_font = regular_font;
    theme->small_font = small_font != NULL ? small_font : regular_font;
    theme->background_rgb565 = GP100_UI_BACKGROUND;
    theme->foreground_rgb565 = GP100_UI_FOREGROUND;
    theme->selected_rgb565 = GP100_UI_ACCENT;
    theme->panel_rgb565 = GP100_UI_PANEL;
    theme->value_rgb565 = GP100_UI_CONTROL;
    theme->track_rgb565 = GP100_UI_TRACK;
    return true;
}

static bool draw_text(gp100_display_canvas_t *canvas,
                      const gp100_bitmap_font_t *font,
                      int32_t x,
                      int32_t y,
                      const char *text,
                      gp100_text_alignment_t alignment,
                      uint16_t foreground,
                      uint16_t background)
{
    gp100_text_style_t style = {
        foreground, background, alignment, true
    };
    return gp100_display_draw_text(canvas, font, x, y, text, &style);
}

static void format_page(char output[8], size_t page, size_t total)
{
    format_unsigned(output, 8u, (unsigned)(page + 1u));
    append_character(output, 8u, '/');
    {
        char value[4];
        format_unsigned(value, sizeof(value), (unsigned)total);
        for (const char *cursor = value; *cursor != '\0'; ++cursor) {
            append_character(output, 8u, *cursor);
        }
    }
}

/* GP100_UI_ASSET_REPLACEMENT: screen composition and widget geometry. */
bool gp100_global_screen_render(
    gp100_display_canvas_t *canvas,
    const gp100_global_screen_snapshot_t *snapshot,
    const gp100_global_screen_theme_t *theme)
{
    size_t row;
    char page[8];

    if (canvas == NULL || snapshot == NULL || theme == NULL ||
        canvas->width < GP100_UI_LOGICAL_WIDTH ||
        canvas->height < GP100_UI_LOGICAL_HEIGHT ||
        (unsigned)snapshot->selected_item >= GP100_GLOBAL_MENU_ITEMS ||
        snapshot->page_count == 0u ||
        snapshot->page_index >= snapshot->page_count ||
        snapshot->first_visible_item >
            GP100_GLOBAL_MENU_ITEMS - GP100_GLOBAL_VISIBLE_ITEMS ||
        theme->large_font == NULL || theme->regular_font == NULL ||
        theme->small_font == NULL) {
        return false;
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 0, GP100_UI_LOGICAL_WIDTH,
        GP100_UI_LOGICAL_HEIGHT, theme->background_rgb565);
    if (!draw_text(canvas, theme->large_font, 5, 4, "GLOBAL",
                   GP100_TEXT_ALIGN_LEFT, theme->foreground_rgb565,
                   theme->background_rgb565)) {
        return false;
    }
    for (row = 0u; row < GP100_GLOBAL_VISIBLE_ITEMS; ++row) {
        size_t item = snapshot->first_visible_item + row;
        int32_t y = 22 + (int32_t)row * 18;
        bool selected = item == (size_t)snapshot->selected_item;
        if (selected) {
            (void)gp100_display_fill_rect(
                canvas, 2, y - 2, 154u, 17u, theme->selected_rgb565);
        }
        if (!draw_text(canvas, theme->regular_font, 7, y,
                       menu_names[item], GP100_TEXT_ALIGN_LEFT,
                       selected ? theme->background_rgb565
                                : theme->foreground_rgb565,
                       selected ? theme->selected_rgb565
                                : theme->background_rgb565)) {
            return false;
        }
    }
    (void)gp100_display_fill_rect(
        canvas, 156, 20, 3u, 56u, theme->track_rgb565);
    {
        uint16_t thumb = 18u;
        uint16_t y = (uint16_t)(20u +
            snapshot->first_visible_item * (56u - thumb) /
            (GP100_GLOBAL_MENU_ITEMS - GP100_GLOBAL_VISIBLE_ITEMS));
        (void)gp100_display_fill_rect(
            canvas, 156, y, 3u, thumb, theme->foreground_rgb565);
    }
    (void)gp100_display_fill_rect(
        canvas, 0, 81, GP100_UI_LOGICAL_WIDTH, 47u,
        theme->panel_rgb565);
    const bool limiter = snapshot->selected_item == GP100_GLOBAL_OUTPUT_LIMITER;
    if (limiter && !draw_text(canvas, theme->small_font, 157, 82,
                              snapshot->limiter_enabled ? "PARA ON" : "PARA OFF",
                              GP100_TEXT_ALIGN_RIGHT, theme->foreground_rgb565,
                              theme->panel_rgb565)) {
        return false;
    }
    if (snapshot->page_count > 1u) {
        format_page(page, snapshot->page_index, snapshot->page_count);
        if (!draw_text(canvas, theme->small_font, 157, 83, page,
                       GP100_TEXT_ALIGN_RIGHT,
                       theme->foreground_rgb565, theme->panel_rgb565)) {
            return false;
        }
    }
    if (snapshot->detail[0] != '\0' &&
        !draw_text(canvas, theme->regular_font, 80, 103, snapshot->detail,
                   GP100_TEXT_ALIGN_CENTER,
                   theme->foreground_rgb565, theme->panel_rgb565)) {
        return false;
    }
    for (row = 0u; row < GP100_GLOBAL_CONTROL_SLOTS; ++row) {
        const gp100_global_control_t *control = &snapshot->controls[row];
        if (!control->present) {
            continue;
        }
        if (!draw_text(canvas, theme->small_font, control_center[row], limiter ? 94 : 86,
                       control->label, GP100_TEXT_ALIGN_CENTER,
                       theme->foreground_rgb565, theme->panel_rgb565) ||
            !draw_text(canvas, limiter ? theme->small_font : theme->regular_font,
                       control_center[row], 111,
                       control->value, GP100_TEXT_ALIGN_CENTER,
                       theme->selected_rgb565, theme->value_rgb565)) {
            return false;
        }
    }
    return true;
}
