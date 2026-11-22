#ifndef GP100_AUDIO_TRANSPORT_H
#define GP100_AUDIO_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#define GP100_AUDIO_BLOCK_FRAMES 64u
#define GP100_AUDIO_CHANNEL_COUNT 2u
#define GP100_AUDIO_BLOCK_SAMPLES \
    (GP100_AUDIO_BLOCK_FRAMES * GP100_AUDIO_CHANNEL_COUNT)

typedef void (*gp100_audio_block_callback_t)(
    const int32_t input[GP100_AUDIO_BLOCK_SAMPLES],
    int32_t output[GP100_AUDIO_BLOCK_SAMPLES],
    uint32_t sequence,
    void *userData);

/* Configure the recovered SAI1/eDMA ping-pong path. SAI and both DMA requests
 * remain stopped until GP100_AudioTransportStart() is called explicitly. */
void GP100_AudioTransportInit(void);

bool GP100_AudioTransportStart(void);
void GP100_AudioTransportStop(void);
bool GP100_AudioTransportIsRunning(void);

/* Foreground calls with interrupts enabled. Begin waits until both TX DMA
 * halves are silent. Pair a successful begin with end after the flash write. */
bool GP100_AudioTransportBeginFlashWrite(void);
void GP100_AudioTransportEndFlashWrite(void);

/* Install the interrupt-context block processor. Samples are signed 24-bit
 * values in int32_t containers, matching the factory DSP work format. A NULL
 * callback selects a direct input-to-output pass-through. The callback has
 * about 1.45 ms at the recovered 64-frame/44.1 kHz cadence. */
void GP100_AudioTransportSetCallback(gp100_audio_block_callback_t callback,
                                    void *userData);

uint32_t GP100_AudioTransportCompletedBlocks(void);
uint32_t GP100_AudioTransportCapturedBlocks(void);

#endif
