#include "gp100_display.h"

#include "board/peripherals.h"

#include "fsl_gpio.h"
#include "fsl_lpspi.h"
#include "system_MIMXRT1052.h"

#define GP100_DISPLAY_DC_GPIO     GPIO3
#define GP100_DISPLAY_DC_PIN      15u
#define GP100_DISPLAY_RESET_GPIO  GPIO3
#define GP100_DISPLAY_RESET_PIN   17u
#define GP100_DISPLAY_ENABLE_GPIO GPIO1
#define GP100_DISPLAY_ENABLE_PIN  3u

enum
{
    kDisplayCommandSleepOut  = 0x11u,
    kDisplayCommandDisplayOn = 0x29u,
    kDisplayCommandColumn    = 0x2au,
    kDisplayCommandRow       = 0x2bu,
    kDisplayCommandMemory    = 0x2cu,
    kDisplayCommandMadctl    = 0x36u,
    kDisplayCommandColmod    = 0x3au,
};

static void GP100_DisplayDelayMs(uint32_t milliseconds)
{
    while (milliseconds != 0u)
    {
        SDK_DelayAtLeastUs(1000u, SystemCoreClock);
        --milliseconds;
    }
}

static status_t GP100_DisplayWrite(const uint8_t *bytes, size_t count)
{
    lpspi_transfer_t transfer = {0};

    if ((bytes == NULL) || (count == 0u))
    {
        return kStatus_InvalidArgument;
    }

    transfer.txData = (uint8_t *)(uintptr_t)bytes;
    transfer.rxData = NULL;
    transfer.dataSize = count;
    transfer.configFlags = kLPSPI_MasterPcs0;
    return LPSPI_MasterTransferBlocking(LPSPI1, &transfer);
}

void GP100_DisplaySetEnabled(bool enabled)
{
    /* GPIO1_3 is definitely driven by the factory display path. Its physical
     * role (backlight versus whole-panel enable) is not yet proven, so this
     * deliberately named API is the only place that asserts it. */
    GPIO_PinWrite(GP100_DISPLAY_ENABLE_GPIO, GP100_DISPLAY_ENABLE_PIN, enabled ? 1u : 0u);
}

void GP100_DisplayHardwareReset(void)
{
    GPIO_PinWrite(GP100_DISPLAY_RESET_GPIO, GP100_DISPLAY_RESET_PIN, 1u);
    GP100_DisplayDelayMs(1u);
    GPIO_PinWrite(GP100_DISPLAY_RESET_GPIO, GP100_DISPLAY_RESET_PIN, 0u);
    GP100_DisplayDelayMs(1u);
    GPIO_PinWrite(GP100_DISPLAY_RESET_GPIO, GP100_DISPLAY_RESET_PIN, 1u);
    GP100_DisplayDelayMs(120u);
}

status_t GP100_DisplayWriteCommand(uint8_t command, const uint8_t *data, size_t dataSize)
{
    status_t status;

    if ((data == NULL) && (dataSize != 0u))
    {
        return kStatus_InvalidArgument;
    }

    GPIO_PinWrite(GP100_DISPLAY_DC_GPIO, GP100_DISPLAY_DC_PIN, 0u);
    status = GP100_DisplayWrite(&command, 1u);
    GPIO_PinWrite(GP100_DISPLAY_DC_GPIO, GP100_DISPLAY_DC_PIN, 1u);
    if ((status == kStatus_Success) && (dataSize != 0u))
    {
        status = GP100_DisplayWrite(data, dataSize);
    }
    return status;
}

static status_t GP100_DisplayConfigureController(void)
{
    static const uint8_t frameRate[] = {0x05u, 0x3au, 0x3au};
    static const uint8_t frameRatePartial[] = {0x05u, 0x3au, 0x3au, 0x05u, 0x3au, 0x3au};
    static const uint8_t power1[] = {0x62u, 0x02u, 0x04u};
    static const uint8_t power3[] = {0x0du, 0x00u};
    static const uint8_t power4[] = {0x8du, 0x6au};
    static const uint8_t power5[] = {0x8du, 0xeeu};
    static const uint8_t gammaPositive[] = {
        0x03u, 0x1bu, 0x12u, 0x11u, 0x3fu, 0x3au, 0x32u, 0x34u,
        0x2fu, 0x2bu, 0x30u, 0x3au, 0x00u, 0x01u, 0x02u, 0x05u,
    };
    static const uint8_t gammaNegative[] = {
        0x03u, 0x1bu, 0x12u, 0x11u, 0x32u, 0x2fu, 0x2au, 0x2fu,
        0x2eu, 0x2cu, 0x35u, 0x3fu, 0x00u, 0x00u, 0x01u, 0x05u,
    };
    static const uint8_t inversion[] = {0x03u};
    static const uint8_t power2[] = {0xc0u};
    static const uint8_t vcom[] = {0x12u};
    static const uint8_t madctl[] = {0xc8u};
    static const uint8_t colmod[] = {0x55u};
    status_t status;

#define DISPLAY_COMMAND(cmd, payload)                                                        \
    do                                                                                        \
    {                                                                                         \
        status = GP100_DisplayWriteCommand((cmd), (payload), sizeof(payload));                \
        if (status != kStatus_Success)                                                        \
        {                                                                                     \
            return status;                                                                    \
        }                                                                                     \
    } while (false)

    status = GP100_DisplayWriteCommand(kDisplayCommandSleepOut, NULL, 0u);
    if (status != kStatus_Success)
    {
        return status;
    }
    GP100_DisplayDelayMs(120u);

    DISPLAY_COMMAND(0xb1u, frameRate);
    DISPLAY_COMMAND(0xb2u, frameRate);
    DISPLAY_COMMAND(0xb3u, frameRatePartial);
    DISPLAY_COMMAND(0xb4u, inversion);
    DISPLAY_COMMAND(0xc0u, power1);
    DISPLAY_COMMAND(0xc1u, power2);
    DISPLAY_COMMAND(0xc2u, power3);
    DISPLAY_COMMAND(0xc3u, power4);
    DISPLAY_COMMAND(0xc4u, power5);
    DISPLAY_COMMAND(0xc5u, vcom);
    DISPLAY_COMMAND(0xe0u, gammaPositive);
    DISPLAY_COMMAND(0xe1u, gammaNegative);
    DISPLAY_COMMAND(kDisplayCommandMadctl, madctl);
    DISPLAY_COMMAND(kDisplayCommandColmod, colmod);

#undef DISPLAY_COMMAND
    return kStatus_Success;
}

