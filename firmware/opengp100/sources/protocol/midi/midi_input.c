#include "midi_input.h"

#include "board/board.h"

#include "fsl_lpuart.h"

#define GP100_MIDI_INPUT_MASK (GP100_MIDI_INPUT_CAPACITY - 1u)

#if (GP100_MIDI_INPUT_CAPACITY & GP100_MIDI_INPUT_MASK) != 0
#error "GP100_MIDI_INPUT_CAPACITY must be a power of two"
#endif

static uint8_t s_bytes[GP100_MIDI_INPUT_CAPACITY];
static volatile uint16_t s_readIndex;
static volatile uint16_t s_writeIndex;
static volatile uint32_t s_received;
static volatile uint32_t s_dropped;
static volatile uint32_t s_errors;
static volatile bool s_running;

bool GP100_MidiInputStart(void)
{
    if ((g_gp100_board_state.initialized & kGP100_InitMidiUart) == 0u)
    {
        return false;
    }
    if (s_running)
    {
        return true;
    }
    s_readIndex = 0u;
    s_writeIndex = 0u;
    s_received = 0u;
    s_dropped = 0u;
    s_errors = 0u;
    (void)LPUART_ClearStatusFlags(LPUART1, kLPUART_AllClearFlags);
    NVIC_SetPriority(LPUART1_IRQn, 4u);
    NVIC_ClearPendingIRQ(LPUART1_IRQn);
    s_running = true;
    LPUART_EnableInterrupts(
        LPUART1, kLPUART_RxDataRegFullInterruptEnable);
    NVIC_EnableIRQ(LPUART1_IRQn);
    return true;
}

void GP100_MidiInputStop(void)
{
    LPUART_DisableInterrupts(
        LPUART1, kLPUART_RxDataRegFullInterruptEnable);
    NVIC_DisableIRQ(LPUART1_IRQn);
    s_running = false;
}

bool GP100_MidiInputIsRunning(void)
{
    return s_running;
}

bool GP100_MidiInputPop(uint8_t *byte)
{
    uint16_t read;

    if (byte == NULL)
    {
        return false;
    }
    read = s_readIndex;
    if (read == s_writeIndex)
    {
        return false;
    }
    *byte = s_bytes[read];
    __DMB();
    s_readIndex = (uint16_t)((read + 1u) & GP100_MIDI_INPUT_MASK);
    return true;
}

bool GP100_MidiInputPush(uint8_t byte)
{
    uint32_t interruptMask = DisableGlobalIRQ();
    uint16_t write = s_writeIndex;
    uint16_t next = (uint16_t)((write + 1u) & GP100_MIDI_INPUT_MASK);

    ++s_received;
    if (next == s_readIndex)
    {
        ++s_dropped;
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    s_bytes[write] = byte;
    __DMB();
    s_writeIndex = next;
    EnableGlobalIRQ(interruptMask);
    return true;
}

uint32_t GP100_MidiInputReceived(void)
{
    return s_received;
}

uint32_t GP100_MidiInputDropped(void)
{
    return s_dropped;
}

uint32_t GP100_MidiInputErrors(void)
{
    return s_errors;
}

void LPUART1_IRQHandler(void)
{
    uint32_t status = LPUART_GetStatusFlags(LPUART1);
    const uint32_t errorFlags =
        kLPUART_RxOverrunFlag | kLPUART_NoiseErrorFlag |
        kLPUART_FramingErrorFlag | kLPUART_ParityErrorFlag;

    if ((status & errorFlags) != 0u)
    {
        ++s_errors;
        (void)LPUART_ClearStatusFlags(LPUART1, status & errorFlags);
    }
    while ((LPUART_GetStatusFlags(LPUART1) &
            kLPUART_RxDataRegFullFlag) != 0u)
    {
        uint8_t byte = LPUART_ReadByte(LPUART1);
        (void)GP100_MidiInputPush(byte);
    }
    SDK_ISR_EXIT_BARRIER;
}
