#ifndef GP100_CODEC_I2C_H
#define GP100_CODEC_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"
#include "fsl_lpi2c.h"

typedef struct
{
    LPI2C_Type *base;
    uint8_t address;
} gp100_codec_i2c_t;

typedef struct
{
    uint8_t reg;
    uint8_t value;
} gp100_codec_register_t;

void GP100_CodecI2cBind(gp100_codec_i2c_t *device, LPI2C_Type *base, uint8_t address);
status_t GP100_CodecI2cRead(const gp100_codec_i2c_t *device, uint8_t reg, uint8_t *value);
status_t GP100_CodecI2cWrite(const gp100_codec_i2c_t *device, uint8_t reg, uint8_t value);
status_t GP100_CodecI2cWriteSequence(const gp100_codec_i2c_t *device,
                                    const gp100_codec_register_t *sequence,
                                    size_t count);

#endif
