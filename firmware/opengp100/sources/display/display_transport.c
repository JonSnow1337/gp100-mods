#include "display_transport.h"

#include "board/board.h"
#include "gp100_display.h"
#include "board/peripherals.h"

#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_lpspi.h"

#define GP100_DISPLAY_DMA_ITERATION_MAX 0x7fffu
#define GP100_DISPLAY_TX_DMA_CSR 0x100au
#define GP100_DISPLAY_RX_DMA_CSR 0x000au
#define GP100_DISPLAY_DMA_MASK (kLPSPI_TxDmaEnable | kLPSPI_RxDmaEnable)
#define GP100_DISPLAY_DMA_TIMEOUT_MS 250u

/* The receive side exists only to prevent full-duplex LPSPI overrun. Keeping
 * its fixed destination out of the cache matches the ownership of the DMA. */
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_receiveDrain, 32);

static volatile bool s_initialized;
static volatile bool s_busy;
static volatile bool s_transferError;
static volatile uint32_t s_completedFrames;

GP100_SDRAM_BSS_SECTION_ALIGN(
    static uint16_t s_renderPixels[GP100_DISPLAY_PIXEL_COUNT], 32);
GP100_SDRAM_BSS_SECTION_ALIGN(
    static uint16_t s_transferPixels[GP100_DISPLAY_PIXEL_COUNT], 32);

static void GP100_DisplayClearDmaStatus(uint32_t channel)
{
    EDMA_ClearChannelStatusFlags(DMA0, channel,
                                 kEDMA_DoneFlag | kEDMA_ErrorFlag |
                                     kEDMA_InterruptFlag);
}

static void GP100_DisplayQueuePixelCommand(void)
{
    /* Each TCR write queues a command. A second read can still see the
     * previous 8-bit, RX-masked command, so set every field in one write. */
    const uint32_t command = LPSPI1->TCR;
    LPSPI1->TCR =
        (command & ~(LPSPI_TCR_FRAMESZ_MASK | LPSPI_TCR_CONT_MASK |
                     LPSPI_TCR_CONTC_MASK | LPSPI_TCR_RXMSK_MASK |
                     LPSPI_TCR_TXMSK_MASK | LPSPI_TCR_PCS_MASK)) |
        LPSPI_TCR_FRAMESZ(GP100_PANEL_PIXEL_BITS - 1u) |
        LPSPI_TCR_PCS(kLPSPI_Pcs0);
}

static void GP100_DisplayConfigureTxDma(const uint16_t *pixels,
                                        uint16_t pixelCount)
{
    edma_tcd_t tcd __attribute__((aligned(32))) = {0};

    tcd.SADDR = (uint32_t)(uintptr_t)pixels;
    tcd.SOFF = 2;
    tcd.ATTR = DMA_ATTR_SSIZE(kEDMA_TransferSize2Bytes) |
               DMA_ATTR_DSIZE(kEDMA_TransferSize2Bytes);
    tcd.NBYTES = 2u;
    tcd.SLAST = 0u;
    tcd.DADDR = LPSPI_GetTxRegisterAddress(LPSPI1);
    tcd.DOFF = 0;
    tcd.CITER = DMA_CITER_ELINKNO_CITER(pixelCount);
    tcd.DLAST_SGA = 0u;
    /* Exact V2 value: major interrupt plus DREQ. MAJORLINKCH contains 16 but
     * has no effect because MAJORELINK is clear. */
    tcd.CSR = GP100_DISPLAY_TX_DMA_CSR;
    tcd.BITER = DMA_BITER_ELINKNO_BITER(pixelCount);
    EDMA_InstallTCD(DMA0, GP100_DMA_PANEL_TX_CHANNEL, &tcd);
}