status_t GP100_DisplayInitialize(void)
{
    status_t status;

    /* Explicitly calling this function opts into driving the uncertain
     * backlight/panel-enable signal. The reference-enabled application calls
     * it during visible boot; the minimal REFERENCE=OFF base does not. */
    GP100_DisplaySetEnabled(false);
    GP100_DisplayHardwareReset();
    status = GP100_DisplayConfigureController();
    if (status == kStatus_Success)
    {
        status = GP100_DisplaySetWindow(0u, 0u, GP100_PANEL_WIDTH, GP100_PANEL_HEIGHT);
    }
    if (status == kStatus_Success)
    {
        status = GP100_DisplayWriteCommand(kDisplayCommandDisplayOn, NULL, 0u);
    }
    if (status == kStatus_Success)
    {
        /* The V2 trace waits one second after DISPON. */
        GP100_DisplayDelayMs(1000u);
        GP100_DisplaySetEnabled(true);
    }
    else
    {
        GP100_DisplaySetEnabled(false);
    }
    return status;
}

status_t GP100_DisplaySetWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t xEnd;
    uint16_t yEnd;
    uint8_t range[4];
    status_t status;

    if ((width == 0u) || (height == 0u) || (x >= GP100_PANEL_WIDTH) ||
        (y >= GP100_PANEL_HEIGHT) || (width > (GP100_PANEL_WIDTH - x)) ||
        (height > (GP100_PANEL_HEIGHT - y)))
    {
        return kStatus_InvalidArgument;
    }

    xEnd = (uint16_t)(x + width - 1u);
    yEnd = (uint16_t)(y + height - 1u);
    range[0] = (uint8_t)(x >> 8u);
    range[1] = (uint8_t)x;
    range[2] = (uint8_t)(xEnd >> 8u);
    range[3] = (uint8_t)xEnd;
    status = GP100_DisplayWriteCommand(kDisplayCommandColumn, range, sizeof(range));
    if (status != kStatus_Success)
    {
        return status;
    }

    range[0] = (uint8_t)(y >> 8u);
    range[1] = (uint8_t)y;
    range[2] = (uint8_t)(yEnd >> 8u);
    range[3] = (uint8_t)yEnd;
    status = GP100_DisplayWriteCommand(kDisplayCommandRow, range, sizeof(range));
    if (status != kStatus_Success)
    {
        return status;
    }
    return GP100_DisplayWriteCommand(kDisplayCommandMemory, NULL, 0u);
}

status_t GP100_DisplayWriteRgb565(const uint16_t *pixels, size_t pixelCount)
{
    uint8_t bytes[128];
    status_t status;

    if ((pixels == NULL) && (pixelCount != 0u))
    {
        return kStatus_InvalidArgument;
    }

    while (pixelCount != 0u)
    {
        size_t count = (pixelCount > ARRAY_SIZE(bytes) / 2u) ? ARRAY_SIZE(bytes) / 2u : pixelCount;
        for (size_t i = 0u; i < count; ++i)
        {
            bytes[2u * i] = (uint8_t)(pixels[i] >> 8u);
            bytes[2u * i + 1u] = (uint8_t)pixels[i];
        }
        status = GP100_DisplayWrite(bytes, count * 2u);
        if (status != kStatus_Success)
        {
            return status;
        }
        pixels += count;
        pixelCount -= count;
    }
    return kStatus_Success;
}

status_t GP100_DisplayFillRect(uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              uint16_t rgb565)
{
    uint16_t pixels[64];
    size_t remaining = (size_t)width * (size_t)height;
    status_t status = GP100_DisplaySetWindow(x, y, width, height);

    if (status != kStatus_Success)
    {
        return status;
    }
    for (size_t i = 0u; i < ARRAY_SIZE(pixels); ++i)
    {
        pixels[i] = rgb565;
    }
    while (remaining != 0u)
    {
        size_t count = (remaining > ARRAY_SIZE(pixels)) ? ARRAY_SIZE(pixels) : remaining;
        status = GP100_DisplayWriteRgb565(pixels, count);
        if (status != kStatus_Success)
        {
            return status;
        }
        remaining -= count;
    }
    return kStatus_Success;
}

status_t GP100_DisplayFill(uint16_t rgb565)
{
    return GP100_DisplayFillRect(0u, 0u, GP100_PANEL_WIDTH, GP100_PANEL_HEIGHT, rgb565);
}
