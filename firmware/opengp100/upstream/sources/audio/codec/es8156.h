#ifndef GP100_ES8156_H
#define GP100_ES8156_H

#include <stdbool.h>
#include <stdint.h>

#include "codec_i2c.h"

/* ES8156 address is binary 000100[CE]. MCUXpresso expects the 7-bit form. */
#define ES8156_I2C_ADDRESS_CE_LOW  0x08u
#define ES8156_I2C_ADDRESS_CE_HIGH 0x09u
#define ES8156_CHIP_ID_HIGH         0x81u
#define ES8156_CHIP_ID_LOW          0x55u
#define ES8156_VOLUME_0DB           0xbfu

typedef enum
{
    kES8156_I2s24Bit = 0x00u,
    kES8156_I2s20Bit = 0x04u,
    kES8156_I2s18Bit = 0x08u,
    kES8156_I2s16Bit = 0x0cu,
    kES8156_I2s32Bit = 0x10u,
} es8156_word_length_t;

typedef struct
{
    gp100_codec_i2c_t i2c;
    uint8_t volume;
} es8156_t;

void ES8156_Bind(es8156_t *device, LPI2C_Type *base, uint8_t address);
status_t ES8156_ReadRegister(const es8156_t *device, uint8_t reg, uint8_t *value);
status_t ES8156_WriteRegister(const es8156_t *device, uint8_t reg, uint8_t value);
status_t ES8156_Probe(const es8156_t *device);
status_t ES8156_Start(es8156_t *device,
                      es8156_word_length_t wordLength,
                      uint8_t volume,
                      bool unmute);
status_t ES8156_SetMute(const es8156_t *device, bool mute);
status_t ES8156_SetVolume(es8156_t *device, uint8_t volume);
status_t ES8156_Standby(const es8156_t *device);
status_t ES8156_Resume(const es8156_t *device);

#endif