static void GP100_DisplayConfigureRxDma(uint16_t pixelCount)
{
    edma_tcd_t tcd __attribute__((aligned(32))) = {0};

    tcd.SADDR = LPSPI_GetRxRegisterAddress(LPSPI1);
    tcd.SOFF = 0;
    tcd.ATTR = DMA_ATTR_SSIZE(kEDMA_TransferSize1Bytes) |
               DMA_ATTR_DSIZE(kEDMA_TransferSize1Bytes);
    tcd.NBYTES = 1u;
    tcd.SLAST = 0u;
    tcd.DADDR = (uint32_t)(uintptr_t)&s_receiveDrain;
    tcd.DOFF = 0;
    tcd.CITER = DMA_CITER_ELINKNO_CITER(pixelCount);
    tcd.DLAST_SGA = 0u;
    /* RX has the same active INTMAJOR+DREQ bits; unlike TX, its inactive
     * MAJORLINKCH field is zero in the shipped template. */
    tcd.CSR = GP100_DISPLAY_RX_DMA_CSR;
    tcd.BITER = DMA_BITER_ELINKNO_BITER(pixelCount);
    EDMA_InstallTCD(DMA0, GP100_DMA_PANEL_RX_CHANNEL, &tcd);
}

void GP100_DisplayTransportInit(void)
{
    s_initialized = false;
    s_busy = false;
    s_transferError = false;
    s_completedFrames = 0u;
    s_receiveDrain = 0u;

    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_RX_CHANNEL);
    LPSPI_DisableDMA(LPSPI1, GP100_DISPLAY_DMA_MASK);

    DMAMUX_SetSource(DMAMUX, GP100_DMA_PANEL_TX_CHANNEL,
                     kDmaRequestMuxLPSPI1Tx);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_PANEL_RX_CHANNEL,
                     kDmaRequestMuxLPSPI1Rx);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_PANEL_TX_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_PANEL_RX_CHANNEL);
    GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_TX_CHANNEL);
    GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_RX_CHANNEL);

    /* The shipped IRQ priority byte is 0x10, ARM priority 1. */
    NVIC_SetPriority(DMA2_DMA18_IRQn, 1u);
    NVIC_ClearPendingIRQ(DMA2_DMA18_IRQn);
    NVIC_EnableIRQ(DMA2_DMA18_IRQn);

    if ((g_gp100_board_state.initialized & kGP100_InitPanelSpi) != 0u)
    {
        s_initialized = true;
        g_gp100_board_state.initialized |= kGP100_InitDisplayTransport;
    }
}

status_t GP100_DisplayTransferRgb565Blocking(const uint16_t *pixels,
                                             size_t pixelCount)
{
    uint16_t iterationCount;
    uint32_t startedAt;
    bool completed;

    if ((pixels == NULL) || (pixelCount == 0u) ||
        (pixelCount > GP100_DISPLAY_DMA_ITERATION_MAX))
    {
        return kStatus_InvalidArgument;
    }
    if (!s_initialized)
    {
        return kStatus_LPSPI_Error;
    }
    if (s_busy)
    {
        return kStatus_LPSPI_Busy;
    }
    iterationCount = (uint16_t)pixelCount;

    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_RX_CHANNEL);
    LPSPI_DisableDMA(LPSPI1, GP100_DISPLAY_DMA_MASK);
    LPSPI_Enable(LPSPI1, false);
    LPSPI_FlushFifo(LPSPI1, true, true);
    LPSPI_ClearStatusFlags(LPSPI1, (uint32_t)kLPSPI_AllStatusFlag);

    GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_TX_CHANNEL);
    GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_RX_CHANNEL);
    GP100_DisplayConfigureTxDma(pixels, iterationCount);
    GP100_DisplayConfigureRxDma(iterationCount);
    SCB_CleanDCache_by_Addr((void *)(uintptr_t)pixels,
                            (int32_t)(pixelCount * sizeof(*pixels)));

    s_transferError = false;
    s_busy = true;
    __DMB();
    NVIC_ClearPendingIRQ(DMA2_DMA18_IRQn);
    LPSPI_Enable(LPSPI1, true);
    GP100_DisplayQueuePixelCommand();
    startedAt = g_gp100_board_state.milliseconds;
    LPSPI_EnableDMA(LPSPI1, GP100_DISPLAY_DMA_MASK);
    /* Preserve the factory request order: transmit 2, then receive 18. */
    EDMA_EnableChannelRequest(DMA0, GP100_DMA_PANEL_TX_CHANNEL);
    EDMA_EnableChannelRequest(DMA0, GP100_DMA_PANEL_RX_CHANNEL);

    while (s_busy && (uint32_t)(g_gp100_board_state.milliseconds - startedAt) <
                         GP100_DISPLAY_DMA_TIMEOUT_MS)
    {
        __NOP();
    }

    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_TX_CHANNEL);
    EDMA_DisableChannelRequest(DMA0, GP100_DMA_PANEL_RX_CHANNEL);
    LPSPI_DisableDMA(LPSPI1, GP100_DISPLAY_DMA_MASK);
    completed = !s_busy;
    s_busy = false;
    LPSPI_Enable(LPSPI1, false);
    if (!completed)
    {
        GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_TX_CHANNEL);
        GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_RX_CHANNEL);
        NVIC_ClearPendingIRQ(DMA2_DMA18_IRQn);
    }
    /* The SDK checks TCR before sending byte commands. Reset the peripheral
     * to restore 8-bit transfers without waiting on a queued pixel command. */
    GP100_InitPanelSpi();
    if (!completed)
    {
        return kStatus_LPSPI_Timeout;
    }
    return s_transferError ? kStatus_LPSPI_Error : kStatus_Success;
}

