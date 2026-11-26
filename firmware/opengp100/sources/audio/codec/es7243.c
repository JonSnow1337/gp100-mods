#include "es7243.h"

#define ES7243_REG_MODE        0x00u
#define ES7243_REG_SERIAL_PORT 0x01u
#define ES7243_REG_MUTE        0x05u
#define ES7243_REG_RESET       0x06u
#define ES7243_REG_ANALOG      0x07u
#define ES7243_REG_GAIN        0x08u
#define ES7243_REG_POWER       0x09u
#define ES7243_REG_CHIP_ID     0x0eu

void ES7243_Bind(es7243_t *device, LPI2C_Type *base, uint8_t address)
{
    if (device != NULL)
    {
        GP100_CodecI2cBind(&device->i2c, base, address);
    }
}

status_t ES7243_ReadRegister(const es7243_t *device, uint8_t reg, uint8_t *value)
{
    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cRead(&device->i2c, reg, value);
}

status_t ES7243_WriteRegister(const es7243_t *device, uint8_t reg, uint8_t value)
{
    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cWrite(&device->i2c, reg, value);
}

status_t ES7243_Probe(const es7243_t *device)
{
    uint8_t chipId;
    status_t status = ES7243_ReadRegister(device, ES7243_REG_CHIP_ID, &chipId);

    if (status != kStatus_Success)
    {
        return status;
    }
    return (chipId == ES7243_CHIP_ID) ? kStatus_Success : kStatus_Fail;
}

status_t ES7243_StartSlave(const es7243_t *device,
                           es7243_word_length_t wordLength,
                           es7243_gain_t gain,
                           bool unmute)
{
    const gp100_codec_register_t sequence[] = {
        {ES7243_REG_MODE, 0x01u},        /* Slave mode, software control. */
        {ES7243_REG_RESET, 0x00u},       /* Release digital reset. */
        {ES7243_REG_MUTE, 0x1bu},        /* Mute while changing clocks. */
        {ES7243_REG_SERIAL_PORT, (uint8_t)wordLength},
        {ES7243_REG_GAIN, (uint8_t)gain},
        {ES7243_REG_MUTE, unmute ? 0x13u : 0x1bu},
    };

    if ((device == NULL) ||
        ((wordLength != kES7243_I2s24Bit) && (wordLength != kES7243_I2s16Bit)))
    {
        return kStatus_InvalidArgument;
    }

    /* The ES7243 application guide requires MCLK before this sequence. The
     * base project never invokes it automatically because the GP-100 address
     * straps and the precise SAI-to-codec net connection still need probing. */
    return GP100_CodecI2cWriteSequence(&device->i2c, sequence, ARRAY_SIZE(sequence));
}

status_t ES7243_SetMute(const es7243_t *device, bool mute)
{
    uint8_t value;
    status_t status = ES7243_ReadRegister(device, ES7243_REG_MUTE, &value);

    if (status != kStatus_Success)
    {
        return status;
    }
    value = mute ? (uint8_t)(value | 0x08u) : (uint8_t)(value & ~0x08u);
    return ES7243_WriteRegister(device, ES7243_REG_MUTE, value);
}

status_t ES7243_SetGain(const es7243_t *device, es7243_gain_t gain)
{
    return ES7243_WriteRegister(device, ES7243_REG_GAIN, (uint8_t)gain);
}

status_t ES7243_Standby(const es7243_t *device)
{
    const gp100_codec_register_t sequence[] = {
        {ES7243_REG_RESET, 0x05u},
        {ES7243_REG_MUTE, 0x1bu},
        {ES7243_REG_RESET, 0x5cu},
        {ES7243_REG_ANALOG, 0x3fu},
        {ES7243_REG_GAIN, 0x4bu},
        {ES7243_REG_POWER, 0x9fu},
    };

    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cWriteSequence(&device->i2c, sequence, ARRAY_SIZE(sequence));
}
