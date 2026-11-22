#ifndef GP100_DISPLAY_TRANSPORT_H
#define GP100_DISPLAY_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

#include "ui/render/display_framebuffer.h"

/* Prepare the recovered LPSPI1/eDMA route without starting a transfer. */
void GP100_DisplayTransportInit(void);

/* Send RGB565 words with the factory channel-2/channel-18 DMA pairing.
 * The call blocks until channel 18 drains the final received frame. It must
 * run with interrupts enabled and pixelCount must not exceed 0x7fff. */
status_t GP100_DisplayTransferRgb565Blocking(const uint16_t *pixels,
                                             size_t pixelCount);
bool GP100_DisplayTransportIsBusy(void);
uint32_t GP100_DisplayTransportCompletedFrames(void);

/* Bind the readable V2 framebuffer model to the concrete board transport. */
void GP100_DisplayTransportGetHooks(gp100_display_hooks_t *hooks);
/* Return the base project's zero-initialized, SDRAM-backed buffer pair. */
void GP100_DisplayTransportGetFramebuffer(
    gp100_display_framebuffer_t *framebuffer);

#endif
