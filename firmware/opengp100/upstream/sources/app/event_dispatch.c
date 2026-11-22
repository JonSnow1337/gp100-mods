#include "event_dispatch.h"

#include <stddef.h>

bool gp100_event_hooks_are_valid(const gp100_event_hooks_t *hooks)
{
    return hooks != NULL &&
           hooks->pop_event != NULL &&
           hooks->read_expression_value != NULL &&
           hooks->resolve_event_handler != NULL &&
           hooks->resolve_tick_handler != NULL;
}

static bool dispatch_event(gp100_event_router_t *router,
                           const gp100_event_hooks_t *hooks,
                           const gp100_event_t *event)
{
    gp100_event_handler_t handler;

    handler = hooks->resolve_event_handler(hooks->context,
                                           router->current_mode);
    if (handler == NULL) {
        return false;
    }

    handler(hooks->context, event, 0u);
    return true;
}

gp100_expression_route_result_t
gp100_event_router_sample_expression(
    gp100_event_router_t *router,
    uint16_t expression_value,
    uint32_t now_ms,
    gp100_event_t *event)
{
    if (router == NULL || event == NULL ||
        router->expression_toe >= router->expression_activation_threshold) {
        return GP100_EXPRESSION_ROUTE_INVALID;
    }
    if (expression_value < router->expression_activation_threshold) {
        router->expression_activation_pending = false;
        if (expression_value <= router->expression_toe) {
            router->expression_activation_latched = false;
        }
    } else if (!router->expression_activation_latched) {
        if (!router->expression_activation_pending) {
            router->expression_activation_pending = true;
            router->expression_activation_started_ms = now_ms;
        } else if (now_ms - router->expression_activation_started_ms >=
                   GP100_EXPRESSION_ACTIVATION_HOLD_MS) {
            router->expression_activation_pending = false;
            router->expression_activation_latched = true;
            router->previous_expression_value = expression_value;
            event->value = 1u;
            event->code = GP100_EXPRESSION_ACTIVATE_EVENT_PREFIX | expression_value;
            ++router->expression_activation_event_count;
            return GP100_EXPRESSION_ROUTE_ACTIVATION;
        }
    }

    if (expression_value == router->previous_expression_value) {
        return GP100_EXPRESSION_ROUTE_UNCHANGED;
    }
    router->previous_expression_value = expression_value;
    event->value = 0u;
    event->code = GP100_EXPRESSION_POSITION_EVENT_PREFIX | expression_value;
    ++router->expression_position_event_count;
    return GP100_EXPRESSION_ROUTE_POSITION;
}

size_t gp100_dispatch_pending_events(
    gp100_event_router_t *router,
    uint32_t now_ms,
    const gp100_event_hooks_t *hooks)
{
    gp100_event_t event;
    gp100_expression_route_result_t route_result;
    uint32_t expression_value;
    size_t dispatch_count = 0u;

    if (router == NULL || !gp100_event_hooks_are_valid(hooks)) {
        return 0u;
    }

    while (hooks->pop_event(hooks->context, &event)) {
        if (dispatch_event(router, hooks, &event)) {
            ++dispatch_count;
        }
    }

    expression_value = hooks->read_expression_value(
        hooks->context, GP100_EXPRESSION_PEDAL_CHANNEL);
    route_result = gp100_event_router_sample_expression(
        router, (uint16_t)expression_value, now_ms, &event);

    if (route_result > GP100_EXPRESSION_ROUTE_UNCHANGED &&
        dispatch_event(router, hooks, &event)) {
        ++dispatch_count;
    }
    return dispatch_count;
}

bool gp100_dispatch_mode_tick(
    gp100_event_router_t *router,
    uint32_t event_tick,
    const gp100_event_hooks_t *hooks)
{
    gp100_tick_handler_t handler;

    if (router == NULL || !gp100_event_hooks_are_valid(hooks)) {
        return false;
    }
    if (event_tick == router->last_dispatched_tick) {
        return false;
    }

    router->last_dispatched_tick = event_tick;
    handler = hooks->resolve_tick_handler(hooks->context,
                                          router->current_mode);
    if (handler == NULL) {
        return false;
    }

    handler(hooks->context);
    return true;
}
