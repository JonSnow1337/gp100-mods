#ifndef GP100_EVENT_DISPATCH_H
#define GP100_EVENT_DISPATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EXPRESSION_PEDAL_CHANNEL UINT32_C(1)
#define GP100_EXPRESSION_POSITION_EVENT_PREFIX UINT32_C(0x11000000)
#define GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX UINT32_C(0x61000000)
#define GP100_EXPRESSION_ACTIVATION_HOLD_MS 50u

/* Low word first, matching the little-endian stack object passed by V2.1.0. */
typedef struct gp100_event {
    uint32_t value;
    uint32_t code;
} gp100_event_t;

typedef void (*gp100_event_handler_t)(void *context,
                                           const gp100_event_t *event,
                                           uint32_t source);
typedef void (*gp100_tick_handler_t)(void *context);

typedef struct gp100_event_hooks {
    void *context;
    bool (*pop_event)(void *context, gp100_event_t *event);
    uint32_t (*read_expression_value)(void *context, uint32_t channel);
    gp100_event_handler_t (*resolve_event_handler)(void *context,
                                                        uint16_t mode);
    gp100_tick_handler_t (*resolve_tick_handler)(void *context,
                                                      uint16_t mode);
} gp100_event_hooks_t;

typedef struct gp100_event_router {
    uint16_t current_mode;
    uint16_t expression_toe;
    uint16_t expression_activation_threshold;
    uint16_t previous_expression_value;
    bool expression_activation_pending;
    bool expression_activation_latched;
    uint32_t expression_activation_started_ms;
    uint32_t last_dispatched_tick;
    uint32_t expression_position_event_count;
    uint32_t expression_activation_event_count;
} gp100_event_router_t;

typedef enum gp100_expression_route_result {
    GP100_EXPRESSION_ROUTE_INVALID = -1,
    GP100_EXPRESSION_ROUTE_UNCHANGED = 0,
    GP100_EXPRESSION_ROUTE_POSITION,
    GP100_EXPRESSION_ROUTE_ACTIVATION
} gp100_expression_route_result_t;

bool gp100_event_hooks_are_valid(const gp100_event_hooks_t *hooks);

/*
 * Report raw positions during travel and toe-switch debounce. A sustained
 * press toggles the target once, until the pedal returns to the calibrated toe.
 */
gp100_expression_route_result_t
gp100_event_router_sample_expression(
    gp100_event_router_t *router,
    uint16_t expression_value,
    uint32_t now_ms,
    gp100_event_t *event);

/*
 * Drain queued events, then sample the pedal. Return the number of handler
 * invocations.
 */
size_t gp100_dispatch_pending_events(
    gp100_event_router_t *router,
    uint32_t now_ms,
    const gp100_event_hooks_t *hooks);

/* Dispatch the active mode's periodic callback once per new event tick. */
bool gp100_dispatch_mode_tick(
    gp100_event_router_t *router,
    uint32_t event_tick,
    const gp100_event_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif
