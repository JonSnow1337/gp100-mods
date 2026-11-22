#include "analog_acquisition.h"

#include "board/board.h"
#include "board/peripherals.h"

#include "fsl_adc.h"
#include "fsl_adc_etc.h"
#include "fsl_clock.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_pit.h"
#include "fsl_xbara.h"

#define GP100_ANALOG_TRIGGER_GROUP 0u
#define GP100_ANALOG_PIT_PERIOD_US 100u
#define GP100_ANALOG_ADC_DISABLED_CHANNEL 16u
#define GP100_ANALOG_ADC_AUX_CHANNEL 9u
#define GP100_ANALOG_ADC_EXPRESSION_CHANNEL 6u
#define GP100_ANALOG_DMA_MAJOR_COUNT 1u
#define GP100_ANALOG_DMA_BANDWIDTH 3u

/* Factory V2 places this pair in DTCM at 0x20001838. The base linker likewise
 * places ordinary BSS in DTCM, which is coherent with eDMA on this target. */
static volatile uint16_t s_dmaSamples[GP100_ADC_CHANNEL_COUNT]
    __attribute__((aligned(4)));
static volatile uint16_t s_latestSamples[GP100_ADC_CHANNEL_COUNT];
static volatile uint32_t s_publishGuard;
static volatile uint32_t s_completedPairs;
static volatile bool s_initialized;
static volatile bool s_running;
static gp100_analog_pair_callback_t s_pairCallback;
static void *s_pairCallbackData;

static void GP100_AnalogConfigureDma(void)
{
    /* One request copies both halfwords before SLAST/DLAST rewind the pair. */
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].SADDR =
        (uint32_t)(uintptr_t)&ADC_ETC->TRIG[0].TRIGn_RESULT_1_0;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].SOFF = 2u;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].ATTR =
        DMA_ATTR_SSIZE(kEDMA_TransferSize2Bytes) |
        DMA_ATTR_DSIZE(kEDMA_TransferSize2Bytes);
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].NBYTES_MLNO = sizeof(s_dmaSamples);
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].SLAST = -4;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].DADDR =
        (uint32_t)(uintptr_t)s_dmaSamples;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].DOFF = 2u;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].CITER_ELINKNO =
        DMA_CITER_ELINKNO_CITER(GP100_ANALOG_DMA_MAJOR_COUNT);
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].DLAST_SGA = -4;
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].CSR =
        DMA_CSR_INTMAJOR(1u) | DMA_CSR_BWC(GP100_ANALOG_DMA_BANDWIDTH);
    DMA0->TCD[GP100_DMA_ADC_CHANNEL].BITER_ELINKNO =
        DMA_BITER_ELINKNO_BITER(GP100_ANALOG_DMA_MAJOR_COUNT);
}

static void GP100_AnalogConfigureAdcEtc(void)
{
    adc_channel_config_t adcChannel = {
        .channelNumber = GP100_ANALOG_ADC_DISABLED_CHANNEL,
        .enableInterruptOnConversionCompleted = false,
    };
    adc_etc_config_t etcConfig;
    adc_etc_trigger_config_t triggerConfig = {0};
    adc_etc_trigger_chain_config_t chainConfig = {0};
    uint32_t group;

    for (group = 0u; group < 4u; ++group)
    {
        ADC_SetChannelConfig(ADC1, group, &adcChannel);
    }
    ADC_EnableHardwareTrigger(ADC1, true);

    ADC_ETC_GetDefaultConfig(&etcConfig);
    etcConfig.XBARtriggerMask = 1u << GP100_ANALOG_TRIGGER_GROUP;
    /* A latched request can keep this circular DMA channel active until the
     * CPU acknowledges it. Pulse once per ADC_ETC trigger instead. */
    etcConfig.dmaMode = kADC_ETC_TrigDMAWithPulsedSignal;
    ADC_ETC_Init(ADC_ETC, &etcConfig);

    triggerConfig.triggerChainLength = 3u;
    ADC_ETC_SetTriggerConfig(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP,
                             &triggerConfig);

    chainConfig.enableB2BMode = true;
    chainConfig.ADCHCRegisterSelect = 1u;
    chainConfig.ADCChannelSelect = GP100_ANALOG_ADC_AUX_CHANNEL;
    chainConfig.InterruptEnable = kADC_ETC_InterruptDisable;
    ADC_ETC_SetTriggerChainConfig(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP, 0u,
                                  &chainConfig);

    chainConfig.ADCHCRegisterSelect = 2u;
    chainConfig.ADCChannelSelect = GP100_ANALOG_ADC_EXPRESSION_CHANNEL;
    for (group = 1u; group < 4u; ++group)
    {
        ADC_ETC_SetTriggerChainConfig(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP,
                                      group, &chainConfig);
    }
    ADC_ETC_DisableDMA(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP);
}

