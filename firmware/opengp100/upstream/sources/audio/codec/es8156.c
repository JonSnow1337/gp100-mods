#include "es8156.h"

#define ES8156_REG_RESET       0x00u
#define ES8156_REG_MODE        0x02u
#define ES8156_REG_CLOCKS      0x08u
#define ES8156_REG_MISC2       0x09u
#define ES8156_REG_TIME1       0x0au
#define ES8156_REG_TIME2       0x0bu
#define ES8156_REG_P2S         0x0du
#define ES8156_REG_SERIAL_PORT 0x11u
#define ES8156_REG_MUTE        0x13u
#define ES8156_REG_VOLUME      0x14u
#define ES8156_REG_MISC3       0x18u
#define ES8156_REG_EQ1         0x19u
#define ES8156_REG_ANALOG1     0x20u
#define ES8156_REG_ANALOG2     0x21u
#define ES8156_REG_ANALOG3     0x22u
#define ES8156_REG_ANALOG4     0x23u
#define ES8156_REG_ANALOG_LP   0x24u
#define ES8156_REG_ANALOG5     0x25u
#define ES8156_REG_CHIP_ID_H   0xfdu
#define ES8156_REG_CHIP_ID_L   0xfeu

void ES8156_Bind(es8156_t *device, LPI2C_Type *base, uint8_t address)
{
    if (device != NULL)
    {
        GP100_CodecI2cBind(&device->i2c, base, address);
        device->volume = ES8156_VOLUME_0DB;
    }
}

status_t ES8156_ReadRegister(const es8156_t *device, uint8_t reg, uint8_t *value)
{
    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cRead(&device->i2c, reg, value);
}

status_t ES8156_WriteRegister(const es8156_t *device, uint8_t reg, uint8_t value)
{
    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cWrite(&device->i2c, reg, value);
}

status_t ES8156_Probe(const es8156_t *device)
{
    uint8_t high;
    uint8_t low;
    status_t status = ES8156_ReadRegister(device, ES8156_REG_CHIP_ID_H, &high);

    if (status == kStatus_Success)
    {
        status = ES8156_ReadRegister(device, ES8156_REG_CHIP_ID_L, &low);
    }
    if (status != kStatus_Success)
    {
        return status;
    }
    return ((high == ES8156_CHIP_ID_HIGH) && (low == ES8156_CHIP_ID_LOW)) ?
               kStatus_Success : kStatus_Fail;
}

status_t ES8156_Start(es8156_t *device,
                      es8156_word_length_t wordLength,
                      uint8_t volume,
                      bool unmute)
{
    const gp100_codec_register_t sequence[] = {
        {ES8156_REG_MODE, 0x04u},       /* Select software/I2C mode. */
        {ES8156_REG_ANALOG1, 0x2au},
        {ES8156_REG_ANALOG2, 0x3cu},
        {ES8156_REG_ANALOG3, 0x00u},
        {ES8156_REG_ANALOG_LP, 0x07u},
        {ES8156_REG_ANALOG4, 0x00u},
        {ES8156_REG_TIME1, 0x01u},
        {ES8156_REG_TIME2, 0x01u},
        {ES8156_REG_SERIAL_PORT, (uint8_t)wordLength},
        {ES8156_REG_EQ1, 0x20u},
        {ES8156_REG_P2S, 0x14u},
        {ES8156_REG_MISC2, 0x00u},
        {ES8156_REG_MISC3, 0x00u},
        {ES8156_REG_CLOCKS, 0x3fu},
        {ES8156_REG_RESET, 0x02u},
        {ES8156_REG_RESET, 0x03u},
        {ES8156_REG_ANALOG5, 0x20u},
        {ES8156_REG_VOLUME, volume},
    };
    status_t status;

    if ((device == NULL) ||
        ((wordLength != kES8156_I2s24Bit) && (wordLength != kES8156_I2s20Bit) &&
         (wordLength != kES8156_I2s18Bit) && (wordLength != kES8156_I2s16Bit) &&
         (wordLength != kES8156_I2s32Bit)))
    {
        return kStatus_InvalidArgument;
    }

    /* This is the public Everest/maintained-driver slave-mode sequence, not a
     * recovered GP-100 transaction trace. MCLK and SAI clocks must be valid. */
    status = GP100_CodecI2cWriteSequence(&device->i2c, sequence, ARRAY_SIZE(sequence));
    if (status == kStatus_Success)
    {
        device->volume = volume;
        /* Read/modify/write preserves the non-mute bits in register 0x13. */
        status = ES8156_SetMute(device, !unmute);
    }
    return status;
}

status_t ES8156_SetMute(const es8156_t *device, bool mute)
{
    uint8_t value;
    status_t status = ES8156_ReadRegister(device, ES8156_REG_MUTE, &value);

    if (status != kStatus_Success)
    {
        return status;
    }
    value = mute ? (uint8_t)(value | 0x06u) : (uint8_t)(value & ~0x06u);
    return ES8156_WriteRegister(device, ES8156_REG_MUTE, value);
}

status_t ES8156_SetVolume(es8156_t *device, uint8_t volume)
{
    status_t status;

    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    status = ES8156_WriteRegister(device, ES8156_REG_VOLUME, volume);
    if (status == kStatus_Success)
    {
        device->volume = volume;
    }
    return status;
}

status_t ES8156_Standby(const es8156_t *device)
{
    const gp100_codec_register_t sequence[] = {
        {ES8156_REG_VOLUME, 0x00u},
        {ES8156_REG_EQ1, 0x02u},
        {ES8156_REG_ANALOG2, 0x1fu},
        {ES8156_REG_ANALOG3, 0x02u},
        {ES8156_REG_ANALOG5, 0x21u},
        {ES8156_REG_ANALOG5, 0xa1u},
        {ES8156_REG_MISC3, 0x01u},
        {ES8156_REG_MISC2, 0x02u},
        {ES8156_REG_MISC2, 0x01u},
        {ES8156_REG_CLOCKS, 0x00u},
    };

    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    return GP100_CodecI2cWriteSequence(&device->i2c, sequence, ARRAY_SIZE(sequence));
}

status_t ES8156_Resume(const es8156_t *device)
{
    gp100_codec_register_t sequence[] = {
        {ES8156_REG_CLOCKS, 0x3fu},
        {ES8156_REG_MISC2, 0x00u},
        {ES8156_REG_MISC3, 0x00u},
        {ES8156_REG_ANALOG5, 0x20u},
        {ES8156_REG_ANALOG3, 0x00u},
        {ES8156_REG_ANALOG2, 0x3cu},
        {ES8156_REG_EQ1, 0x20u},
        {ES8156_REG_VOLUME, 0x00u},
    };

    if (device == NULL)
    {
        return kStatus_InvalidArgument;
    }
    sequence[ARRAY_SIZE(sequence) - 1u].value = device->volume;
    return GP100_CodecI2cWriteSequence(&device->i2c, sequence, ARRAY_SIZE(sequence));
}
