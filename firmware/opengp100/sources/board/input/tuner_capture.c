#include "tuner_capture.h"

#include "board/board.h"
#include "board/peripherals.h"

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_qtmr.h"

#define GP100_TUNER_GPIO_PIN 21u
#define GP100_TUNER_GPIO_MASK (UINT32_C(1) << GP100_TUNER_GPIO_PIN)
#define GP100_TUNER_COUNTER_DIVIDER 4u
#define GP100_TUNER_CAPTURE_CALIBRATION 35u

static volatile uint32_t
    s_intervals[GP100_TUNER_CAPTURE_HISTORY_LENGTH];
static volatile uint32_t s_sequence;
static volatile uint32_t s_counterClockHz;
static volatile uint16_t s_captureCount;
static volatile bool s_initialized;
static volatile bool s_running;

static uint32_t GP100_TunerReadAndResetCounter(void)
{
    uint32_t low = TMR3->CHANNEL[kQTMR_Channel_0].CNTR;
    uint32_t high = TMR3->CHANNEL[kQTMR_Channel_1].CNTR;

    TMR3->CHANNEL[kQTMR_Channel_0].CNTR = 0u;
    TMR3->CHANNEL[kQTMR_Channel_1].CNTR = 0u;
    return low | (high << 16);
}

static uint32_t GP100_TunerNormalizePeriod(uint32_t rawCount)
{
    uint64_t scaled = (uint64_t)rawCount *
                      GP100_TUNER_FACTORY_PERIOD_CLOCK_HZ;

    return (uint32_t)(scaled / s_counterClockHz) +
           GP100_TUNER_CAPTURE_CALIBRATION;
}

void GP100_TunerCaptureInit(void)
{
    qtmr_config_t config;
    uint32_t index;
    const uint32_t ipgClockHz = CLOCK_GetFreq(kCLOCK_IpgClk);

    s_running = false;
    s_initialized = false;
    s_sequence = 0u;
    s_captureCount = 0u;
    s_counterClockHz = ipgClockHz / GP100_TUNER_COUNTER_DIVIDER;
    for (index = 0u; index < GP100_TUNER_CAPTURE_HISTORY_LENGTH; ++index)
    {
        s_intervals[index] = 0u;
    }
    if (s_counterClockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorTunerTimerClock;
        return;
    }

    QTMR_StopTimer(TMR3, kQTMR_Channel_0);
    QTMR_StopTimer(TMR3, kQTMR_Channel_1);
    QTMR_DisableInterrupts(TMR3, kQTMR_Channel_1,
                           kQTMR_CompareInterruptEnable);

    QTMR_GetDefaultConfig(&config);
    config.primarySource = kQTMR_ClockDivide_4;
    QTMR_Init(TMR3, kQTMR_Channel_0, &config);
    config.primarySource = kQTMR_ClockCounter0Output;
    QTMR_Init(TMR3, kQTMR_Channel_1, &config);
    QTMR_SetTimerPeriod(TMR3, kQTMR_Channel_0, UINT16_MAX);
    QTMR_SetTimerPeriod(TMR3, kQTMR_Channel_1, UINT16_MAX);
    QTMR_EnableInterrupts(TMR3, kQTMR_Channel_1,
                          kQTMR_CompareInterruptEnable);
    QTMR_ClearStatusFlags(TMR3, kQTMR_Channel_1, kQTMR_CompareFlag);

    GPIO_PortDisableInterrupts(GPIO1, GP100_TUNER_GPIO_MASK);
    GPIO_PinSetInterruptConfig(GPIO1, GP100_TUNER_GPIO_PIN,
                               kGPIO_IntFallingEdge);
    GPIO_PortClearInterruptFlags(GPIO1, GP100_TUNER_GPIO_MASK);

    /* Factory initialization assigns both IRQs the encoded priority 0x20/0x50. */
    NVIC_SetPriority(GPIO1_Combined_16_31_IRQn, 2u);
    NVIC_SetPriority(TMR3_IRQn, 5u);
    NVIC_ClearPendingIRQ(GPIO1_Combined_16_31_IRQn);
    NVIC_ClearPendingIRQ(TMR3_IRQn);

    s_initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitTunerCapture;
}

