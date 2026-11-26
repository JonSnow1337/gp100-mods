#include "peripherals.h"

#include "board/input/analog_acquisition.h"
#include "audio/transport/audio_transport.h"
#include "board.h"
#include "display/display_transport.h"
#include "rhythm/rhythm_clock.h"
#include "board/input/control_input.h"
#include "board/input/tuner_capture.h"
#include "fsl_adc.h"
#include "fsl_adc_etc.h"
#include "fsl_clock.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "fsl_lpspi.h"
#include "fsl_lpuart.h"
#include "fsl_pit.h"
#include "fsl_qtmr.h"
#include "fsl_sai.h"
#include "fsl_xbara.h"

typedef struct
{
    GPIO_Type *base;
    uint32_t pin;
} gp100_board_gpio_pin_t;

/* Four recovered quadrature pairs: Quick 1, Quick 2, Quick 3, and PARA. */
static const gp100_board_gpio_pin_t s_encoderInputs[] = {
    {GPIO3, 27u},
    {GPIO3, 26u}, {GPIO1, 24u}, {GPIO1, 27u},
    {GPIO1, 11u}, {GPIO1, 9u},  {GPIO1, 26u}, {GPIO1, 0u},
};

/* Active-low front-panel controls in factory scanner order. */
static const gp100_board_gpio_pin_t s_buttonInputs[] = {
    {GPIO1, 22u}, {GPIO1, 10u}, {GPIO1, 23u}, {GPIO1, 15u},
    {GPIO1, 16u}, {GPIO1, 19u}, {GPIO3, 16u}, {GPIO1, 18u},
};

static const gp100_board_gpio_pin_t s_otherInputs[] = {
    /* ADC1_IN9 hardware-test TUNER, ADC1_IN6 expression pedal, tuner edge,
     * and the factory's mux-only reserved GPIO2 pad. */
    {GPIO1, 20u}, {GPIO1, 17u}, {GPIO1, 21u}, {GPIO2, 31u},
};

static void GP100_InitGpio(void)
{
    const gpio_pin_config_t input = {
        .direction = kGPIO_DigitalInput,
        .outputLogic = 0u,
        .interruptMode = kGPIO_NoIntmode,
    };
    const gpio_pin_config_t outputLow = {
        .direction = kGPIO_DigitalOutput,
        .outputLogic = 0u,
        .interruptMode = kGPIO_NoIntmode,
    };
    const gpio_pin_config_t outputHigh = {
        .direction = kGPIO_DigitalOutput,
        .outputLogic = 1u,
        .interruptMode = kGPIO_NoIntmode,
    };

    CLOCK_EnableClock(kCLOCK_Gpio1);
    CLOCK_EnableClock(kCLOCK_Gpio2);
    CLOCK_EnableClock(kCLOCK_Gpio3);

    for (uint32_t i = 0u; i < ARRAY_SIZE(s_encoderInputs); ++i)
    {
        GPIO_PinInit(s_encoderInputs[i].base, s_encoderInputs[i].pin, &input);
    }
    for (uint32_t i = 0u; i < ARRAY_SIZE(s_buttonInputs); ++i)
    {
        GPIO_PinInit(s_buttonInputs[i].base, s_buttonInputs[i].pin, &input);
    }
    for (uint32_t i = 0u; i < ARRAY_SIZE(s_otherInputs); ++i)
    {
        GPIO_PinInit(s_otherInputs[i].base, s_otherInputs[i].pin, &input);
    }

    /* Recovered panel-control idle levels:
     *   GPIO3_17 = RESET, deasserted high
     *   GPIO3_15 = D/C, data state high (the command helper drives it low)
     *   GPIO1_3  = backlight or panel-enable, off low
     * V1 pulses RESET high-low-high (1 ms, 1 ms, then 120 ms) before sending
     * the ST7735-compatible command sequence. */
    GPIO_PinInit(GPIO3, 17u, &outputHigh);
    GPIO_PinInit(GPIO3, 15u, &outputHigh);
    GPIO_PinInit(GPIO1, 3u, &outputLow);

    g_gp100_board_state.initialized |= kGP100_InitGpio;
}

