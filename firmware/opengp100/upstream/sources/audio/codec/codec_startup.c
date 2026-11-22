#include "codec_startup.h"

#include "codec_i2c.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"

#define GP100_CODEC_SCL_PIN 1u
#define GP100_CODEC_SDA_PIN 2u
#define GP100_CODEC_BUS_MASK ((1u << GP100_CODEC_SCL_PIN) | (1u << GP100_CODEC_SDA_PIN))
#define GP100_CODEC_PAD_CONTROL 0x58b0u
#define GP100_CODEC_BUS_DELAY_US 20u

/* V2.1.0 audio startup at 0x2aa8 uses the following tables for selector 1.
 * It tries the same primary sequence at 0x10 and 0x14, then the shorter
 * sequence at 0x13. The addresses below omit the write bit. */
static const gp100_codec_register_t s_primaryCodecRegisters[] = {
    {0x01u, 0x3au}, {0x00u, 0x80u}, {0x04u, 0x02u}, {0x04u, 0x00u},
    {0xf9u, 0x01u}, {0x00u, 0x1eu}, {0x01u, 0x00u}, {0x02u, 0x00u},
    {0x03u, 0x20u}, {0x0du, 0x30u}, {0xf9u, 0x00u}, {0x04u, 0x02u},
    {0x04u, 0x00u}, {0x05u, 0x00u}, {0x06u, 0x07u}, {0x07u, 0x01u},
    {0x08u, 0xffu}, {0x09u, 0xc5u}, {0x0au, 0x81u}, {0x0bu, 0x00u},
    {0x0eu, 0xbfu}, {0x0fu, 0xa0u}, {0x10u, 0x18u}, {0x11u, 0x16u},
    {0x14u, 0x0cu}, {0x15u, 0x0du}, {0x17u, 0x02u}, {0x18u, 0x26u},
    {0x19u, 0x77u}, {0x1au, 0xf4u}, {0x1bu, 0x66u}, {0x1cu, 0x44u},
    {0x1eu, 0x00u}, {0x1fu, 0x0cu}, {0x20u, 0x10u}, {0x21u, 0x10u},
    {0x00u, 0xc0u}, {0x01u, 0x3au}, {0x16u, 0x3fu}, {0x16u, 0x00u},
};

static const gp100_codec_register_t s_fallbackCodecRegisters[] = {
    {0x00u, 0x23u}, {0x05u, 0x1bu}, {0x06u, 0x10u}, {0x01u, 0x00u},
    {0x02u, 0x10u}, {0x03u, 0x04u}, {0x04u, 0x02u}, {0x0du, 0xa0u},
    {0x08u, 0x11u}, {0x06u, 0x00u}, {0x05u, 0x13u},
};

static volatile uint8_t s_codecAddress;
static volatile uint8_t s_codecRegistersWritten;

static void GP100_CodecDelay(void)
{
    SDK_DelayAtLeastUs(GP100_CODEC_BUS_DELAY_US, SystemCoreClock);
}

static bool GP100_CodecReleaseClock(void)
{
    GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 1u);
    for (uint32_t waits = 0u; waits < 5u; ++waits)
    {
        GP100_CodecDelay();
        if ((GPIO1->PSR & (1u << GP100_CODEC_SCL_PIN)) != 0u)
        {
            return true;
        }
    }
    return false;
}

static bool GP100_CodecSendByte(uint8_t data)
{
    for (uint32_t bit = 0x80u; bit != 0u; bit >>= 1u)
    {
        GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, (data & bit) != 0u);
        GP100_CodecDelay();
        if (!GP100_CodecReleaseClock())
        {
            return false;
        }
        GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 0u);
        GP100_CodecDelay();
    }
    GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, 1u);
    GP100_CodecDelay();
    const bool acknowledged = GP100_CodecReleaseClock() &&
                              (GPIO1->PSR & (1u << GP100_CODEC_SDA_PIN)) == 0u;
    GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 0u);
    GP100_CodecDelay();
    return acknowledged;
}

