#include "app/app_runtime.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum test_event {
    EVENT_TMR2_SCHEDULER = 0x40000,
    EVENT_PERIOD_200,
    EVENT_PERIOD_500,
    EVENT_POLL,
    EVENT_MIDI_INPUT,
    EVENT_MIDI_TRANSPORT,
    EVENT_UI_DISPATCH,
    EVENT_DISPLAY_FLUSH,
    EVENT_SETTINGS_FLUSH,
    EVENT_TMR2_IRQ_DISABLE,
    EVENT_DSB,
    EVENT_ISB,
    EVENT_TMR2_IRQ_ENABLE,
    EVENT_TICK_MAINTENANCE
};

struct test_context {
    uint32_t events[64];
    size_t event_count;
    gp100_app_state_t *state;
    uint32_t tick_maintenance_seen;
};

static void record(struct test_context *context, uint32_t event)
{
    context->events[context->event_count++] = event;
}

static void run_initialization_stage(void *opaque,
                                     gp100_init_stage_t stage)
{
    record(opaque, (uint32_t)stage);
}

static uint32_t initialize_tuner_counter(void *opaque)
{
    record(opaque, EVENT_TMR2_SCHEDULER);
    return 37500000u;
}

static void configure_periodic_timer(void *opaque,
                                     gp100_periodic_timer_t timer,
                                     uint32_t value)
{
    struct test_context *context = opaque;
    if (timer == GP100_PERIODIC_TIMER_CONTROL_SCAN && value == 200u) {
        record(context, EVENT_PERIOD_200);
    } else if (timer == GP100_PERIODIC_TIMER_RHYTHM_SEQUENCER &&
               value == 500u) {
        record(context, EVENT_PERIOD_500);
    } else {
        record(context, UINT32_MAX);
    }
}

#define SIMPLE_HOOK(name, event)                    \
    static void name(void *opaque)                  \
    {                                                \
        record((struct test_context *)opaque, event); \
    }

SIMPLE_HOOK(poll_control_events, EVENT_POLL)
SIMPLE_HOOK(parse_midi_input, EVENT_MIDI_INPUT)
SIMPLE_HOOK(parse_midi_transport, EVENT_MIDI_TRANSPORT)
SIMPLE_HOOK(dispatch_ui_event, EVENT_UI_DISPATCH)
SIMPLE_HOOK(flush_display, EVENT_DISPLAY_FLUSH)
SIMPLE_HOOK(flush_settings, EVENT_SETTINGS_FLUSH)
SIMPLE_HOOK(data_synchronization_barrier, EVENT_DSB)
SIMPLE_HOOK(instruction_synchronization_barrier, EVENT_ISB)

static void set_tmr2_irq_enabled(void *opaque, bool enabled)
{
    record(opaque, enabled ? EVENT_TMR2_IRQ_ENABLE : EVENT_TMR2_IRQ_DISABLE);
}

static void tick_maintenance(void *opaque)
{
    struct test_context *context = opaque;
    context->tick_maintenance_seen = context->state->ticks_since_poll;
    record(context, EVENT_TICK_MAINTENANCE);
}

static gp100_app_hooks_t make_hooks(struct test_context *context)
{
    gp100_app_hooks_t hooks = {
        .context = context,
        .run_initialization_stage = run_initialization_stage,
        .initialize_tuner_counter = initialize_tuner_counter,
        .configure_periodic_timer = configure_periodic_timer,
        .poll_control_events = poll_control_events,
        .parse_midi_input = parse_midi_input,
        .parse_midi_transport = parse_midi_transport,
        .dispatch_ui_event = dispatch_ui_event,
        .flush_display = flush_display,
        .flush_settings = flush_settings,
        .set_tmr2_irq_enabled = set_tmr2_irq_enabled,
        .data_synchronization_barrier = data_synchronization_barrier,
        .instruction_synchronization_barrier = instruction_synchronization_barrier,
        .tick_maintenance = tick_maintenance
    };
    return hooks;
}