static void GP100_InitDma(void)
{
    edma_config_t dmaConfig;

    EDMA_GetDefaultConfig(&dmaConfig);
    EDMA_Init(DMA0, &dmaConfig);
    DMAMUX_Init(DMAMUX);

    /* V2 init_9 assigns channel 0 to ADC_ETC. Its trigger/channel selection is
     * deliberately left disabled below, so enabling this route cannot start a
     * conversion by itself. */
    DMAMUX_SetSource(DMAMUX, GP100_DMA_ADC_CHANNEL, kDmaRequestMuxADC_ETC);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_PANEL_TX_CHANNEL, kDmaRequestMuxLPSPI1Tx);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_PANEL_RX_CHANNEL, kDmaRequestMuxLPSPI1Rx);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_SAI_RX_CHANNEL, kDmaRequestMuxSai1Rx);
    DMAMUX_SetSource(DMAMUX, GP100_DMA_SAI_TX_CHANNEL, kDmaRequestMuxSai1Tx);

    /* Routing can be enabled now; no request is generated until the associated
     * peripheral and its DMA request bit are explicitly started by user code. */
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_ADC_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_PANEL_TX_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_PANEL_RX_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_SAI_RX_CHANNEL);
    DMAMUX_EnableChannel(DMAMUX, GP100_DMA_SAI_TX_CHANNEL);

    g_gp100_board_state.initialized |= kGP100_InitDma;
}

static void GP100_InitAdc(void)
{
    adc_config_t adcConfig;

    ADC_GetDefaultConfig(&adcConfig);
    ADC_Init(ADC1, &adcConfig);
    if (ADC_DoAutoCalibration(ADC1) != kStatus_Success)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorAdcCalibration;
    }

    XBARA_Init(XBARA1);

    /* V2 uses PIT0 at 10 kHz -> XBARA1 -> ADC_ETC trigger 0 -> DMA0 channel 0.
     * Its four ADC1 slots are IN9, IN6, IN6, IN6. IN6/GPIO_AD_B1_01 is the
     * built-in expression pedal. Hardware Test displays raw IN9/GPIO_AD_B1_04
     * as TUNER; the external analogue source remains unknown. The base keeps
     * the recovered acquisition stopped until explicitly requested. */
    g_gp100_board_state.initialized |= kGP100_InitAdc;
}

static void GP100_InitControlI2c(void)
{
    lpi2c_master_config_t config;
    const uint32_t clockHz = CLOCK_GetClockRootFreq(kCLOCK_Lpi2cClkRoot);

    if (clockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorI2cClock;
        return;
    }

    /* The LPI2C1 SCL/SDA mux is observed in V2, but factory startup does not
     * initialize the peripheral or issue a transfer. Board photographs show
     * ES7243/ES8156 converters, but their connection to this bus is unproven.
     * Callable experimental drivers are in drivers/es7243.h and es8156.h.
     * The 100 kHz rate is a safe base policy, not a recovered rate; this sends
     * no transfer. */
    LPI2C_MasterGetDefaultConfig(&config);
    config.baudRate_Hz = GP100_CONTROL_I2C_BAUD_HZ;
    LPI2C_MasterInit(LPI2C1, &config, clockHz);
    g_gp100_board_state.initialized |= kGP100_InitI2c;
}

static void GP100_InitMidiUart(void)
{
    lpuart_config_t config;
    const uint32_t clockHz = CLOCK_GetClockRootFreq(kCLOCK_UartClkRoot);

    if (clockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorMidiClock;
        return;
    }

    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = GP100_MIDI_BAUD_HZ;
    config.enableTx = false;
    config.enableRx = true;
    if (LPUART_Init(LPUART1, &config, clockHz) != kStatus_Success)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorMidiInit;
        return;
    }
    g_gp100_board_state.initialized |= kGP100_InitMidiUart;
}

void GP100_InitPanelSpi(void)
{
    lpspi_master_config_t config;
    const uint32_t clockHz = CLOCK_GetClockRootFreq(kCLOCK_LpspiClkRoot);

    if (clockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorSpiClock;
        return;
    }

    LPSPI_MasterGetDefaultConfig(&config);
    /* V2's 28-byte lpspi_master_config_t is recovered field-for-field:
     * 10 MHz, 8-bit, Mode 3, MSB first, 60/40/50 ns delays, PCS0 active low,
     * and SDO used for both the input/output pin selection. There is no panel
     * MISO pad, but RX DMA is used to drain the full-duplex receive side.
     *
     * Panel protocol recovered from V1 and V2:
     *   - 128x160, RGB565 (COLMOD 0x3A=0x55), MADCTL 0x36=0xC8
     *   - ST7735-family command set; full window is X=0..127, Y=0..159
     *   - GPIO3_15 is D/C, GPIO3_17 RESET, GPIO1_3 backlight/enable
     *   - V2 uses hardware PCS0 on GPIO_SD_B0_01; V1 muxes the same pad as
     *     GPIO3_13 and controls CS manually.
     * Recovered controller sequence (command followed by data bytes):
     *   11; B1:05 3A 3A; B2:05 3A 3A; B3:05 3A 3A 05 3A 3A; B4:03;
     *   C0:62 02 04; C1:C0; C2:0D 00; C3:8D 6A; C4:8D EE; C5:12;
     *   E0:03 1B 12 11 3F 3A 32 34 2F 2B 30 3A 00 01 02 05;
     *   E1:03 1B 12 11 32 2F 2A 2F 2E 2C 35 3F 00 00 01 05;
     *   36:C8; 3A:55; 2A:0000..007F; 2B:0000..009F; 2C; 29.
     * SLPOUT 11 is followed by 120 ms; DISPON 29 is followed by 1000 ms in
     * V2. RGB565 pixel words are sent high byte first.
     * drivers/gp100_display.h implements this exact sequence plus window,
     * RGB565, and fill operations. This routine configures only the bus; base
     * startup intentionally does not reset, illuminate, or command the panel. */
    config.baudRate = GP100_PANEL_SPI_BAUD_HZ;
    config.bitsPerFrame = 8u;
    config.cpol = kLPSPI_ClockPolarityActiveLow;
    config.cpha = kLPSPI_ClockPhaseSecondEdge;
    config.direction = kLPSPI_MsbFirst;
    config.pcsToSckDelayInNanoSec = 60u;
    config.lastSckToPcsDelayInNanoSec = 40u;
    config.betweenTransferDelayInNanoSec = 50u;
    config.whichPcs = kLPSPI_Pcs0;
    config.pcsActiveHighOrLow = kLPSPI_PcsActiveLow;
    config.pinCfg = kLPSPI_SdoInSdoOut;
    config.dataOutConfig = kLpspiDataOutRetained;
    CLOCK_EnableClock(kCLOCK_Lpspi1);
    LPSPI_Reset(LPSPI1);
    LPSPI_MasterInit(LPSPI1, &config, clockHz);

    g_gp100_board_state.initialized |= kGP100_InitPanelSpi;
}

