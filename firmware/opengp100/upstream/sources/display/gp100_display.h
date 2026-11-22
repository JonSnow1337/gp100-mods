#ifndef GP100_DISPLAY_H
#define GP100_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"

/* Fixed GP-100 board wiring recovered from V1/V2:
 *   LPSPI1 PCS0 = panel CS, GPIO3_15 = D/C, GPIO3_17 = RESET,
 *   GPIO1_3 = active-high backlight or panel enable.
 * The command set is ST7735-compatible; the exact controller suffix is not
 * visible in the available photographs. */
void GP100_DisplaySetEnabled(bool enabled);
void GP100_DisplayHardwareReset(void);
status_t GP100_DisplayWriteCommand(uint8_t command, const uint8_t *data, size_t dataSize);
status_t GP100_DisplayInitialize(void);
status_t GP100_DisplaySetWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
status_t GP100_DisplayWriteRgb565(const uint16_t *pixels, size_t pixelCount);
status_t GP100_DisplayFillRect(uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              uint16_t rgb565);
status_t GP100_DisplayFill(uint16_t rgb565);

#endif