static int expect_events(const struct test_context *context,
                         const uint32_t *expected,
                         size_t count)
{
    size_t index;
    if (context->event_count != count) {
        return 1;
    }
    for (index = 0u; index < count; ++index) {
        if (context->events[index] != expected[index]) {
            return 2;
        }
    }
    return 0;
}

static int test_initialization_order(void)
{
    static const uint32_t expected[] = {
        0x00004030u, 0x00004268u, 0x00004220u, 0x00003f18u,
        0x00015178u, 0x00003ef0u, 0x00015a0cu, 0x00007960u,
        0x0001529cu, 0x0001552cu, 0x00016d88u, 0x0001907cu,
        EVENT_TMR2_SCHEDULER, EVENT_PERIOD_200, EVENT_PERIOD_500,
        0x00020d00u, 0x00020d5cu, 0x0001986cu, 0x000064b8u
    };
    gp100_app_state_t state = {0};
    struct test_context context = {.state = &state};
    gp100_app_hooks_t hooks = make_hooks(&context);

    if (!gp100_app_initialize(&state, &hooks)) {
        return 1;
    }
    if (!state.system_ready || state.tuner_counter_clock_hz != 37500000u) {
        return 2;
    }
    return expect_events(&context, expected,
                         sizeof(expected) / sizeof(expected[0]));
}

static int test_idle_service_pass(void)
{
    static const uint32_t expected[] = {
        EVENT_POLL, EVENT_MIDI_INPUT, EVENT_MIDI_TRANSPORT, EVENT_UI_DISPATCH
    };
    gp100_app_state_t state = {0};
    struct test_context context = {.state = &state};
    gp100_app_hooks_t hooks = make_hooks(&context);

    if (gp100_app_service_once(&state, &hooks) != GP100_STEP_IDLE) {
        return 1;
    }
    return expect_events(&context, expected,
                         sizeof(expected) / sizeof(expected[0]));
}

static int test_tick_service_pass(void)
{
    static const uint32_t expected[] = {
        EVENT_POLL, EVENT_MIDI_INPUT, EVENT_MIDI_TRANSPORT, EVENT_UI_DISPATCH,
        EVENT_DISPLAY_FLUSH, EVENT_SETTINGS_FLUSH, EVENT_TMR2_IRQ_DISABLE,
        EVENT_DSB, EVENT_ISB, EVENT_TMR2_IRQ_ENABLE
    };
    gp100_app_state_t state = {.foreground_ticks_pending = 3u};
    struct test_context context = {.state = &state};
    gp100_app_hooks_t hooks = make_hooks(&context);

    if (gp100_app_service_once(&state, &hooks) !=
        GP100_STEP_TICK_HANDLED) {
        return 1;
    }
    if (state.foreground_ticks_pending != 0u) {
        return 2;
    }
    return expect_events(&context, expected,
                         sizeof(expected) / sizeof(expected[0]));
}

static int test_systick_order(void)
{
    static const uint32_t expected[] = {EVENT_TICK_MAINTENANCE};
    gp100_app_state_t state = {0};
    struct test_context context = {.state = &state};
    gp100_app_hooks_t hooks = make_hooks(&context);

    gp100_app_on_systick(&state, &hooks);
    if (state.ticks_since_poll != 1u || state.event_tick != 1u ||
        state.foreground_ticks_pending != 1u) {
        return 1;
    }
    if (context.tick_maintenance_seen != 1u) {
        return 2;
    }
    return expect_events(&context, expected,
                         sizeof(expected) / sizeof(expected[0]));
}

int main(void)
{
    int result;

    result = test_initialization_order();
    if (result != 0) {
        fprintf(stderr, "initialization test failed: %d\n", result);
        return 10 + result;
    }
    result = test_idle_service_pass();
    if (result != 0) {
        fprintf(stderr, "idle service test failed: %d\n", result);
        return 20 + result;
    }
    result = test_tick_service_pass();
    if (result != 0) {
        fprintf(stderr, "tick service test failed: %d\n", result);
        return 30 + result;
    }
    result = test_systick_order();
    if (result != 0) {
        fprintf(stderr, "systick test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
