#ifndef GP100_USB_DEVICE_ADAPTER_H
#define GP100_USB_DEVICE_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gp100_usb_device_status {
    bool initialized;
    bool attached;
    bool audio_active;
    uint8_t configuration;
    uint8_t speed;
    uint32_t bus_resets;
    uint32_t audio_out_packets;
    uint32_t audio_in_packets;
    uint32_t midi_out_packets;
    uint32_t midi_in_packets;
    uint32_t transfer_errors;
} gp100_usb_device_status_t;

bool GP100_USB_DeviceInitialize(void);
void GP100_USB_DeviceStop(void);
void GP100_USB_DeviceGetStatus(gp100_usb_device_status_t *status);

/* Publish the recovered Global USB Audio gains. Host playback uses the
 * monitor level (-20..+6 dB); device capture uses the recording level
 * (-20..+20 dB). */
bool GP100_USB_SetAudioLevels(
    int16_t monitorLevelDb, int16_t recordLevelDb);

/* Mirror the factory audio-runtime flag used by both isochronous callbacks.
 * Inactive capture sends silence; inactive playback publishes silence so the
 * asynchronous ring continues to advance without retaining host audio. Ring
 * state is cleared across this standalone API's extra stop/start lifecycle. */
void GP100_USB_SetAudioActive(bool active);

/* Audio samples use the reference library's signed 24-bit-in-int32 domain. */
bool GP100_USB_PlaybackRead(int32_t *interleavedStereo, size_t frameCount);
bool GP100_USB_CaptureWrite(
    const int32_t *interleavedStereo, size_t frameCount);

/* Queues one complete raw MIDI message for USB-MIDI cable zero. Long SysEx
 * replies are packetized into consecutive 64-byte bulk transfers. */
bool GP100_USB_MidiCanWrite(void);
bool GP100_USB_MidiWrite(const uint8_t *message, size_t messageLength);

#endif
