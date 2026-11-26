#ifndef GP100_ANALOG_CONTROL_H
#define GP100_ANALOG_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_ANALOG_CHANNEL_COUNT 2u
#define GP100_ANALOG_HISTORY_LENGTH 32u
#define GP100_ANALOG_ADC_SCALE 4096.0f
#define GP100_ANALOG_FILTER_HZ 36.0f
#define GP100_ANALOG_SAMPLE_HZ 10000.0f

typedef enum gp100_analog_channel {
    /* ADC1_IN9 / GPIO_AD_B1_04, displayed as TUNER in Hardware Test. */
    GP100_ANALOG_HARDWARE_TEST_TUNER = 0,
    /* ADC1_IN6 / GPIO_AD_B1_01, built-in expression pedal. */
    GP100_ANALOG_EXPRESSION_PEDAL = 1
} gp100_analog_channel_t;

/* Compatibility spellings. This diagnostic ADC input remains distinct from
 * the GPIO_AD_B1_05/QTMR pitch-period path used by normal Tuner mode. */
#define GP100_ANALOG_AUXILIARY_INPUT \
    GP100_ANALOG_HARDWARE_TEST_TUNER
#define GP100_ANALOG_TUNER_INPUT \
    GP100_ANALOG_HARDWARE_TEST_TUNER

typedef struct gp100_analog_biquad {
    float b0;
    float b1;
    float b2;
    float a0;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
} gp100_analog_biquad_t;

typedef struct gp100_analog_control {
    gp100_analog_biquad_t filter[GP100_ANALOG_CHANNEL_COUNT];
    uint16_t history[GP100_ANALOG_CHANNEL_COUNT]
                    [GP100_ANALOG_HISTORY_LENGTH];
    uint32_t sum[GP100_ANALOG_CHANNEL_COUNT];
    uint16_t filtered[GP100_ANALOG_CHANNEL_COUNT];
    uint16_t history_index;
    uint16_t expression_value;
} gp100_analog_control_t;

/* Configure the two factory 36 Hz low-pass sections for a 10 kHz sample rate. */
void gp100_analog_control_init(gp100_analog_control_t *control);

/* Factory channel-1 pedal curve: 4096 * (0.7*x^0.4 + 0.3*x^96). */
uint16_t gp100_expression_curve(uint16_t filtered_adc);

/* Consume one ADC_ETC/DMA pair and return the debounced built-in pedal value. */
uint16_t gp100_analog_control_process(
    gp100_analog_control_t *control,
    const uint16_t dma_samples[GP100_ANALOG_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif
