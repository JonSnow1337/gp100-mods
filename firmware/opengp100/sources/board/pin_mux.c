#include "pin_mux.h"

#include "board.h"
#include "fsl_iomuxc.h"

#define PAD_GPIO      0x000010b0u
#define PAD_GPIO_IN   0x0001b040u
#define PAD_LCD       0x0001b0b0u
#define PAD_I2C       0x0000d8b0u

#define MUX_PAD(signal, sion, pad)       \
    do                                   \
    {                                    \
        IOMUXC_SetPinMux(signal, sion);   \
        IOMUXC_SetPinConfig(signal, pad); \
    } while (0)

static void BOARD_InitLcdPins(void)
{
    /* V2 writes ALT0 to these 28 consecutive mux registers and 0x1b0b0 to
     * their pad controls: CLK, ENABLE, HSYNC, VSYNC and RGB888 data. The linked
     * LCDIF IRQ handler contains the image's sole LCDIF-base literal; there is
     * no initialized-SDRAM occurrence and no active peripheral setup. The
     * proven 128x160 framebuffer path uses LPSPI1. Preserve the factory mux,
     * but do not enable LCDIF clocks, interrupts, or pixel output. */
    MUX_PAD(IOMUXC_GPIO_B0_00_LCD_CLK, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_01_LCD_ENABLE, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_02_LCD_HSYNC, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_03_LCD_VSYNC, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_04_LCD_DATA00, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_05_LCD_DATA01, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_06_LCD_DATA02, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_07_LCD_DATA03, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_08_LCD_DATA04, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_09_LCD_DATA05, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_10_LCD_DATA06, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_11_LCD_DATA07, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_12_LCD_DATA08, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_13_LCD_DATA09, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_14_LCD_DATA10, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B0_15_LCD_DATA11, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_00_LCD_DATA12, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_01_LCD_DATA13, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_02_LCD_DATA14, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_03_LCD_DATA15, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_04_LCD_DATA16, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_05_LCD_DATA17, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_06_LCD_DATA18, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_07_LCD_DATA19, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_08_LCD_DATA20, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_09_LCD_DATA21, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_10_LCD_DATA22, 0u, PAD_LCD);
    MUX_PAD(IOMUXC_GPIO_B1_11_LCD_DATA23, 0u, PAD_LCD);

    /* Factory muxes this pad to GPIO2_IO31 but never references GPIO2. Keep
     * the mux-only reserved pad in its safe input state. */
    MUX_PAD(IOMUXC_GPIO_B1_15_GPIO2_IO31, 0u, PAD_GPIO);
}

static void BOARD_InitControlPins(void)
{
    /* Polled input group recovered from board_init_5 (0x00007960). */
    MUX_PAD(IOMUXC_GPIO_EMC_41_GPIO3_IO27, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_EMC_40_GPIO3_IO26, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B1_08_GPIO1_IO24, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B1_11_GPIO1_IO27, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B0_11_GPIO1_IO11, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B0_09_GPIO1_IO09, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B1_10_GPIO1_IO26, 0u, PAD_GPIO_IN);
    MUX_PAD(IOMUXC_GPIO_AD_B0_00_GPIO1_IO00, 0u, PAD_GPIO_IN);

    /* Second GPIO group recovered from 0x00015a0c. Board roles remain open. */
    MUX_PAD(IOMUXC_GPIO_AD_B1_06_GPIO1_IO22, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B0_10_GPIO1_IO10, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_07_GPIO1_IO23, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B0_15_GPIO1_IO15, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_00_GPIO1_IO16, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_03_GPIO1_IO19, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_SD_B0_04_GPIO3_IO16, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_02_GPIO1_IO18, 0u, PAD_GPIO);

    MUX_PAD(IOMUXC_GPIO_AD_B1_04_GPIO1_IO20, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_01_GPIO1_IO17, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_05_GPIO1_IO21, 0u, PAD_GPIO);
    /* AD_B1_15 is briefly configured as GPIO1_IO31 in one vendor helper, but
     * the final audio setup remuxes it to SAI1_TX_SYNC. Keep the final role. */
}

static void BOARD_InitPanelControlPins(void)
{
    /* Panel controls recovered from both applications: RESET, D/C, and
     * backlight/panel-enable respectively. */
    MUX_PAD(IOMUXC_GPIO_SD_B0_05_GPIO3_IO17, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_SD_B0_03_GPIO3_IO15, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B0_03_GPIO1_IO03, 0u, PAD_GPIO);
}

static void BOARD_InitSerialPins(void)
{
    /* V2 muxes these LPI2C1 pads and the UART receive pad. Its converter
     * startup uses the separate GPIO bus configured by codec_startup.c. */
    MUX_PAD(IOMUXC_GPIO_SD_B1_04_LPI2C1_SCL, 1u, PAD_I2C);
    MUX_PAD(IOMUXC_GPIO_SD_B1_05_LPI2C1_SDA, 1u, PAD_I2C);
    MUX_PAD(IOMUXC_GPIO_AD_B0_13_LPUART1_RXD, 0u, PAD_GPIO);

    /* V2 panel SPI bus: clock, hardware PCS0 and MOSI/SDO. V1 instead muxes
     * SD_B0_01 as GPIO3_IO13 for manual active-low CS. No SDI pad is used. */
    MUX_PAD(IOMUXC_GPIO_SD_B0_00_LPSPI1_SCK, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_SD_B0_01_LPSPI1_PCS0, 0u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_SD_B0_02_LPSPI1_SDO, 0u, PAD_GPIO);
}

static void BOARD_InitAudioPins(void)
{
    /* All five writes use ALT3 | SION in the shipped application. */
    MUX_PAD(IOMUXC_GPIO_AD_B1_09_SAI1_MCLK, 1u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_12_SAI1_RX_DATA00, 1u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_13_SAI1_TX_DATA00, 1u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_14_SAI1_TX_BCLK, 1u, PAD_GPIO);
    MUX_PAD(IOMUXC_GPIO_AD_B1_15_SAI1_TX_SYNC, 1u, PAD_GPIO);
}

void BOARD_InitBootPins(void)
{
    CLOCK_EnableClock(kCLOCK_Iomuxc);
    BOARD_InitLcdPins();
    BOARD_InitControlPins();
    BOARD_InitPanelControlPins();
    BOARD_InitSerialPins();
    BOARD_InitAudioPins();
    g_gp100_board_state.initialized |= kGP100_InitPins;
}