static void GP100_InitSai(void)
{
    sai_config_t txConfig;
    sai_config_t rxConfig;
    sai_transfer_format_t format = {0};
    const uint32_t clockHz = CLOCK_GetClockRootFreq(kCLOCK_Sai1ClkRoot);

    if (clockHz == 0u)
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorSaiClock;
        return;
    }

    SAI_TxGetDefaultConfig(&txConfig);
    SAI_RxGetDefaultConfig(&rxConfig);
    /* Factory audio_init builds {protocol=I2S, async, 0, bclkSource=2,
     * slave} for TX, then changes only RX to synchronous. BCLK source is
     * ignored in slave mode. Do not inherit the SDK's master defaults. */
    txConfig.masterSlave = kSAI_Slave;
    rxConfig.masterSlave = kSAI_Slave;
    SAI_TxInit(SAI1, &txConfig);
    SAI_RxInit(SAI1, &rxConfig);

    format.sampleRate_Hz = GP100_AUDIO_SAMPLE_RATE_HZ;
    format.bitWidth = GP100_AUDIO_SLOT_WIDTH;
    format.stereo = kSAI_Stereo;
    format.channel = 0u;
    format.channelMask = kSAI_Channel0Mask;
    format.protocol = kSAI_BusI2S;
    format.isFrameSyncCompact = false;
    SAI_TxSetFormat(SAI1, &format, clockHz, clockHz);
    SAI_RxSetFormat(SAI1, &format, clockHz, clockHz);

    /* The photographed converter pair is probably an Everest ES7243 ADC on RX
     * and ES8156 DAC on TX. That identification comes from package markings,
     * not a completed net trace. Factory SAI1 is a clock slave, so an external
     * source must supply BCLK/frame sync before this path can run. Their
     * drivers are compiled into the base but
     * no converter-control registers are written here; TX/RX and their DMA
     * request bits intentionally remain disabled. The ES7243 driver documents
     * its requirement that MCLK be running before codec configuration. */
    g_gp100_board_state.initialized |= kGP100_InitSai;
}

static void GP100_InitTimers(void)
{
    pit_config_t pitConfig;
    qtmr_config_t qtmrConfig;

    PIT_GetDefaultConfig(&pitConfig);
    PIT_Init(PIT, &pitConfig);
    QTMR_GetDefaultConfig(&qtmrConfig);
    QTMR_Init(TMR2, kQTMR_Channel_0, &qtmrConfig);
    QTMR_Init(TMR3, kQTMR_Channel_0, &qtmrConfig);
    QTMR_Init(TMR3, kQTMR_Channel_1, &qtmrConfig);
    QTMR_Init(TMR4, kQTMR_Channel_0, &qtmrConfig);

    /* V2 actively uses QTMR2 channel 0, QTMR3 channels 0/1, and QTMR4 channel
     * 0 and supplies handlers for TMR2/3/4. All remain stopped here. SysTick
     * supplies the base project's 1 ms heartbeat; PIT is also left stopped. */
    g_gp100_board_state.initialized |= kGP100_InitTimers;
}

void GP100_InitPeripherals(void)
{
    /* V2 peripheral audit:
     *   initialized here, inactive: GPIO1/2/3, DMA0+DMAMUX, ADC1+ADC_ETC,
     *     XBARA1, LPI2C1, LPUART1 RX, LPSPI1, SAI1, PIT, QTMR2/3/4;
     *   established before main: FlexSPI NOR, SEMC SDRAM, clocks, watchdog
     *     disable, MPU/cache, and later the SysTick heartbeat;
     *   intentionally omitted: USB1/USBPHY1 (requires the chosen USB Audio/
     *     MIDI descriptors and middleware) and active LCDIF (pins/IRQ exist,
     *     but no live V2 LCDIF initialization path has been established).
     * Other SDK peripheral base-address literals in V2 (LPUART2..8,
     * LPSPI2..4, LPI2C2..3, SAI2) occur in generic instance tables and are
     * not evidence that those peripherals are connected or initialized. */
    GP100_InitGpio();
    GP100_InitDma();
    GP100_InitAdc();
    GP100_InitControlI2c();
    GP100_InitMidiUart();
    GP100_InitPanelSpi();
    GP100_InitSai();
    GP100_InitTimers();
    GP100_DisplayTransportInit();
    GP100_AnalogAcquisitionInit();
    GP100_AudioTransportInit();
    GP100_RhythmClockInit();
    GP100_ControlInputInit();
    GP100_TunerCaptureInit();
}
