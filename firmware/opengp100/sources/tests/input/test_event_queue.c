#include "input/event_queue.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static gp100_event_t make_event(uint32_t value)
{
    gp100_event_t event = {
        .value = value,
        .code = UINT32_C(0x22000000) | value
    };
    return event;
}

static int test_fifo_and_wrap(void)
{
    gp100_event_queue_t queue;
    gp100_event_t event;
    uint32_t index;

    memset(&queue, 0, sizeof(queue));
    queue.write_index = 498u;
    queue.read_index = 498u;

    if (!gp100_event_queue_push_safe(&queue, make_event(1u)) ||
        !gp100_event_queue_push_safe(&queue, make_event(2u))) {
        return 1;
    }
    if (queue.write_index != 0u || gp100_event_queue_count(&queue) != 2u) {
        return 2;
    }
    for (index = 1u; index <= 2u; ++index) {
        if (!gp100_event_queue_pop(&queue, &event) ||
            event.value != index) {
            return 3;
        }
    }
    if (gp100_event_queue_pop(&queue, &event) ||
        event.value != 0u || event.code != 0u) {
        return 4;
    }
    return 0;
}

static int test_safe_capacity(void)
{
    gp100_event_queue_t queue;
    uint32_t index;

    memset(&queue, 0, sizeof(queue));
    for (index = 0u; index < GP100_EVENT_QUEUE_CAPACITY - 1u; ++index) {
        if (!gp100_event_queue_push_safe(&queue, make_event(index))) {
            return 1;
        }
    }
    if (gp100_event_queue_count(&queue) != 499u) {
        return 2;
    }
    if (gp100_event_queue_push_safe(&queue, make_event(500u))) {
        return 3;
    }
    return 0;
}

static int test_observed_full_ambiguity(void)
{
    gp100_event_queue_t queue;
    gp100_event_t event;
    uint32_t index;

    memset(&queue, 0, sizeof(queue));
    for (index = 0u; index < GP100_EVENT_QUEUE_CAPACITY; ++index) {
        if (!gp100_event_queue_push_observed(&queue, make_event(index))) {
            return 1;
        }
    }

    /* The observed implementation now has the same index state as empty. */
    if (queue.write_index != queue.read_index ||
        gp100_event_queue_count(&queue) != 0u) {
        return 2;
    }
    if (gp100_event_queue_pop(&queue, &event)) {
        return 3;
    }
    return 0;
}

int main(void)
{
    int result;

    result = test_fifo_and_wrap();
    if (result != 0) {
        fprintf(stderr, "FIFO/wrap test failed: %d\n", result);
        return 10 + result;
    }
    result = test_safe_capacity();
    if (result != 0) {
        fprintf(stderr, "safe capacity test failed: %d\n", result);
        return 20 + result;
    }
    result = test_observed_full_ambiguity();
    if (result != 0) {
        fprintf(stderr, "observed full-state test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
