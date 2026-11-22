#ifndef GP100_EVENT_QUEUE_H
#define GP100_EVENT_QUEUE_H

#include "app/event_dispatch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_EVENT_QUEUE_CAPACITY 500u

typedef struct gp100_event_queue {
    uint16_t write_index;
    uint16_t read_index;
    gp100_event_t entries[GP100_EVENT_QUEUE_CAPACITY];
} gp100_event_queue_t;

void gp100_event_queue_reset(gp100_event_queue_t *queue);

size_t gp100_event_queue_count(const gp100_event_queue_t *queue);

/*
 * Reproduce the observed enqueue check at ITCM 0x26270. This accepts the 500th
 * entry and therefore permits write_index == read_index at full capacity.
 */
bool gp100_event_queue_push_observed(gp100_event_queue_t *queue,
                                          gp100_event_t event);

/*
 * Recommended for new firmware: reserve one slot so empty and full cannot
 * share the same index state. Maximum stored events are therefore 499.
 */
bool gp100_event_queue_push_safe(gp100_event_queue_t *queue,
                                      gp100_event_t event);

/* Reproduce the observed pop behavior at ITCM 0x19e20. */
bool gp100_event_queue_pop(gp100_event_queue_t *queue,
                                gp100_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
