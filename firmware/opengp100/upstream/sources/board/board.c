#include "board.h"

#include "fsl_clock.h"
#include "peripherals.h"

#include <string.h>

volatile gp100_board_state_t g_gp100_board_state;

extern uint8_t __sdram_code_start__[];
extern uint8_t __sdram_code_end__[];
extern const uint8_t __sdram_code_load__[];
extern uint8_t __sdram_bss_start__[];
extern uint8_t __sdram_bss_end__[];

#if defined(GP100_AUTOSTART_AUDIO)
static bool BOARD_WaitForClockFlag(volatile const uint32_t *reg, uint32_t mask)
{
    for (uint32_t count = 0u; count < 1000000u; ++count)
    {
        if ((*reg & mask) != 0u)
        {
            return true;
        }
    }
    return false;
}

static bool BOARD_InitAudioClockRoots(void)
{
    /* Keep the ROM's independent SEMC root and the active FlexSPI clock.
     * The factory bootloader runs the Cortex-M7 at 600 MHz with IPG at 150 MHz. */
    if ((CCM->CBCDR & CCM_CBCDR_SEMC_CLK_SEL_MASK) == 0u)
    {
        return false;
    }
    if (CLOCK_GetFreq(kCLOCK_CpuClk) != 600000000u)
    {
        DCDC->REG3 = (DCDC->REG3 & ~DCDC_REG3_TRG_MASK) | DCDC_REG3_TRG(0x12u);
        if (!BOARD_WaitForClockFlag(&DCDC->REG0, DCDC_REG0_STS_DC_OK_MASK))
        {
            return false;
        }
        /* Use the oscillator for PERIPH while changing its PLL source. */
        CLOCK_SetMux(kCLOCK_PeriphClk2Mux, 1u);
        CLOCK_SetMux(kCLOCK_PeriphMux, 1u);
        CLOCK_SetDiv(kCLOCK_IpgDiv, kCLOCK_IpgDivBy4);
        CLOCK_SetDiv(kCLOCK_ArmDiv, 1u);
        CCM_ANALOG->PLL_ARM = CCM_ANALOG_PLL_ARM_ENABLE_MASK |
                             CCM_ANALOG_PLL_ARM_DIV_SELECT(100u);
        if (!BOARD_WaitForClockFlag(&CCM_ANALOG->PLL_ARM,
                                    CCM_ANALOG_PLL_ARM_LOCK_MASK))
        {
            return false;
        }
        CLOCK_SetMux(kCLOCK_PrePeriphMux, 3u);
        CLOCK_SetMux(kCLOCK_PeriphMux, 0u);
        CLOCK_SetDiv(kCLOCK_AhbDiv, kCLOCK_AhbDivBy1);
    }

    /* V2.1.0 audio_init at 0x3c90 selects PLL4 / 64 for the SAI1 root.
     * SAI still receives BCLK, frame sync and MCLK from the board. */
    CCM_ANALOG->PLL_AUDIO_NUM = CCM_ANALOG_PLL_AUDIO_NUM_A(77u);
    CCM_ANALOG->PLL_AUDIO_DENOM = CCM_ANALOG_PLL_AUDIO_DENOM_B(100u);
    CCM_ANALOG->MISC2 &= ~(CCM_ANALOG_MISC2_AUDIO_DIV_LSB_MASK |
                          CCM_ANALOG_MISC2_AUDIO_DIV_MSB_MASK);
    CCM_ANALOG->PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_ENABLE_MASK |
                           CCM_ANALOG_PLL_AUDIO_DIV_SELECT(32u) |
                           CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2u);
    if (!BOARD_WaitForClockFlag(&CCM_ANALOG->PLL_AUDIO,
                                CCM_ANALOG_PLL_AUDIO_LOCK_MASK))
    {
        return false;
    }
    CLOCK_SetMux(kCLOCK_Sai1Mux, 2u);
    CLOCK_SetDiv(kCLOCK_Sai1PreDiv, kCLOCK_Sai1PreDivBy1);
    CLOCK_SetDiv(kCLOCK_Sai1Div, 63u);
    return true;
}
#endif

void BOARD_ConfigMPU(void)
{
    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0u)
    {
        SCB_DisableICache();
    }
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0u)
    {
        SCB_DisableDCache();
    }

    ARM_MPU_Disable();

    /* Deny speculative access first, then open only the regions this board uses. */
    MPU->RBAR = ARM_MPU_RBAR(0, 0x00000000u);
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_NONE, 0, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_4GB);

    MPU->RBAR = ARM_MPU_RBAR(1, 0x00000000u);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_128KB);

    MPU->RBAR = ARM_MPU_RBAR(2, 0x20000000u);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_128KB);

    /* The linker reserves OCRAM for .ncache and .ncache.init. CPU reads
     * and writes must see the same storage as the DMA controllers. */
    MPU->RBAR = ARM_MPU_RBAR(3, 0x20200000u);
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 1, 1, 0, 0, 0, ARM_MPU_REGION_SIZE_256KB);

    MPU->RBAR = ARM_MPU_RBAR(4, 0x40000000u);
    MPU->RASR = ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 2, 0, 0, 0, 0, ARM_MPU_REGION_SIZE_4MB);

    MPU->RBAR = ARM_MPU_RBAR(5, 0x60000000u);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_RO, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_8MB);

    MPU->RBAR = ARM_MPU_RBAR(6, BOARD_SDRAM_BASE);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 0, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_32MB);

    /* The FlexSPI driver reads its API table and runs code in the 96 KiB
     * on-chip ROM. Disable the last two 16 KiB subregions above the ROM. */
    MPU->RBAR = ARM_MPU_RBAR(7, 0x00200000u);
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_RO, 0, 0, 1, 0, 0xc0u,
                            ARM_MPU_REGION_SIZE_128KB);

    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
    SCB_EnableDCache();
    SCB_EnableICache();
}

