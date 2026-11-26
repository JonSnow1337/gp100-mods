#include "audio_transport.h"

#include "board/board.h"
#include "board/peripherals.h"

#include "fsl_common.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_sai.h"

#include <string.h>

#define GP100_AUDIO_DMA_HALF_COUNT 2u
#define GP100_AUDIO_DMA_WORD_COUNT \
    (GP100_AUDIO_BLOCK_SAMPLES * GP100_AUDIO_DMA_HALF_COUNT)
#define GP100_AUDIO_DMA_ITERATION_MASK 0x7fffu
#define GP100_AUDIO_DMA_CSR 0x1006u
#define GP100_AUDIO_TX_PUBLISH_MARGIN_WORDS 8u
#define GP100_AUDIO_FLASH_GAIN_FULL 128
#define GP100_AUDIO_FLASH_WAIT_MS 40u
#define GP100_AUDIO_ALL_TX_HALVES ((1u << GP100_AUDIO_DMA_HALF_COUNT) - 1u)

typedef struct gp100_audio_timing
{
    uint32_t block_budget_cycles;
    uint32_t last_block_cycles;
    uint32_t peak_block_cycles;
    uint32_t over_budget_blocks;
    uint32_t late_tx_blocks;
} gp100_audio_timing_t;

/* Factory V2 uses 1 KiB circular TCD buffers at 0x20013960 (TX) and
 * 0x20014560 (RX). The base keeps equivalent buffers in non-cacheable OCRAM,
 * avoiding cache maintenance and making eDMA ownership explicit. */
AT_NONCACHEABLE_SECTION_ALIGN(
    static int32_t s_txSaiWords[GP100_AUDIO_DMA_WORD_COUNT], 32);
AT_NONCACHEABLE_SECTION_ALIGN(
    static int32_t s_rxSaiWords[GP100_AUDIO_DMA_WORD_COUNT], 32);

static int32_t s_capturedSamples[GP100_AUDIO_BLOCK_SAMPLES];
static int32_t s_outputSamples[GP100_AUDIO_BLOCK_SAMPLES];
static volatile uint32_t s_completedBlocks;
static volatile uint32_t s_capturedBlocks;
static volatile gp100_audio_timing_t s_timing;
static volatile bool s_initialized;
static volatile bool s_running;
static volatile bool s_flashQuietRequested;
static volatile uint8_t s_flashSilentHalves;
static int32_t s_flashGain;
static gp100_audio_block_callback_t s_callback;
static void *s_callbackData;

static bool GP100_AudioDmaUsesSecondHalf(uint32_t channel)
{
    const uint16_t beginning =
        DMA0->TCD[channel].BITER_ELINKNO & GP100_AUDIO_DMA_ITERATION_MASK;
    const uint16_t current =
        DMA0->TCD[channel].CITER_ELINKNO & GP100_AUDIO_DMA_ITERATION_MASK;

    return (uint16_t)(beginning >> 1) < current;
}

static void GP100_AudioConfigureTxDma(void)
{
    edma_tcd_t tcd __attribute__((aligned(32))) = {0};

    tcd.SADDR = (uint32_t)(uintptr_t)s_txSaiWords;
    tcd.SOFF = 4;
    tcd.ATTR = DMA_ATTR_SSIZE(kEDMA_TransferSize4Bytes) |
               DMA_ATTR_DSIZE(kEDMA_TransferSize4Bytes);
    tcd.NBYTES = 4u;
    tcd.SLAST = (uint32_t)(-(int32_t)sizeof(s_txSaiWords));
    tcd.DADDR = SAI_TxGetDataRegisterAddress(SAI1, 0u);
    tcd.DOFF = 0;
    tcd.CITER = DMA_CITER_ELINKNO_CITER(GP100_AUDIO_DMA_WORD_COUNT);
    tcd.DLAST_SGA = 0u;
    /* The shipped 0x1006 value enables half and major interrupts. Its unused
     * MAJORLINKCH field contains 16 while MAJORELINK remains clear. */
    tcd.CSR = GP100_AUDIO_DMA_CSR;
    tcd.BITER = DMA_BITER_ELINKNO_BITER(GP100_AUDIO_DMA_WORD_COUNT);
    EDMA_InstallTCD(DMA0, GP100_DMA_SAI_TX_CHANNEL, &tcd);
}

static void GP100_AudioConfigureRxDma(void)
{
    edma_tcd_t tcd __attribute__((aligned(32))) = {0};

    tcd.SADDR = (uint32_t)(uintptr_t)&SAI1->RDR[0];
    tcd.SOFF = 0;
    tcd.ATTR = DMA_ATTR_SSIZE(kEDMA_TransferSize4Bytes) |
               DMA_ATTR_DSIZE(kEDMA_TransferSize4Bytes);
    tcd.NBYTES = 4u;
    tcd.SLAST = 0u;
    tcd.DADDR = (uint32_t)(uintptr_t)s_rxSaiWords;
    tcd.DOFF = 4;
    tcd.CITER = DMA_CITER_ELINKNO_CITER(GP100_AUDIO_DMA_WORD_COUNT);
    tcd.DLAST_SGA = (uint32_t)(-(int32_t)sizeof(s_rxSaiWords));
    tcd.CSR = GP100_AUDIO_DMA_CSR;
    tcd.BITER = DMA_BITER_ELINKNO_BITER(GP100_AUDIO_DMA_WORD_COUNT);
    EDMA_InstallTCD(DMA0, GP100_DMA_SAI_RX_CHANNEL, &tcd);
}

static void GP100_AudioClearDmaStatus(uint32_t channel)
{
    EDMA_ClearChannelStatusFlags(DMA0, channel,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag |
                                     kEDMA_InterruptFlag);
}

static void GP100_AudioResetTiming(void)
{
    s_timing = (gp100_audio_timing_t){0};
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) == 0u)
    {
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u)
        {
            s_timing.block_budget_cycles =
                (uint32_t)(((uint64_t)SystemCoreClock * GP100_AUDIO_BLOCK_FRAMES) /
                           GP100_AUDIO_SAMPLE_RATE_HZ);
        }
    }
}

static bool GP100_AudioCanPublishTxHalf(bool secondHalf)
{
    const uint16_t current =
        DMA0->TCD[GP100_DMA_SAI_TX_CHANNEL].CITER_ELINKNO &
            GP100_AUDIO_DMA_ITERATION_MASK;
    const bool secondHalfFree = current > GP100_AUDIO_BLOCK_SAMPLES;
    const uint16_t wordsRemaining = secondHalfFree
        ? current - GP100_AUDIO_BLOCK_SAMPLES
        : current;

    /* A full half can elapse while the callback is running. The original
     * destination may already belong to DMA when processing returns. */
    return secondHalf == secondHalfFree &&
           wordsRemaining > GP100_AUDIO_TX_PUBLISH_MARGIN_WORDS;
}

static void GP100_AudioPublishTxHalf(bool secondHalf)
{
    const size_t offset = secondHalf ? GP100_AUDIO_BLOCK_SAMPLES : 0u;
    const uint8_t half = (uint8_t)(1u << (secondHalf ? 1u : 0u));
    const bool quiet = s_flashQuietRequested;

    if (quiet && s_flashGain == 0)
    {
        memset(&s_txSaiWords[offset], 0, sizeof(s_outputSamples));
        __DMB();
        s_flashSilentHalves |= half;
        return;
    }
    s_flashSilentHalves &= (uint8_t)~half;
    if (!quiet && s_flashGain == GP100_AUDIO_FLASH_GAIN_FULL)
    {
        for (size_t sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; ++sample)
        {
            s_txSaiWords[offset + sample] =
                (int32_t)((uint32_t)s_outputSamples[sample] << 8);
        }
        return;
    }

    const int32_t target = quiet ? 0 : GP100_AUDIO_FLASH_GAIN_FULL;
    const int32_t step = quiet ? -1 : 1;
    for (size_t sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; sample += 2u)
    {
        const int32_t left = (s_outputSamples[sample] * s_flashGain) /
                             GP100_AUDIO_FLASH_GAIN_FULL;
        const int32_t right = (s_outputSamples[sample + 1u] * s_flashGain) /
                              GP100_AUDIO_FLASH_GAIN_FULL;

        s_txSaiWords[offset + sample] = (int32_t)((uint32_t)left << 8);
        s_txSaiWords[offset + sample + 1u] = (int32_t)((uint32_t)right << 8);
        if (s_flashGain != target)
        {
            s_flashGain += step;
        }
    }
}

