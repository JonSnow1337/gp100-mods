#ifndef GP100_ANALOG_ACQUISITION_H
#define GP100_ANALOG_ACQUISITION_H

#include <stdbool.h>
#include <stdint.h>

#define GP100_ADC_CHANNEL_COUNT 2u
#define GP100_ANALOG_SAMPLE_RATE_HZ 10000u

typedef enum
{
    /* ADC1_IN9 / GPIO_AD_B1_04, raw TUNER diagnostic in Hardware Test. */
    kGP100_AnalogHardwareTestTuner = 0u,
    /* ADC1_IN6 / GPIO_AD_B1_01. Built-in expression pedal. */
    kGP100_AnalogExpressionPedal = 1u,
} gp100_adc_channel_t;

#define kGP100_AnalogAuxiliary kGP100_AnalogHardwareTestTuner

typedef void (*gp100_analog_pair_callback_t)(
    const uint16_t samples[GP100_ADC_CHANNEL_COUNT],
    uint32_t completedPairs,
    void *userData);

/* Configure the recovered ADC1/PIT/XBARA1/ADC_ETC/eDMA path. This function
 * leaves PIT0 and the DMA request disabled; call GP100_AnalogStart explicitly. */
void GP100_AnalogAcquisitionInit(void);

bool GP100_AnalogStart(void);
void GP100_AnalogStop(void);
bool GP100_AnalogIsRunning(void);

/* Install an optional interrupt-context consumer. It is called for every
 * completed 10 kHz pair, after the DMA and ADC_ETC flags have been cleared.
 * Keep it bounded below 100 us. Passing NULL removes the callback. */
void GP100_AnalogSetPairCallback(gp100_analog_pair_callback_t callback,
                                void *userData);

/* Copy the most recently completed pair. Returns false until the first pair
 * has arrived. completedPairs may be NULL; otherwise it receives a monotonic
 * completion count that lets a polling caller reject duplicate samples. */
bool GP100_AnalogReadLatest(uint16_t samples[GP100_ADC_CHANNEL_COUNT],
                           uint32_t *completedPairs);

#endif
