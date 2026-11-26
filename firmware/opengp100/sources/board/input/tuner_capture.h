#ifndef GP100_TUNER_CAPTURE_H
#define GP100_TUNER_CAPTURE_H

#include "tuner/tuner_model.h"

#include <stdbool.h>
#include <stdint.h>

#define GP100_TUNER_CAPTURE_HISTORY_LENGTH 128u

/* Configure the recovered GPIO1_IO21 -> cascaded QTMR3 capture path. The
 * counter and interrupts remain stopped until GP100_TunerCaptureStart(). */
void GP100_TunerCaptureInit(void);

bool GP100_TunerCaptureStart(void);
void GP100_TunerCaptureStop(void);
bool GP100_TunerCaptureIsRunning(void);

/* Copy the newest twelve normalized intervals in newest-first order, suitable
 * for gp100_tuner_reduce_factory_intervals(). Returns false until twelve
 * falling-edge intervals have been captured. A supplied sequence receives the
 * edge count even before the first complete window. */
bool GP100_TunerCaptureReadIntervals(
    uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    uint32_t *sequence);

/* Actual QTMR3 channel-0 count frequency derived from the current IPG clock. */
uint32_t GP100_TunerCaptureClockHz(void);

#endif