void GP100_AnalogAcquisitionInit(void)
{
    const uint32_t pitClockHz = CLOCK_GetFreq(kCLOCK_PerClk);

    s_running = false;
    s_completedPairs = 0u;
    s_publishGuard = 0u;
    s_dmaSamples[0] = 0u;
    s_dmaSamples[1] = 0u;
    s_latestSamples[0] = 0u;
    s_latestSamples[1] = 0u;

    if (pitClockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorPitClock;
        s_initialized = false;
        return;
    }

    GP100_AnalogConfigureAdcEtc();
    XBARA_SetSignalsConnection(XBARA1, kXBARA1_InputPitTrigger0,
                               kXBARA1_OutputAdcEtcXbar0Trig0);

    /* Factory code writes this count directly to LDVAL. PIT counts LDVAL+1,
     * so retain its observed one-cycle offset instead of using the SDK's
     * count-to-LDVAL helper. */
    PIT->CHANNEL[kPIT_Chnl_0].LDVAL =
        (uint32_t)(((uint64_t)pitClockHz * GP100_ANALOG_PIT_PERIOD_US) /
                   1000000u);
    PIT_StopTimer(PIT, kPIT_Chnl_0);

    DMAMUX_SetSource(DMAMUX, GP100_DMA_ADC_CHANNEL,
                     kDmaRequestMuxADC_ETC);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_ADC_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_ADC_CHANNEL);
    GP100_AnalogConfigureDma();

    /* Factory setup writes 0x40 to the DMA0 NVIC priority byte. */
    NVIC_SetPriority(DMA0_DMA16_IRQn, 4u);
    NVIC_ClearPendingIRQ(DMA0_DMA16_IRQn);
    s_initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitAnalogAcquisition;
}

bool GP100_AnalogStart(void)
{
    if (!s_initialized)
    {
        return false;
    }
    if (s_running)
    {
        return true;
    }

    PIT_StopTimer(PIT, kPIT_Chnl_0);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_ADC_CHANNEL);
    EDMA_ClearChannelStatusFlags(DMA0, GP100_DMA_ADC_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag |
                                     kEDMA_InterruptFlag);
    ADC_ETC_ClearDMAStatusFlags(ADC_ETC,
                                1u << GP100_ANALOG_TRIGGER_GROUP);
    GP100_AnalogConfigureDma();
    NVIC_ClearPendingIRQ(DMA0_DMA16_IRQn);
    NVIC_EnableIRQ(DMA0_DMA16_IRQn);
    EDMA_EnableChannelRequest(DMA0, GP100_DMA_ADC_CHANNEL);
    ADC_ETC_EnableDMA(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP);
    s_running = true;
    PIT_StartTimer(PIT, kPIT_Chnl_0);
    return true;
}

void GP100_AnalogStop(void)
{
    PIT_StopTimer(PIT, kPIT_Chnl_0);
    ADC_ETC_DisableDMA(ADC_ETC, GP100_ANALOG_TRIGGER_GROUP);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_ADC_CHANNEL);
    NVIC_DisableIRQ(DMA0_DMA16_IRQn);
    s_running = false;
}

bool GP100_AnalogIsRunning(void)
{
    return s_running;
}

void GP100_AnalogSetPairCallback(gp100_analog_pair_callback_t callback,
                                void *userData)
{
    const uint32_t interruptMask = DisableGlobalIRQ();

    s_pairCallbackData = userData;
    __DMB();
    s_pairCallback = callback;
    EnableGlobalIRQ(interruptMask);
}

bool GP100_AnalogReadLatest(uint16_t samples[GP100_ADC_CHANNEL_COUNT],
                           uint32_t *completedPairs)
{
    uint32_t guardBefore;
    uint32_t guardAfter;
    uint32_t count;
    uint16_t auxiliary;
    uint16_t expression;

    if (samples == NULL)
    {
        return false;
    }
    do
    {
        guardBefore = s_publishGuard;
        if ((guardBefore & 1u) != 0u)
        {
            continue;
        }
        __DMB();
        auxiliary = s_latestSamples[kGP100_AnalogAuxiliary];
        expression = s_latestSamples[kGP100_AnalogExpressionPedal];
        count = s_completedPairs;
        __DMB();
        guardAfter = s_publishGuard;
    } while (guardBefore != guardAfter || (guardAfter & 1u) != 0u);

    samples[kGP100_AnalogAuxiliary] = auxiliary;
    samples[kGP100_AnalogExpressionPedal] = expression;
    if (completedPairs != NULL)
    {
        *completedPairs = count;
    }
    return count != 0u;
}

void DMA0_DMA16_IRQHandler(void)
{
    const uint32_t flags =
        EDMA_GetChannelStatusFlags(DMA0, GP100_DMA_ADC_CHANNEL);
    uint16_t completed[GP100_ADC_CHANNEL_COUNT];
    uint32_t count;

    if ((flags & kEDMA_InterruptFlag) == 0u)
    {
        return;
    }
    EDMA_ClearChannelStatusFlags(DMA0, GP100_DMA_ADC_CHANNEL,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag |
                                     kEDMA_InterruptFlag);
    ADC_ETC_ClearDMAStatusFlags(ADC_ETC, 1u << GP100_ANALOG_TRIGGER_GROUP);

    ++s_publishGuard;
    __DMB();
    completed[kGP100_AnalogAuxiliary] =
        s_dmaSamples[kGP100_AnalogAuxiliary] & 0x0fffu;
    completed[kGP100_AnalogExpressionPedal] =
        s_dmaSamples[kGP100_AnalogExpressionPedal] & 0x0fffu;
    s_latestSamples[kGP100_AnalogAuxiliary] =
        completed[kGP100_AnalogAuxiliary];
    s_latestSamples[kGP100_AnalogExpressionPedal] =
        completed[kGP100_AnalogExpressionPedal];
    count = ++s_completedPairs;
    __DMB();
    ++s_publishGuard;

    if (s_pairCallback != NULL)
    {
        s_pairCallback(completed, count, s_pairCallbackData);
    }
    SDK_ISR_EXIT_BARRIER;
}