bool GP100_TunerCaptureStart(void)
{
    uint32_t index;

    if (!s_initialized)
    {
        return false;
    }
    if (s_running)
    {
        return true;
    }

    s_sequence = 0u;
    s_captureCount = 0u;
    for (index = 0u; index < GP100_TUNER_CAPTURE_HISTORY_LENGTH; ++index)
    {
        s_intervals[index] = 0u;
    }
    TMR3->CHANNEL[kQTMR_Channel_0].CNTR = 0u;
    TMR3->CHANNEL[kQTMR_Channel_1].CNTR = 0u;
    QTMR_ClearStatusFlags(TMR3, kQTMR_Channel_1, kQTMR_CompareFlag);
    GPIO_PortClearInterruptFlags(GPIO1, GP100_TUNER_GPIO_MASK);
    NVIC_ClearPendingIRQ(TMR3_IRQn);
    NVIC_ClearPendingIRQ(GPIO1_Combined_16_31_IRQn);

    s_running = true;
    EnableIRQ(TMR3_IRQn);
    EnableIRQ(GPIO1_Combined_16_31_IRQn);
    QTMR_StartTimer(TMR3, kQTMR_Channel_1, kQTMR_CascadeCount);
    QTMR_StartTimer(TMR3, kQTMR_Channel_0, kQTMR_PriSrcRiseEdge);
    GPIO_PortEnableInterrupts(GPIO1, GP100_TUNER_GPIO_MASK);
    return true;
}

void GP100_TunerCaptureStop(void)
{
    GPIO_PortDisableInterrupts(GPIO1, GP100_TUNER_GPIO_MASK);
    DisableIRQ(GPIO1_Combined_16_31_IRQn);
    QTMR_StopTimer(TMR3, kQTMR_Channel_0);
    QTMR_StopTimer(TMR3, kQTMR_Channel_1);
    DisableIRQ(TMR3_IRQn);
    NVIC_ClearPendingIRQ(GPIO1_Combined_16_31_IRQn);
    NVIC_ClearPendingIRQ(TMR3_IRQn);
    s_running = false;
}

bool GP100_TunerCaptureIsRunning(void)
{
    return s_running;
}

bool GP100_TunerCaptureReadIntervals(
    uint32_t intervals[GP100_TUNER_FACTORY_INTERVAL_COUNT],
    uint32_t *sequence)
{
    uint32_t irqMask;
    uint32_t index;

    if (intervals == NULL)
    {
        return false;
    }
    irqMask = DisableGlobalIRQ();
    if (sequence != NULL)
    {
        *sequence = s_sequence;
    }
    if (s_captureCount < GP100_TUNER_FACTORY_INTERVAL_COUNT)
    {
        EnableGlobalIRQ(irqMask);
        return false;
    }
    for (index = 0u;
         index < GP100_TUNER_FACTORY_INTERVAL_COUNT;
         ++index)
    {
        intervals[index] = s_intervals[index];
    }
    EnableGlobalIRQ(irqMask);
    return true;
}

uint32_t GP100_TunerCaptureClockHz(void)
{
    return s_counterClockHz;
}

void GPIO1_Combined_16_31_IRQHandler(void)
{
    uint32_t flags = GPIO_PortGetInterruptFlags(GPIO1);
    uint32_t rawCount;
    uint32_t index;

    if ((flags & GP100_TUNER_GPIO_MASK) == 0u)
    {
        return;
    }
    GPIO_PortClearInterruptFlags(GPIO1, GP100_TUNER_GPIO_MASK);
    if (!s_running || (GPIO1->DR & GP100_TUNER_GPIO_MASK) != 0u)
    {
        return;
    }

    rawCount = GP100_TunerReadAndResetCounter();
    for (index = GP100_TUNER_CAPTURE_HISTORY_LENGTH - 1u;
         index != 0u;
         --index)
    {
        s_intervals[index] = s_intervals[index - 1u];
    }
    s_intervals[0] = GP100_TunerNormalizePeriod(rawCount);
    if (s_captureCount < GP100_TUNER_CAPTURE_HISTORY_LENGTH)
    {
        ++s_captureCount;
    }
    ++s_sequence;
}

void TMR3_IRQHandler(void)
{
    QTMR_ClearStatusFlags(TMR3, kQTMR_Channel_1, kQTMR_CompareFlag);
}
