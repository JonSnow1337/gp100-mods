#include "app/event_dispatch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct test_context {
    gp100_event_t queued[4];
    size_t queued_count;
    size_t queued_index;
    gp100_event_t handled[8];
    uint16_t handled_modes[8];
    size_t handled_count;
    uint32_t control_value;
    uint32_t requested_channel;
    uint32_t tick_calls;
    bool expose_event_handler;
    bool expose_tick_handler;
    gp100_event_router_t *router;
};

static bool pop_event(void *opaque, gp100_event_t *event)
{
    struct test_context *context = opaque;
    if (context->queued_index == context->queued_count) {
        return false;
    }
    *event = context->queued[context->queued_index++];
    return true;
}

static uint32_t read_expression_value(void *opaque, uint32_t channel)
{
    struct test_context *context = opaque;
    context->requested_channel = channel;
    return context->control_value;
}

static void handle_event(void *opaque,
                         const gp100_event_t *event,
                         uint32_t source)
{
    struct test_context *context = opaque;
    if (source != 0u) {
        context->handled_count = 99u;
        return;
    }
    context->handled[context->handled_count] = *event;
    context->handled_modes[context->handled_count] =
        context->router->current_mode;
    ++context->handled_count;
}

static void handle_tick(void *opaque)
{
    struct test_context *context = opaque;
    ++context->tick_calls;
}

static gp100_event_handler_t resolve_event_handler(void *opaque,
                                                        uint16_t mode)
{
    struct test_context *context = opaque;
    (void)mode;
    return context->expose_event_handler ? handle_event : NULL;
}

static gp100_tick_handler_t resolve_tick_handler(void *opaque,
                                                      uint16_t mode)
{
    struct test_context *context = opaque;
    (void)mode;
    return context->expose_tick_handler ? handle_tick : NULL;
}

static gp100_event_hooks_t make_hooks(struct test_context *context)
{
    gp100_event_hooks_t hooks = {
        .context = context,
        .pop_event = pop_event,
        .read_expression_value = read_expression_value,
        .resolve_event_handler = resolve_event_handler,
        .resolve_tick_handler = resolve_tick_handler
    };
    return hooks;
}

static int test_queue_and_low_transition(void)
{
    gp100_event_router_t router = {
        .current_mode = 9u,
        .expression_toe = 90u,
        .expression_activation_threshold = 100u,
        .previous_expression_value = 90u,
        .expression_activation_latched = true
    };
    struct test_context context = {
        .queued = {{7u, 0x22000001u}, {8u, 0x22000002u}},
        .queued_count = 2u,
        .control_value = 42u,
        .expose_event_handler = true,
        .expose_tick_handler = true,
        .router = &router
    };
    gp100_event_hooks_t hooks = make_hooks(&context);

    if (gp100_dispatch_pending_events(&router, 0u, &hooks) != 3u) {
        return 1;
    }
    if (context.requested_channel != GP100_EXPRESSION_PEDAL_CHANNEL ||
        context.handled_count != 3u) {
        return 2;
    }
    if (context.handled[0].value != 7u ||
        context.handled[1].code != 0x22000002u) {
        return 3;
    }
    if (context.handled[2].value != 0u ||
        context.handled[2].code != 0x1100002au) {
        return 4;
    }
    if (router.expression_activation_latched ||
        router.expression_position_event_count != 1u ||
        router.previous_expression_value != 42u) {
        return 5;
    }
    return 0;
}

static int test_sustained_expression_press(void)
{
    const uint32_t start = UINT32_MAX - 25u;
    gp100_event_router_t router = {
        .current_mode = 4u,
        .expression_toe = 95u,
        .expression_activation_threshold = 100u,
        .previous_expression_value = 20u
    };
    struct test_context context = {
        .control_value = 120u,
        .expose_event_handler = true,
        .expose_tick_handler = true,
        .router = &router
    };
    gp100_event_hooks_t hooks = make_hooks(&context);

    if (gp100_dispatch_pending_events(&router, start, &hooks) != 1u ||
        context.handled[0].value != 0u ||
        context.handled[0].code != 0x11000078u) {
        return 1;
    }
    if (gp100_dispatch_pending_events(&router, start + 49u, &hooks) != 0u ||
        gp100_dispatch_pending_events(&router, start + 50u, &hooks) != 1u ||
        context.handled[1].value != 1u ||
        context.handled[1].code != 0x61000078u ||
        router.expression_activation_event_count != 1u) {
        return 2;
    }

    context.control_value = 98u;
    if (gp100_dispatch_pending_events(&router, start + 51u, &hooks) != 1u ||
        !router.expression_activation_latched) {
        return 3;
    }
    context.control_value = 120u;
    if (gp100_dispatch_pending_events(&router, start + 52u, &hooks) != 1u ||
        gp100_dispatch_pending_events(&router, start + 102u, &hooks) != 0u ||
        router.expression_activation_event_count != 1u) {
        return 4;
    }
    context.control_value = 95u;
    if (gp100_dispatch_pending_events(&router, start + 103u, &hooks) != 1u ||
        router.expression_activation_latched) {
        return 5;
    }
    context.control_value = 120u;
    if (gp100_dispatch_pending_events(&router, start + 104u, &hooks) != 1u ||
        gp100_dispatch_pending_events(&router, start + 154u, &hooks) != 1u ||
        router.expression_activation_event_count != 2u) {
        return 6;
    }
    return 0;
}

