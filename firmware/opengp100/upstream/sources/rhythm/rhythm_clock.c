#include "rhythm_clock.h"

#include "board/board.h"
#include "board/peripherals.h"

#include "fsl_clock.h"
#include "fsl_qtmr.h"

#define GP100_RHYTHM_QTMR_PRESCALER 128u
#define GP100_MICROSECONDS_PER_MINUTE 60000000u
#define GP100_MICROSECONDS_PER_SECOND 1000000u

static volatile uint32_t s_pendingTicks;
static volatile uint32_t s_timerClockHz;
static volatile uint16_t s_tempoBpm;
static volatile bool s_initialized;
static volatile bool s_running;

static bool GP100_RhythmPeriodTicks(uint32_t periodUs, uint16_t *periodTicks)
{
    const uint64_t ticks =
        ((uint64_t)s_timerClockHz * periodUs) /
        GP100_MICROSECONDS_PER_SECOND;

    if (ticks == 0u || ticks > UINT16_MAX || periodTicks == NULL)
    {
        return false;
    }
    *periodTicks = (uint16_t)ticks;
    return true;
}

static bool GP100_RhythmSetPeriodUs(uint32_t periodUs)
{
    uint16_t periodTicks;

    if (!GP100_RhythmPeriodTicks(periodUs, &periodTicks))
    {
        return false;
    }
    QTMR_SetTimerPeriod(TMR4, kQTMR_Channel_0, periodTicks);
    return true;
}

void GP100_RhythmClockInit(void)
{
    qtmr_config_t config;
    uint16_t initialPeriodTicks;
    const uint32_t ipgClockHz = CLOCK_GetFreq(kCLOCK_IpgClk);

    s_pendingTicks = 0u;
    s_tempoBpm = 0u;
    s_initialized = false;
    s_running = false;
    s_timerClockHz = ipgClockHz / GP100_RHYTHM_QTMR_PRESCALER;

    if (ipgClockHz == 0u ||
        !GP100_RhythmPeriodTicks(GP100_RHYTHM_INITIAL_PERIOD_US,
                                 &initialPeriodTicks))
    {
        g_gp100_board_state.peripheral_errors |=
            kGP100_ErrorRhythmTimerClock;
        return;
    }

    QTMR_StopTimer(TMR4, kQTMR_Channel_0);
    QTMR_DisableInterrupts(TMR4, kQTMR_Channel_0,
                           kQTMR_CompareInterruptEnable);
    QTMR_GetDefaultConfig(&config);
    config.primarySource = kQTMR_ClockDivide_128;
    QTMR_Init(TMR4, kQTMR_Channel_0, &config);
    TMR4->CHANNEL[kQTMR_Channel_0].LOAD = 0u;
    TMR4->CHANNEL[kQTMR_Channel_0].CNTR = 0u;
    QTMR_SetTimerPeriod(TMR4, kQTMR_Channel_0, initialPeriodTicks);
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0,
                          kQTMR_CompareFlag | kQTMR_OverflowFlag);

    /* Factory setup writes 0x10 to the TMR4 NVIC priority byte. */
    NVIC_SetPriority(TMR4_IRQn, 1u);
    NVIC_ClearPendingIRQ(TMR4_IRQn);

    s_initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitRhythmClock;
}

bool GP100_RhythmClockStart(void)
{
    uint32_t irqMask;

    if (!s_initialized)
    {
        return false;
    }
    if (s_running)
    {
        return true;
    }

    irqMask = DisableGlobalIRQ();
    s_pendingTicks = 0u;
    TMR4->CHANNEL[kQTMR_Channel_0].CNTR = 0u;
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0,
                          kQTMR_CompareFlag | kQTMR_OverflowFlag);
    NVIC_ClearPendingIRQ(TMR4_IRQn);
    s_running = true;
    QTMR_EnableInterrupts(TMR4, kQTMR_Channel_0,
                          kQTMR_CompareInterruptEnable);
    EnableIRQ(TMR4_IRQn);
    QTMR_StartTimer(TMR4, kQTMR_Channel_0, kQTMR_PriSrcRiseEdge);
    EnableGlobalIRQ(irqMask);
    return true;
}

void GP100_RhythmClockStop(void)
{
    uint32_t irqMask = DisableGlobalIRQ();

    QTMR_StopTimer(TMR4, kQTMR_Channel_0);
    QTMR_DisableInterrupts(TMR4, kQTMR_Channel_0,
                           kQTMR_CompareInterruptEnable);
    DisableIRQ(TMR4_IRQn);
    NVIC_ClearPendingIRQ(TMR4_IRQn);
    s_running = false;
    EnableGlobalIRQ(irqMask);
}

bool GP100_RhythmClockIsRunning(void)
{
    return s_running;
}

bool GP100_RhythmClockSetTempoBpm(uint16_t tempoBpm)
{
    uint32_t periodUs;

    if (!s_initialized || tempoBpm < GP100_RHYTHM_MIN_TEMPO_BPM ||
        tempoBpm > GP100_RHYTHM_MAX_TEMPO_BPM)
    {
        return false;
    }

    /* Keep the factory's integer division order: first calculate whole
     * microseconds per sequencer tick, then convert that period to QTMR ticks. */
    periodUs = GP100_MICROSECONDS_PER_MINUTE /
               ((uint32_t)tempoBpm * GP100_RHYTHM_TICKS_PER_QUARTER);
    if (!GP100_RhythmSetPeriodUs(periodUs))
    {
        return false;
    }
    s_tempoBpm = tempoBpm;
    return true;
}

uint16_t GP100_RhythmClockTempoBpm(void)
{
    return s_tempoBpm;
}

uint32_t GP100_RhythmClockDrainPending(void)
{
    uint32_t pending;
    uint32_t irqMask = DisableGlobalIRQ();

    pending = s_pendingTicks;
    s_pendingTicks = 0u;
    EnableGlobalIRQ(irqMask);
    return pending;
}

uint32_t GP100_RhythmClockPending(void)
{
    return s_pendingTicks;
}

uint32_t GP100_RhythmClockTimerHz(void)
{
    return s_timerClockHz;
}

static bool GP100_RhythmPlayerStart(void *context)
{
    (void)context;
    return GP100_RhythmClockStart();
}

static void GP100_RhythmPlayerStop(void *context)
{
    (void)context;
    GP100_RhythmClockStop();
}

static bool GP100_RhythmPlayerSetTempo(void *context, uint16_t tempoBpm)
{
    (void)context;
    return GP100_RhythmClockSetTempoBpm(tempoBpm);
}

static uint32_t GP100_RhythmPlayerDrain(void *context)
{
    (void)context;
    return GP100_RhythmClockDrainPending();
}

void GP100_RhythmClockGetPlayerClock(gp100_rhythm_clock_t *clock)
{
    if (clock == NULL)
    {
        return;
    }
    clock->start = GP100_RhythmPlayerStart;
    clock->stop = GP100_RhythmPlayerStop;
    clock->set_tempo_bpm = GP100_RhythmPlayerSetTempo;
    clock->drain_pending = GP100_RhythmPlayerDrain;
    clock->context = NULL;
}

void TMR4_IRQHandler(void)
{
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0, kQTMR_CompareFlag);
    ++s_pendingTicks;
}