bool GP100_DisplayTransportIsBusy(void)
{
    return s_busy;
}

uint32_t GP100_DisplayTransportCompletedFrames(void)
{
    return s_completedFrames;
}

static void GP100_DisplayHookCleanCache(void *context,
                                       const void *address,
                                       size_t byteCount)
{
    (void)context;
    SCB_CleanDCache_by_Addr((void *)(uintptr_t)address, (int32_t)byteCount);
}

static bool GP100_DisplayHookSetWindow(void *context,
                                      uint16_t xStart,
                                      uint16_t xEnd,
                                      uint16_t yStart,
                                      uint16_t yEnd)
{
    (void)context;
    return GP100_DisplaySetWindow(xStart, yStart,
                                  (uint16_t)(xEnd - xStart + 1u),
                                  (uint16_t)(yEnd - yStart + 1u)) ==
           kStatus_Success;
}

static bool GP100_DisplayHookTransfer(void *context,
                                      const uint16_t *pixels,
                                      size_t pixelCount)
{
    (void)context;
    return GP100_DisplayTransferRgb565Blocking(pixels, pixelCount) ==
           kStatus_Success;
}

void GP100_DisplayTransportGetHooks(gp100_display_hooks_t *hooks)
{
    if (hooks != NULL)
    {
        hooks->context = NULL;
        hooks->clean_data_cache = GP100_DisplayHookCleanCache;
        hooks->set_window = GP100_DisplayHookSetWindow;
        hooks->transfer_rgb565_blocking = GP100_DisplayHookTransfer;
    }
}

void GP100_DisplayTransportGetFramebuffer(
    gp100_display_framebuffer_t *framebuffer)
{
    if (framebuffer != NULL)
    {
        framebuffer->render_pixels = s_renderPixels;
        framebuffer->transfer_pixels = s_transferPixels;
        framebuffer->dirty = false;
    }
}

void DMA2_DMA18_IRQHandler(void)
{
    uint32_t flags;

    flags = EDMA_GetChannelStatusFlags(DMA0,
                                       GP100_DMA_PANEL_TX_CHANNEL);
    if ((flags & (kEDMA_InterruptFlag | kEDMA_ErrorFlag)) != 0u)
    {
        if ((flags & kEDMA_ErrorFlag) != 0u)
        {
            s_transferError = true;
        }
        GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_TX_CHANNEL);
    }

    flags = EDMA_GetChannelStatusFlags(DMA0,
                                       GP100_DMA_PANEL_RX_CHANNEL);
    if ((flags & (kEDMA_InterruptFlag | kEDMA_ErrorFlag)) != 0u)
    {
        if ((flags & kEDMA_ErrorFlag) != 0u)
        {
            s_transferError = true;
        }
        GP100_DisplayClearDmaStatus(GP100_DMA_PANEL_RX_CHANNEL);
        if ((flags & kEDMA_InterruptFlag) != 0u)
        {
            ++s_completedFrames;
            __DMB();
            s_busy = false;
        }
    }
}