static bool GP100_CodecStop(void)
{
    GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 0u);
    GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, 0u);
    GP100_CodecDelay();
    const bool clockHigh = GP100_CodecReleaseClock();
    GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, 1u);
    GP100_CodecDelay();
    return clockHigh;
}

static bool GP100_CodecRecoverBus(void)
{
    GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, 1u);
    if (!GP100_CodecReleaseClock())
    {
        return false;
    }
    if ((GPIO1->PSR & (1u << GP100_CODEC_SDA_PIN)) != 0u)
    {
        return true;
    }

    /* The powered codec can hold SDA after an MCU reset during an ACK.
     * Clock the unfinished byte out before issuing a STOP (UM10204). */
    for (uint32_t pulse = 0u; pulse < 9u; ++pulse)
    {
        GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 0u);
        GP100_CodecDelay();
        if (!GP100_CodecReleaseClock())
        {
            return false;
        }
        if ((GPIO1->PSR & (1u << GP100_CODEC_SDA_PIN)) != 0u)
        {
            break;
        }
    }
    return GP100_CodecStop() &&
        (GPIO1->PSR & GP100_CODEC_BUS_MASK) == GP100_CODEC_BUS_MASK;
}

static bool GP100_CodecWrite(uint8_t address, gp100_codec_register_t item)
{
    GPIO1->DR |= GP100_CODEC_BUS_MASK;
    GP100_CodecDelay();
    if ((GPIO1->PSR & GP100_CODEC_BUS_MASK) != GP100_CODEC_BUS_MASK)
    {
        return false;
    }
    GPIO_PinWrite(GPIO1, GP100_CODEC_SDA_PIN, 0u);
    GP100_CodecDelay();
    GPIO_PinWrite(GPIO1, GP100_CODEC_SCL_PIN, 0u);
    GP100_CodecDelay();

    const bool written = GP100_CodecSendByte((uint8_t)(address << 1u)) &&
                         GP100_CodecSendByte(item.reg) &&
                         GP100_CodecSendByte(item.value);
    return GP100_CodecStop() && written;
}

static bool GP100_CodecTrySequence(uint8_t address,
                                  const gp100_codec_register_t *sequence,
                                  size_t count)
{
    s_codecAddress = address;
    s_codecRegistersWritten = 0u;
    for (size_t index = 0u; index < count; ++index)
    {
        if (!GP100_CodecWrite(address, sequence[index]))
        {
            return false;
        }
        ++s_codecRegistersWritten;
    }
    return true;
}

bool GP100_CodecStartup(void)
{
    /* GPIO1_IO01 is CCLK and GPIO1_IO02 is CDATA. Force the GPIO input
     * path on so the open-drain pins can be sampled while released. */
    GPIO1->GDIR &= ~GP100_CODEC_BUS_MASK;
    IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_01_GPIO1_IO01, GP100_CODEC_PAD_CONTROL);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B0_02_GPIO1_IO02, GP100_CODEC_PAD_CONTROL);
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_01_GPIO1_IO01, 1u);
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_02_GPIO1_IO02, 1u);
    GPIO1->DR |= GP100_CODEC_BUS_MASK;
    GPIO1->GDIR |= GP100_CODEC_BUS_MASK;
    SDK_DelayAtLeastUs(10000u, SystemCoreClock);

    for (uint32_t attempt = 0u; attempt < 3u; ++attempt)
    {
        if (!GP100_CodecRecoverBus())
        {
            SDK_DelayAtLeastUs(3000u, SystemCoreClock);
            continue;
        }
        if (GP100_CodecTrySequence(0x10u, s_primaryCodecRegisters,
                                   ARRAY_SIZE(s_primaryCodecRegisters)))
        {
            return true;
        }
        SDK_DelayAtLeastUs(5000u, SystemCoreClock);
        if (GP100_CodecTrySequence(0x14u, s_primaryCodecRegisters,
                                   ARRAY_SIZE(s_primaryCodecRegisters)) ||
            GP100_CodecTrySequence(0x13u, s_fallbackCodecRegisters,
                                   ARRAY_SIZE(s_fallbackCodecRegisters)))
        {
            return true;
        }
        SDK_DelayAtLeastUs(3000u, SystemCoreClock);
    }
    return false;
}