static int test_mode_tick_gate(void)
{
    gp100_event_router_t router = {
        .current_mode = 3u,
        .last_dispatched_tick = 10u
    };
    struct test_context context = {
        .expose_event_handler = true,
        .expose_tick_handler = true,
        .router = &router
    };
    gp100_event_hooks_t hooks = make_hooks(&context);

    if (gp100_dispatch_mode_tick(&router, 10u, &hooks)) {
        return 1;
    }
    if (!gp100_dispatch_mode_tick(&router, 11u, &hooks)) {
        return 2;
    }
    if (context.tick_calls != 1u || router.last_dispatched_tick != 11u) {
        return 3;
    }

    context.expose_tick_handler = false;
    if (gp100_dispatch_mode_tick(&router, 12u, &hooks)) {
        return 4;
    }
    if (router.last_dispatched_tick != 12u) {
        return 5;
    }
    return 0;
}

static int test_fast_expression_sweep(void)
{
    gp100_event_router_t router = {
        .expression_toe = 79u,
        .expression_activation_threshold = 85u,
        .previous_expression_value = 37u,
    };
    gp100_event_t event = {99u, 99u};

    if (gp100_event_router_sample_expression(
            &router, 85u, 10u, &event) !=
            GP100_EXPRESSION_ROUTE_POSITION ||
        event.value != 0u || event.code != UINT32_C(0x11000055)) {
        return 1;
    }
    if (gp100_event_router_sample_expression(
            &router, 85u, 45u, &event) != GP100_EXPRESSION_ROUTE_UNCHANGED ||
        gp100_event_router_sample_expression(
            &router, 83u, 46u, &event) != GP100_EXPRESSION_ROUTE_POSITION ||
        event.code != UINT32_C(0x11000053)) {
        return 2;
    }
    if (gp100_event_router_sample_expression(
            &router, 85u, 47u, &event) != GP100_EXPRESSION_ROUTE_POSITION ||
        gp100_event_router_sample_expression(
            &router, 85u, 70u, &event) != GP100_EXPRESSION_ROUTE_UNCHANGED) {
        return 3;
    }
    if (gp100_event_router_sample_expression(
            &router, 37u, 71u, &event) != GP100_EXPRESSION_ROUTE_POSITION ||
        event.code != UINT32_C(0x11000025) ||
        router.expression_activation_event_count != 0u) {
        return 4;
    }
    router.expression_activation_threshold = 0u;
    if (gp100_event_router_sample_expression(
            &router, 1u, 72u, &event) !=
            GP100_EXPRESSION_ROUTE_INVALID) {
        return 5;
    }
    return 0;
}

static int test_expression_held_at_startup(void)
{
    gp100_event_router_t router = {
        .expression_toe = 79u,
        .expression_activation_threshold = 85u,
        .expression_activation_latched = true,
    };
    gp100_event_t event;

    if (gp100_event_router_sample_expression(&router, 85u, 1u, &event) !=
            GP100_EXPRESSION_ROUTE_POSITION ||
        gp100_event_router_sample_expression(&router, 85u, 1000u, &event) !=
            GP100_EXPRESSION_ROUTE_UNCHANGED ||
        router.expression_activation_event_count != 0u) {
        return 1;
    }
    if (gp100_event_router_sample_expression(&router, 37u, 1010u, &event) !=
            GP100_EXPRESSION_ROUTE_POSITION ||
        router.expression_activation_latched ||
        gp100_event_router_sample_expression(&router, 85u, 1020u, &event) !=
            GP100_EXPRESSION_ROUTE_POSITION ||
        gp100_event_router_sample_expression(&router, 85u, 1070u, &event) !=
            GP100_EXPRESSION_ROUTE_ACTIVATION) {
        return 2;
    }
    return 0;
}

int main(void)
{
    int result;

    result = test_queue_and_low_transition();
    if (result != 0) {
        fprintf(stderr, "queue/low transition test failed: %d\n", result);
        return 10 + result;
    }
    result = test_sustained_expression_press();
    if (result != 0) {
        fprintf(stderr, "high transition test failed: %d\n", result);
        return 20 + result;
    }
    result = test_mode_tick_gate();
    if (result != 0) {
        fprintf(stderr, "mode tick test failed: %d\n", result);
        return 30 + result;
    }
    result = test_fast_expression_sweep();
    if (result != 0) {
        fprintf(stderr, "single sample test failed: %d\n", result);
        return 40 + result;
    }
    result = test_expression_held_at_startup();
    if (result != 0) {
        fprintf(stderr, "startup release test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
