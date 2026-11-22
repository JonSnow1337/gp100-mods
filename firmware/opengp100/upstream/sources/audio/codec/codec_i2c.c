#include "codec_i2c.h"

static bool GP100_CodecI2cIsValid(const gp100_codec_i2c_t *device)
{
    return (device != NULL) && (device->base != NULL) && (device->address <= 0x7fu);
}

void GP100_CodecI2cBind(gp100_codec_i2c_t *device, LPI2C_Type *base, uint8_t address)
{
    if (device != NULL)
    {
        device->base = base;
        device->address = address;
    }
}

status_t GP100_CodecI2cRead(const gp100_codec_i2c_t *device, uint8_t reg, uint8_t *value)
{
    lpi2c_master_transfer_t transfer = {0};

    if (!GP100_CodecI2cIsValid(device) || (value == NULL))
    {
        return kStatus_InvalidArgument;
    }

    transfer.flags = kLPI2C_TransferDefaultFlag;
    transfer.slaveAddress = device->address;
    transfer.direction = kLPI2C_Read;
    transfer.subaddress = reg;
    transfer.subaddressSize = 1u;
    transfer.data = value;
    transfer.dataSize = 1u;
    return LPI2C_MasterTransferBlocking(device->base, &transfer);
}

status_t GP100_CodecI2cWrite(const gp100_codec_i2c_t *device, uint8_t reg, uint8_t value)
{
    lpi2c_master_transfer_t transfer = {0};

    if (!GP100_CodecI2cIsValid(device))
    {
        return kStatus_InvalidArgument;
    }

    transfer.flags = kLPI2C_TransferDefaultFlag;
    transfer.slaveAddress = device->address;
    transfer.direction = kLPI2C_Write;
    transfer.subaddress = reg;
    transfer.subaddressSize = 1u;
    transfer.data = &value;
    transfer.dataSize = 1u;
    return LPI2C_MasterTransferBlocking(device->base, &transfer);
}

status_t GP100_CodecI2cWriteSequence(const gp100_codec_i2c_t *device,
                                    const gp100_codec_register_t *sequence,
                                    size_t count)
{
    status_t status;

    if ((sequence == NULL) && (count != 0u))
    {
        return kStatus_InvalidArgument;
    }

    for (size_t i = 0u; i < count; ++i)
    {
        status = GP100_CodecI2cWrite(device, sequence[i].reg, sequence[i].value);
        if (status != kStatus_Success)
        {
            return status;
        }
    }
    return kStatus_Success;
}
