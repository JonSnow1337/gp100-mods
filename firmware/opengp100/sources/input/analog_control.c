#include "analog_control.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define GP100_TWO_PI 6.28318530717958647692f
#define GP100_SQRT_TWO 1.41421356237309504880f

static void initialize_low_pass(gp100_analog_biquad_t *filter)
{
    const float omega = GP100_TWO_PI * GP100_ANALOG_FILTER_HZ /
                        GP100_ANALOG_SAMPLE_HZ;
    const float cosine = cosf(omega);
    const float alpha = sinf(omega) / GP100_SQRT_TWO;

    filter->b0 = (1.0f - cosine) * 0.5f;
    filter->b1 = 1.0f - cosine;
    filter->b2 = filter->b0;
    filter->a0 = 1.0f + alpha;
    filter->a1 = -2.0f * cosine;
    filter->a2 = 1.0f - alpha;
}

static uint16_t filter_sample(gp100_analog_biquad_t *filter,
                              uint16_t sample)
{
    float output = ((float)sample * filter->b0 +
                    filter->x1 * filter->b1 +
                    filter->x2 * filter->b2 -
                    filter->y1 * filter->a1 -
                    filter->y2 * filter->a2) / filter->a0;

    filter->x2 = filter->x1;
    filter->x1 = (float)sample;
    filter->y2 = filter->y1;
    filter->y1 = output;
    if (output <= 0.0f) {
        return 0u;
    }
    return (uint16_t)(uint32_t)output;
}

void gp100_analog_control_init(gp100_analog_control_t *control)
{
    size_t channel;

    if (control == NULL) {
        return;
    }
    memset(control, 0, sizeof(*control));
    for (channel = 0u; channel < GP100_ANALOG_CHANNEL_COUNT; ++channel) {
        initialize_low_pass(&control->filter[channel]);
    }
}

uint16_t gp100_expression_curve(uint16_t filtered_adc)
{
    const float normalized = (float)filtered_adc / GP100_ANALOG_ADC_SCALE;
    const double endpoint = 0.3 * pow((double)normalized, 96.0);
    const double travel = 0.7 * (double)powf(normalized, 0.4f);
    const double result = (travel + endpoint) * GP100_ANALOG_ADC_SCALE;

    return result <= 0.0 ? 0u : (uint16_t)(uint32_t)result;
}

uint16_t gp100_analog_control_process(
    gp100_analog_control_t *control,
    const uint16_t dma_samples[GP100_ANALOG_CHANNEL_COUNT])
{
    uint32_t channel;
    uint32_t target;

    if (control == NULL || dma_samples == NULL) {
        return 0u;
    }
    for (channel = 0u; channel < GP100_ANALOG_CHANNEL_COUNT; ++channel) {
        uint16_t value = filter_sample(&control->filter[channel],
                                       dma_samples[channel]);
        if (channel == GP100_ANALOG_EXPRESSION_PEDAL) {
            value = gp100_expression_curve(value);
        }
        control->sum[channel] -=
            control->history[channel][control->history_index];
        control->history[channel][control->history_index] = value;
        control->filtered[channel] = value;
        control->sum[channel] += value;
    }
    control->history_index =
        (uint16_t)((control->history_index + 1u) &
                   (GP100_ANALOG_HISTORY_LENGTH - 1u));

    target = (control->sum[GP100_ANALOG_EXPRESSION_PEDAL] >> 9u) &
             UINT32_C(0xffff);
    if ((target > control->expression_value
             ? target - control->expression_value
             : control->expression_value - target) >= 2u) {
        control->expression_value = target < 3u ? 0u : (uint16_t)target;
    }
    return control->expression_value;
}
