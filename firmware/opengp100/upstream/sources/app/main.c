#include "application.h"

#include "board/board.h"
#include "board/flash_storage.h"
#include "board/peripherals.h"
#include "board/pin_mux.h"
#include "protocol/usb/usb_device_adapter.h"

#include "protocol/midi/sysex/vendor_sysex.h"

#include "fsl_gpio.h"

#define GP100_STATE_MAGIC 0x47503130u /* "GP10" */
#define GP100_FACTORY_RESET_REBOOT_DELAY_MS 250u

static bool s_factoryResetActive;
static bool s_factoryResetRebootPending;
static uint32_t s_factoryResetRebootStartedMs;

static bool GP100_ServiceFactoryReset(void)
{
    gp100_flash_factory_reset_result_t result;
    size_t completedSteps;

    if (s_factoryResetRebootPending)
    {
        if ((uint32_t)(g_gp100_board_state.milliseconds -
                       s_factoryResetRebootStartedMs) >=
            GP100_FACTORY_RESET_REBOOT_DELAY_MS)
        {
            NVIC_SystemReset();
        }
        return false;
    }
    if (!s_factoryResetActive &&
        GP100_ApplicationTakeFactoryResetRequest())
    {
        if (!GP100_ApplicationFactoryResetBegin(
                GP100_FLASH_FACTORY_RESET_STEPS) ||
            !GP100_FlashStorageFactoryResetBegin())
        {
            g_gp100_board_state.peripheral_errors |=
                kGP100_ErrorFactoryReset;
            return true;
        }
        s_factoryResetActive = true;
    }
    if (!s_factoryResetActive)
    {
        return false;
    }

    result = GP100_FlashStorageFactoryResetService(&completedSteps);
    if (result == kGP100_FlashFactoryResetProgress ||
        result == kGP100_FlashFactoryResetComplete)
    {
        if (!GP100_ApplicationFactoryResetUpdate(completedSteps))
        {
            g_gp100_board_state.peripheral_errors |=
                kGP100_ErrorFactoryReset;
        }
    }
    if (result == kGP100_FlashFactoryResetComplete)
    {
        s_factoryResetActive = false;
        s_factoryResetRebootPending = true;
        s_factoryResetRebootStartedMs =
            g_gp100_board_state.milliseconds;
    }
    else if (result == kGP100_FlashFactoryResetError ||
             result == kGP100_FlashFactoryResetIdle)
    {
        s_factoryResetActive = false;
        g_gp100_board_state.peripheral_errors |=
            kGP100_ErrorFactoryReset;
    }
    return true;
}

static uint8_t s_usbMidiResponse[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
static size_t s_usbMidiResponseSize;

static bool GP100_ServiceUsbMidiResponse(void)
{
    size_t responseSize;

    if (s_usbMidiResponseSize == 0u && GP100_USB_MidiCanWrite() &&
        GP100_ApplicationTakeMidiResponse(
            s_usbMidiResponse, sizeof(s_usbMidiResponse), &responseSize))
    {
        s_usbMidiResponseSize = responseSize;
    }
    if (s_usbMidiResponseSize != 0u && GP100_USB_MidiCanWrite() &&
        GP100_USB_MidiWrite(s_usbMidiResponse, s_usbMidiResponseSize))
    {
        s_usbMidiResponseSize = 0u;
        return true;
    }
    return false;
}

void SysTick_Handler(void)
{
    ++g_gp100_board_state.milliseconds;
}

void HardFault_Handler(void)
{
    g_gp100_board_state.fault = 1u;
    __disable_irq();
    for (;;)
    {
        __NOP();
    }
}

int main(void)
{
    g_gp100_board_state.magic = GP100_STATE_MAGIC;
    g_gp100_board_state.abi_version = 1u;

    BOARD_ConfigMPU();
    BOARD_InitBootClocks();
    BOARD_InitBootPins();
    if (BOARD_TestSdram())
    {
        BOARD_InitSdramSections();
        GP100_InitPeripherals();
    }
    else
    {
        g_gp100_board_state.fault = 3u;
    }

    if (SysTick_Config(g_gp100_board_state.core_clock_hz / 1000u) != 0u)
    {
        g_gp100_board_state.fault = 2u;
    }
    else
    {
        /* Keep the millisecond timeout clock ahead of the SAI DMA ISR. */
        NVIC_SetPriority(SysTick_IRQn, 2u);
    }

    if (g_gp100_board_state.fault == 0u)
    {
        if (!GP100_ApplicationInitialize())
        {
            g_gp100_board_state.fault = 4u;
        }
        else if (!GP100_ApplicationSelectStartupMode(GPIO1->DR))
        {
            g_gp100_board_state.fault = 4u;
        }
        else
        {
            gp100_flash_update_hooks_t flashHooks;

            if (GP100_FlashStorageInitialize(&flashHooks))
            {
                GP100_ApplicationSetFlashHooks(&flashHooks);
                g_gp100_board_state.initialized |= kGP100_InitFlashStorage;
            }
            else
            {
                g_gp100_board_state.peripheral_errors |=
                    kGP100_ErrorFlashStorage;
            }
            /* Let the expression input settle while the startup screen runs. */
            if (!GP100_ApplicationStartAnalog())
            {
                g_gp100_board_state.peripheral_errors |= kGP100_ErrorAnalogStart;
            }
#if defined(GP100_AUTOSTART_TUNER_CAPTURE)
            /* Factory startup leaves the GPIO1_IO21/QTMR3 interval producer
             * running continuously; Tuner mode gates only reduction/UI use. */
            if (!GP100_ApplicationStartTunerCapture())
            {
                g_gp100_board_state.peripheral_errors |=
                    kGP100_ErrorTunerStart;
            }
#endif
            if (!GP100_ApplicationInitializeDisplay())
            {
                g_gp100_board_state.peripheral_errors |=
                    kGP100_ErrorDisplayInit;
            }
#if defined(GP100_AUTOSTART_AUDIO)
            /* The first application frame has reached the panel. Start the
             * codec and transport with the current expression level. */
            else if (!GP100_ApplicationStartAudio())
            {
                g_gp100_board_state.peripheral_errors |= kGP100_ErrorAudioStart;
            }
#endif
            if (!GP100_ApplicationStartControls())
            {
                g_gp100_board_state.fault = 5u;
            }
            if (!GP100_ApplicationStartMidi())
            {
                g_gp100_board_state.peripheral_errors |=
                    kGP100_ErrorMidiStart;
            }
            if (!GP100_USB_DeviceInitialize())
            {
                g_gp100_board_state.peripheral_errors |=
                    kGP100_ErrorUsbStart;
            }
        }
    }

    for (;;)
    {
        g_gp100_board_state.gpio1_snapshot = GPIO1->DR;
        g_gp100_board_state.gpio3_snapshot = GPIO3->DR;
        bool serviced = GP100_ServiceFactoryReset();
        if (!s_factoryResetActive && !s_factoryResetRebootPending)
        {
            serviced = GP100_ApplicationService() || serviced;
        }
        serviced = GP100_ServiceUsbMidiResponse() || serviced;
        if (serviced)
        {
            continue;
        }
        __WFI();
    }
}
