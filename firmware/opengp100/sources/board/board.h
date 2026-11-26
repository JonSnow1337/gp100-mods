#ifndef GP100_BOARD_H
#define GP100_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "fsl_common.h"

#define BOARD_NAME "Valeton GP-100"
#ifndef BOARD_FLASH_SIZE
#define BOARD_FLASH_SIZE 0x00800000u
#endif
#define BOARD_SDRAM_BASE 0x80000000u
#define BOARD_SDRAM_SIZE 0x02000000u
#define BOARD_XTAL0_CLK_HZ 24000000u
#define BOARD_XTAL32K_CLK_HZ 32768u

#if defined(__GNUC__)
#define GP100_SDRAM_BSS_SECTION_ALIGN(var, alignment) \
    var __attribute__((section(".sdram_bss"), aligned(alignment)))
#define GP100_SDRAM_NOINIT_SECTION_ALIGN(var, alignment) \
    var __attribute__((section(".sdram_noinit"), aligned(alignment)))
#else
#error "Define GP100 SDRAM section attributes for this compiler"
#endif

enum
{
    kGP100_InitClocks      = 1u << 0,
    kGP100_InitPins        = 1u << 1,
    kGP100_InitGpio        = 1u << 2,
    kGP100_InitDma         = 1u << 3,
    kGP100_InitAdc         = 1u << 4,
    kGP100_InitI2c         = 1u << 5,
    kGP100_InitMidiUart    = 1u << 6,
    kGP100_InitPanelSpi    = 1u << 7,
    kGP100_InitSai         = 1u << 8,
    kGP100_InitTimers      = 1u << 9,
    kGP100_InitSdramTested = 1u << 10,
    kGP100_InitAnalogAcquisition = 1u << 11,
    kGP100_InitControlInput = 1u << 12,
    kGP100_InitTunerCapture = 1u << 13,
    kGP100_InitAudioTransport = 1u << 14,
    kGP100_InitRhythmClock = 1u << 15,
    kGP100_InitDisplayTransport = 1u << 16,
    kGP100_InitApplication = 1u << 17,
    kGP100_InitUsbDevice = 1u << 18,
    kGP100_InitFlashStorage = 1u << 19,
};

typedef struct
{
    uint32_t magic;
    uint32_t abi_version;
    uint32_t initialized;
    uint32_t core_clock_hz;
    uint32_t ahb_clock_hz;
    uint32_t ipg_clock_hz;
    uint32_t semc_clock_hz;
    uint32_t sai1_clock_hz;
    uint32_t lpspi_clock_hz;
    uint32_t gpio1_snapshot;
    uint32_t gpio3_snapshot;
    uint32_t milliseconds;
    uint32_t peripheral_errors;
    uint32_t fault;
} gp100_board_state_t;

extern volatile gp100_board_state_t g_gp100_board_state;

void BOARD_ConfigMPU(void);
void BOARD_InitBootClocks(void);
void BOARD_InitBootPins(void);
bool BOARD_TestSdram(void);
void BOARD_InitSdramSections(void);

#endif
