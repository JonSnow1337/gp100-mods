#ifndef GP100_ES7243_H
#define GP100_ES7243_H

#include <stdbool.h>
#include <stdint.h>

#include "codec_i2c.h"

/* ES7243 AD1/AD0 address strap table, expressed as MCUXpresso 7-bit addresses. */
#define ES7243_I2C_ADDRESS_AD1_HIGH_AD0_HIGH 0x10u
#define ES7243_I2C_ADDRESS_AD1_HIGH_AD0_LOW  0x11u
#define ES7243_I2C_ADDRESS_AD1_LOW_AD0_HIGH  0x12u
#define ES7243_I2C_ADDRESS_AD1_LOW_AD0_LOW   0x13u

#define ES7243_CHIP_ID 0x40u

typedef enum
{
    kES7243_I2s24Bit = 0x00u,
    kES7243_I2s16Bit = 0x0cu,
} es7243_word_length_t;

/* Register 0x08 values from the ES7243 user guide. These values also select
 * the normal analogue-input path; they are not linear dB encodings. */
typedef enum
{
    kES7243_Gain1Db   = 0x11u,
    kES7243_Gain3_5Db = 0x13u,
    kES7243_Gain18Db  = 0x21u,
    kES7243_Gain20_5Db = 0x23u,
    kES7243_Gain22_5Db = 0x06u,
    kES7243_Gain24_5Db = 0x41u,
    kES7243_Gain25Db   = 0x07u,
    kES7243_Gain27Db   = 0x43u,
} es7243_gain_t;

typedef struct
{
    gp100_codec_i2c_t i2c;
} es7243_t;

void ES7243_Bind(es7243_t *device, LPI2C_Type *base, uint8_t address);
status_t ES7243_ReadRegister(const es7243_t *device, uint8_t reg, uint8_t *value);
status_t ES7243_WriteRegister(const es7243_t *device, uint8_t reg, uint8_t value);
status_t ES7243_Probe(const es7243_t *device);
status_t ES7243_StartSlave(const es7243_t *device,
                           es7243_word_length_t wordLength,
                           es7243_gain_t gain,
                           bool unmute);
status_t ES7243_SetMute(const es7243_t *device, bool mute);
status_t ES7243_SetGain(const es7243_t *device, es7243_gain_t gain);
status_t ES7243_Standby(const es7243_t *device);

#endif
