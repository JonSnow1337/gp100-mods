#include "event_queue.h"

#include <stddef.h>

static uint16_t advance_index(uint16_t index)
{
    ++index;
    if (index >= GP100_EVENT_QUEUE_CAPACITY) {
        index = 0u;
    }
    return index;
}

void gp100_event_queue_reset(gp100_event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    queue->write_index = 0u;
    queue->read_index = 0u;
}

size_t gp100_event_queue_count(const gp100_event_queue_t *queue)
{
    if (queue == NULL) {
        return 0u;
    }
    if (queue->write_index >= queue->read_index) {
        return (size_t)(queue->write_index - queue->read_index);
    }
    return GP100_EVENT_QUEUE_CAPACITY -
           (size_t)(queue->read_index - queue->write_index);
}

bool gp100_event_queue_push_observed(gp100_event_queue_t *queue,
                                          gp100_event_t event)
{
    size_t used;

    if (queue == NULL) {
        return false;
    }

    used = gp100_event_queue_count(queue);
    if (GP100_EVENT_QUEUE_CAPACITY - used < 1u) {
        return false;
    }

    queue->entries[queue->write_index] = event;
    queue->write_index = advance_index(queue->write_index);
    return true;
}

bool gp100_event_queue_push_safe(gp100_event_queue_t *queue,
                                      gp100_event_t event)
{
    uint16_t next;

    if (queue == NULL) {
        return false;
    }
    next = advance_index(queue->write_index);
    if (next == queue->read_index) {
        return false;
    }

    queue->entries[queue->write_index] = event;
    queue->write_index = next;
    return true;
}

bool gp100_event_queue_pop(gp100_event_queue_t *queue,
                                gp100_event_t *event)
{
    if (queue == NULL || event == NULL) {
        return false;
    }
    if (queue->write_index == queue->read_index) {
        event->value = 0u;
        event->code = 0u;
        return false;
    }

    *event = queue->entries[queue->read_index];
    queue->read_index = advance_index(queue->read_index);
    return true;
}