static void GP100_AudioProcessTxHalf(uint32_t startedAt, uint32_t budget)
{
    const bool secondHalf =
        GP100_AudioDmaUsesSecondHalf(GP100_DMA_SAI_TX_CHANNEL);
    gp100_audio_block_callback_t callback = s_callback;
    uint32_t sequence = s_completedBlocks + 1u;

    if (callback != NULL)
    {
        callback(s_capturedSamples, s_outputSamples, sequence,
                 s_callbackData);
    }
    else
    {
        memcpy(s_outputSamples, s_capturedSamples,
               sizeof(s_outputSamples));
    }

    const bool canPublish =
        (budget == 0u || (uint32_t)(DWT->CYCCNT - startedAt) < budget) &&
        GP100_AudioCanPublishTxHalf(secondHalf);
    if (canPublish)
    {
        GP100_AudioPublishTxHalf(secondHalf);
    }
    else
    {
        ++s_timing.late_tx_blocks;
    }
    __DMB();
    s_completedBlocks = sequence;
}

static void GP100_AudioCaptureRxHalf(void)
{
    const uint32_t offset = GP100_AudioDmaUsesSecondHalf(
                                GP100_DMA_SAI_RX_CHANNEL)
                                ? GP100_AUDIO_BLOCK_SAMPLES
                                : 0u;
    uint32_t sample;

    for (sample = 0u; sample < GP100_AUDIO_BLOCK_SAMPLES; ++sample)
    {
        s_capturedSamples[sample] = s_rxSaiWords[offset + sample] >> 8;
    }
    __DMB();
    ++s_capturedBlocks;
}

void GP100_AudioTransportInit(void)
{
    s_running = false;
    s_initialized = false;
    s_completedBlocks = 0u;
    s_capturedBlocks = 0u;
    s_timing = (gp100_audio_timing_t){0};
    memset(s_txSaiWords, 0, sizeof(s_txSaiWords));
    memset(s_rxSaiWords, 0, sizeof(s_rxSaiWords));
    memset(s_capturedSamples, 0, sizeof(s_capturedSamples));
    memset(s_outputSamples, 0, sizeof(s_outputSamples));

    SAI_TxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    SAI_RxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_RX_CHANNEL);

    DMAMUX_SetSource(DMAMUX, GP100_DMA_SAI_TX_CHANNEL,
                     kDmaRequestMuxSai1Tx);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_SAI_RX_CHANNEL,
                     kDmaRequestMuxSai1Rx);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_SAI_TX_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_SAI_RX_CHANNEL);
    GP100_AudioConfigureTxDma();
    GP100_AudioConfigureRxDma();
    GP100_AudioClearDmaStatus(GP100_DMA_SAI_TX_CHANNEL);
    GP100_AudioClearDmaStatus(GP100_DMA_SAI_RX_CHANNEL);

    /* Factory audio_init writes 0x30 to the IRQ3 priority byte. */
    NVIC_SetPriority(DMA3_DMA19_IRQn, 3u);
    NVIC_ClearPendingIRQ(DMA3_DMA19_IRQn);
    s_initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitAudioTransport;
}

bool GP100_AudioTransportStart(void)
{
    if (!s_initialized)
    {
        return false;
    }
    if (s_running)
    {
        return true;
    }

    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_RX_CHANNEL);
    SAI_TxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    SAI_RxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    GP100_AudioClearDmaStatus(GP100_DMA_SAI_TX_CHANNEL);
    GP100_AudioClearDmaStatus(GP100_DMA_SAI_RX_CHANNEL);
    GP100_AudioConfigureTxDma();
    GP100_AudioConfigureRxDma();
    memset(s_txSaiWords, 0, sizeof(s_txSaiWords));
    memset(s_rxSaiWords, 0, sizeof(s_rxSaiWords));
    memset(s_capturedSamples, 0, sizeof(s_capturedSamples));
    memset(s_outputSamples, 0, sizeof(s_outputSamples));
    s_completedBlocks = 0u;
    s_capturedBlocks = 0u;
    s_flashQuietRequested = false;
    s_flashSilentHalves = 0u;
    s_flashGain = GP100_AUDIO_FLASH_GAIN_FULL;
    GP100_AudioResetTiming();

    SAI_TxSoftwareReset(SAI1, kSAI_ResetTypeFIFO);
    SAI_RxSoftwareReset(SAI1, kSAI_ResetTypeFIFO);
    NVIC_ClearPendingIRQ(DMA3_DMA19_IRQn);
    NVIC_EnableIRQ(DMA3_DMA19_IRQn);
    s_running = true;

    SAI_TxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, true);
    SAI_RxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, true);
    SAI_TxEnable(SAI1, true);
    SAI_RxEnable(SAI1, true);
    /* Preserve the observed request order: TX channel 3, then RX channel 19. */
    EDMA_EnableChannelRequest(DMA0, GP100_DMA_SAI_TX_CHANNEL);
    EDMA_EnableChannelRequest(DMA0, GP100_DMA_SAI_RX_CHANNEL);
    return true;
}