void BOARD_InitBootClocks(void)
{
    /* Keep the DCD clock roots. Leave the system in Run mode when the core
     * idles so SysTick and the peripheral clocks can wake the application. */
    CLOCK_SetMode(kCLOCK_ModeRun);
    CLOCK_SetXtalFreq(BOARD_XTAL0_CLK_HZ);
    CLOCK_SetRtcXtalFreq(BOARD_XTAL32K_CLK_HZ);
#if defined(GP100_AUTOSTART_AUDIO)
    if (!BOARD_InitAudioClockRoots())
    {
        g_gp100_board_state.peripheral_errors |= kGP100_ErrorAudioClock;
    }
#endif
    SystemCoreClockUpdate();

    g_gp100_board_state.core_clock_hz = CLOCK_GetFreq(kCLOCK_CpuClk);
    g_gp100_board_state.ahb_clock_hz = CLOCK_GetFreq(kCLOCK_AhbClk);
    g_gp100_board_state.ipg_clock_hz = CLOCK_GetFreq(kCLOCK_IpgClk);
    g_gp100_board_state.semc_clock_hz = CLOCK_GetFreq(kCLOCK_SemcClk);
    g_gp100_board_state.sai1_clock_hz = CLOCK_GetClockRootFreq(kCLOCK_Sai1ClkRoot);
    g_gp100_board_state.lpspi_clock_hz = CLOCK_GetClockRootFreq(kCLOCK_LpspiClkRoot);
    g_gp100_board_state.initialized |= kGP100_InitClocks;
}

bool BOARD_TestSdram(void)
{
    static const uint32_t patterns[] = {
        0x00000000u, 0xffffffffu, 0xaaaaaaaaU, 0x55555555u, 0xa5a55a5au,
    };
    volatile uint32_t *const probes[] = {
        (volatile uint32_t *)(BOARD_SDRAM_BASE + 0x00000000u),
        (volatile uint32_t *)(BOARD_SDRAM_BASE + 0x00001000u),
        (volatile uint32_t *)(BOARD_SDRAM_BASE + 0x01000000u),
        (volatile uint32_t *)(BOARD_SDRAM_BASE + BOARD_SDRAM_SIZE - 4u),
    };
    uint32_t saved[sizeof(probes) / sizeof(probes[0])];

    for (uint32_t i = 0; i < (uint32_t)(sizeof(probes) / sizeof(probes[0])); ++i)
    {
        saved[i] = *probes[i];
    }

    bool ok = true;
    for (uint32_t p = 0; p < (uint32_t)(sizeof(patterns) / sizeof(patterns[0])); ++p)
    {
        for (uint32_t i = 0; i < (uint32_t)(sizeof(probes) / sizeof(probes[0])); ++i)
        {
            *probes[i] = patterns[p] ^ (uint32_t)(uintptr_t)probes[i];
        }
        __DSB();
        SCB_CleanInvalidateDCache();
        for (uint32_t i = 0; i < (uint32_t)(sizeof(probes) / sizeof(probes[0])); ++i)
        {
            if (*probes[i] != (patterns[p] ^ (uint32_t)(uintptr_t)probes[i]))
            {
                ok = false;
            }
        }
    }

    for (uint32_t i = 0; i < (uint32_t)(sizeof(probes) / sizeof(probes[0])); ++i)
    {
        *probes[i] = saved[i];
    }
    __DSB();
    SCB_CleanDCache();

    if (ok)
    {
        g_gp100_board_state.initialized |= kGP100_InitSdramTested;
    }
    return ok;
}

void BOARD_InitSdramSections(void)
{
    const size_t codeSize =
        (uintptr_t)__sdram_code_end__ - (uintptr_t)__sdram_code_start__;
    uint8_t *cursor = __sdram_bss_start__;

    if (codeSize != 0u)
    {
        memcpy(__sdram_code_start__, __sdram_code_load__, codeSize);
        SCB_CleanDCache_by_Addr(__sdram_code_start__, (int32_t)codeSize);
        SCB_InvalidateICache();
    }
    while (cursor < __sdram_bss_end__)
    {
        *cursor++ = 0u;
    }
    __DSB();
}
