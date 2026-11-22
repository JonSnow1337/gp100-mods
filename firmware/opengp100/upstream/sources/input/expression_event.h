#ifndef GP100_EXPRESSION_EVENT_H
#define GP100_EXPRESSION_EVENT_H

#include "expression_runtime.h"
#include "ui/ui_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gp100_expression_event_status {
    GP100_EXPRESSION_EVENT_INVALID = -1,
    GP100_EXPRESSION_EVENT_UNHANDLED = 0,
    GP100_EXPRESSION_EVENT_POSITION,
    GP100_EXPRESSION_EVENT_TARGET_TOGGLED
} gp100_expression_event_status_t;

typedef struct gp100_expression_event_result {
    gp100_expression_apply_result_t apply;
    gp100_expression_pedal_target_t active_target;
} gp100_expression_event_result_t;

/*
 * Handle the expression classes shared by the seven normal mode handlers.
 * Hardware Test owns its diagnostic pedal path. Other controls are returned
 * as UNHANDLED so this function can precede the ordinary screen controller.
 */
gp100_expression_event_status_t gp100_expression_event_handle(
    gp100_ui_mode_t mode,
    gp100_preset_record_t *preset,
    gp100_effect_runtime_t *effects,
    uint16_t calibration_heel,
    uint16_t calibration_toe,
    const gp100_event_t *event,
    gp100_expression_event_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