void GP100_AudioTransportStop(void)
{
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_SAI_RX_CHANNEL);
    SAI_TxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    SAI_RxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, false);
    SAI_TxEnable(SAI1, false);
    SAI_RxEnable(SAI1, false);
    NVIC_DisableIRQ(DMA3_DMA19_IRQn);
    NVIC_ClearPendingIRQ(DMA3_DMA19_IRQn);
    s_running = false;
}

bool GP100_AudioTransportIsRunning(void)
{
    return s_running;
}

bool GP100_AudioTransportBeginFlashWrite(void)
{
    const uint32_t started = g_gp100_board_state.milliseconds;

    if (!s_running)
    {
        return true;
    }
    s_flashQuietRequested = true;
    while (s_flashSilentHalves != GP100_AUDIO_ALL_TX_HALVES)
    {
        if (g_gp100_board_state.milliseconds - started >=
            GP100_AUDIO_FLASH_WAIT_MS)
        {
            s_flashQuietRequested = false;
            return false;
        }
        __WFI();
    }
    return true;
}

void GP100_AudioTransportEndFlashWrite(void)
{
    s_flashQuietRequested = false;
}

void GP100_AudioTransportSetCallback(gp100_audio_block_callback_t callback,
                                    void *userData)
{
    const uint32_t interruptMask = DisableGlobalIRQ();

    s_callbackData = userData;
    __DMB();
    s_callback = callback;
    EnableGlobalIRQ(interruptMask);
}

uint32_t GP100_AudioTransportCompletedBlocks(void)
{
    return s_completedBlocks;
}

uint32_t GP100_AudioTransportCapturedBlocks(void)
{
    return s_capturedBlocks;
}

void DMA3_DMA19_IRQHandler(void)
{
    const uint32_t budget = s_timing.block_budget_cycles;
    const uint32_t start = budget != 0u ? DWT->CYCCNT : 0u;
    uint32_t flags;
    bool rendered = false;

    /* The factory services channel 3 first. Its just-freed TX half is filled
     * from the work block staged by the most recent channel-19 completion. */
    flags = EDMA_GetChannelStatusFlags(DMA0, GP100_DMA_SAI_TX_CHANNEL);
    if ((flags & kEDMA_InterruptFlag) != 0u)
    {
        GP100_AudioClearDmaStatus(GP100_DMA_SAI_TX_CHANNEL);
        if (s_running)
        {
            GP100_AudioProcessTxHalf(start, budget);
            rendered = true;
        }
    }

    flags = EDMA_GetChannelStatusFlags(DMA0, GP100_DMA_SAI_RX_CHANNEL);
    if ((flags & kEDMA_InterruptFlag) != 0u)
    {
        GP100_AudioClearDmaStatus(GP100_DMA_SAI_RX_CHANNEL);
        if (s_running)
        {
            GP100_AudioCaptureRxHalf();
        }
    }
    if (rendered && budget != 0u)
    {
        const uint32_t cycles = DWT->CYCCNT - start;

        s_timing.last_block_cycles = cycles;
        if (cycles > s_timing.peak_block_cycles)
        {
            s_timing.peak_block_cycles = cycles;
        }
        if (cycles >= budget)
        {
            ++s_timing.over_budget_blocks;
        }
    }
}
