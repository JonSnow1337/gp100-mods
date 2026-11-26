#ifndef GP100_PERIPHERALS_H
#define GP100_PERIPHERALS_H

#include <stdint.h>

#include "fsl_device_registers.h"

/* Recovered V2 eDMA/DMAMUX allocation. Channel 0 is the ADC_ETC result path;
 * LPSPI1 channels 2/18 move panel command/pixel data; channels 3/19 carry SAI1
 * audio. The paired IRQ names (DMA0/16, DMA2/18, DMA3/19) do not mean that
 * channel 16 is configured by the application. */
#define GP100_DMA_ADC_CHANNEL       0u
#define GP100_DMA_PANEL_TX_CHANNEL  2u
#define GP100_DMA_SAI_TX_CHANNEL    3u
#define GP100_DMA_PANEL_RX_CHANNEL 18u
#define GP100_DMA_SAI_RX_CHANNEL   19u

#define GP100_AUDIO_SAMPLE_RATE_HZ 44100u
#define GP100_AUDIO_SLOT_WIDTH     32u
#define GP100_PANEL_SPI_BAUD_HZ    10000000u
#define GP100_PANEL_WIDTH          128u
#define GP100_PANEL_HEIGHT         160u
#define GP100_PANEL_PIXEL_BITS     16u
#define GP100_PANEL_MADCTL         0xc8u
#define GP100_PANEL_COLMOD         0x55u
#define GP100_MIDI_BAUD_HZ         31250u
#define GP100_CONTROL_I2C_BAUD_HZ  100000u

enum
{
    kGP100_ErrorAdcCalibration = 1u << 0,
    kGP100_ErrorMidiClock      = 1u << 1,
    kGP100_ErrorMidiInit       = 1u << 2,
    kGP100_ErrorI2cClock       = 1u << 3,
    kGP100_ErrorSpiClock       = 1u << 4,
    kGP100_ErrorSaiClock       = 1u << 5,
    kGP100_ErrorPitClock       = 1u << 6,
    kGP100_ErrorControlTimerClock = 1u << 7,
    kGP100_ErrorTunerTimerClock = 1u << 8,
    kGP100_ErrorRhythmTimerClock = 1u << 9,
    kGP100_ErrorDisplayInit = 1u << 10,
    kGP100_ErrorAnalogStart = 1u << 11,
    kGP100_ErrorMidiStart = 1u << 12,
    kGP100_ErrorUsbStart = 1u << 13,
    kGP100_ErrorFlashStorage = 1u << 14,
    kGP100_ErrorFactoryReset = 1u << 15,
    kGP100_ErrorAudioStart = 1u << 16,
    kGP100_ErrorTunerStart = 1u << 17,
    kGP100_ErrorAudioClock = 1u << 18,
};

void GP100_InitPeripherals(void);
void GP100_InitPanelSpi(void);

#endif
