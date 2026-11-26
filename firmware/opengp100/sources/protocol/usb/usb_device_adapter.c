#include "usb_device_adapter.h"

#include "board/board.h"
#include "protocol/midi/midi_input.h"

#include "usb_audio_capture.h"
#include "usb_audio_packet.h"
#include "usb_audio_playback.h"
#include "usb_audio_schedule.h"
#include "usb_descriptors.h"
#include "usb_device_state.h"
#include "usb_midi_packet.h"
#include "protocol/midi/sysex/vendor_sysex.h"

#include "fsl_clock.h"
#include "fsl_common.h"
#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_audio.h"
#include "usb_device_dci.h"
#include "usb_device_ehci.h"
#include "usb_phy.h"

#include <string.h>

#define GP100_USB_CONTROLLER kUSB_ControllerEhci0
#define GP100_USB_AUDIO_PACKET_BYTES 192u
#define GP100_USB_PLAYBACK_TRANSFERS 4u
#define GP100_USB_MIDI_IN_PACKET_BYTES 64u
#define GP100_USB_MIDI_OUT_PACKET_BYTES 256u
#define GP100_USB_MIDI_ENCODED_BYTES 384u
/* Let isochronous completions preempt the SAI DMA render at priority 3. */
#define GP100_USB_IRQ_PRIORITY 2u

_Static_assert(
    GP100_USB_MIDI_ENCODED_BYTES >=
        ((GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES + 2u) / 3u) * 4u,
    "USB MIDI staging must hold the largest factory SysEx response");

typedef struct gp100_usb_control_state {
    uint8_t mute[2][3];
    uint16_t volume[2][3];
    uint32_t pending_sample_rate;
    uint8_t clock_valid;
} gp100_usb_control_state_t;

static usb_device_endpoint_struct_t s_captureEndpoint[] = {
    {GP100_USB_AUDIO_CAPTURE_ENDPOINT, USB_ENDPOINT_ISOCHRONOUS,
     GP100_USB_AUDIO_PACKET_BYTES, 1u}
};
static usb_device_endpoint_struct_t s_playbackEndpoint[] = {
    {GP100_USB_AUDIO_PLAYBACK_ENDPOINT, USB_ENDPOINT_ISOCHRONOUS,
     GP100_USB_AUDIO_PACKET_BYTES, 1u}
};

static usb_device_audio_entity_struct_t s_audioEntities[] = {
    {0x11u, USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_CLOCK_SOURCE_UNIT, 0u},
    {0x0bu, USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_INPUT_TERMINAL, 0u},
    {GP100_USB_AUDIO_PLAYBACK_FEATURE,
     USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_FEATURE_UNIT, 0u},
    {0x0du, USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_OUTPUT_TERMINAL, 0u},
    {0x08u, USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_INPUT_TERMINAL, 0u},
    {GP100_USB_AUDIO_CAPTURE_FEATURE,
     USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_FEATURE_UNIT, 0u},
    {0x0au, USB_DESCRIPTOR_SUBTYPE_AUDIO_CONTROL_OUTPUT_TERMINAL, 0u}
};
static usb_device_audio_entities_struct_t s_audioEntityList = {
    s_audioEntities,
    (uint8_t)(sizeof(s_audioEntities) / sizeof(s_audioEntities[0]))
};

static usb_device_interface_struct_t s_controlAlternates[] = {
    {0u, {0u, NULL}, &s_audioEntityList}
};
/* The SDK needs one class handle per stream. Both handles see the shared
 * control interface, and only the capture handle owns its entities. */
static usb_device_audio_entities_struct_t s_noAudioEntities;
static usb_device_interface_struct_t s_playbackControlAlternates[] = {
    {0u, {0u, NULL}, &s_noAudioEntities}
};
static usb_device_interface_struct_t s_captureAlternates[] = {
    {0u, {0u, NULL}, NULL},
    {1u, {1u, s_captureEndpoint}, NULL},
    {2u, {1u, s_captureEndpoint}, NULL}
};
static usb_device_interface_struct_t s_playbackAlternates[] = {
    {0u, {0u, NULL}, NULL},
    {1u, {1u, s_playbackEndpoint}, NULL},
    {2u, {1u, s_playbackEndpoint}, NULL}
};

static usb_device_interfaces_struct_t s_captureInterfaces[] = {
    {0x01u, 0x01u, 0x20u, GP100_USB_INTERFACE_AUDIO_CONTROL,
     s_controlAlternates, 1u},
    {0x01u, 0x02u, 0x20u, GP100_USB_INTERFACE_CAPTURE,
     s_captureAlternates, 3u}
};
static usb_device_interfaces_struct_t s_playbackInterfaces[] = {
    {0x01u, 0x01u, 0x20u, GP100_USB_INTERFACE_AUDIO_CONTROL,
     s_playbackControlAlternates, 1u},
    {0x01u, 0x02u, 0x20u, GP100_USB_INTERFACE_PLAYBACK,
     s_playbackAlternates, 3u}
};
static usb_device_interface_list_t s_captureInterfaceList[] = {
    {2u, s_captureInterfaces}
};
static usb_device_interface_list_t s_playbackInterfaceList[] = {
    {2u, s_playbackInterfaces}
};
static usb_device_class_struct_t s_captureClass = {
    s_captureInterfaceList, kUSB_DeviceClassTypeAudio, 1u
};
static usb_device_class_struct_t s_playbackClass = {
    s_playbackInterfaceList, kUSB_DeviceClassTypeAudio, 1u
};

static usb_device_handle s_deviceHandle;
static gp100_usb_device_state_t s_deviceState;
static gp100_usb_control_state_t s_controls;
GP100_SDRAM_BSS_SECTION_ALIGN(
    static gp100_usb_capture_ring_t s_captureRing, 32);
GP100_SDRAM_BSS_SECTION_ALIGN(
    static gp100_usb_playback_ring_t s_playbackRing, 32);
static gp100_usb_audio_schedule_t s_captureSchedule;
static gp100_usb_capture_controls_t s_captureControls;
static gp100_usb_playback_controls_t s_playbackControls;
static volatile gp100_usb_device_status_t s_status;
static volatile bool s_audioActive;
static volatile bool s_midiInBusy;
static volatile int16_t s_monitorLevelDb;
static volatile int16_t s_recordLevelDb;
static size_t s_midiInLength;
static size_t s_midiInOffset;
static uint32_t s_midiInTransferLength;

AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_audioCapturePacket[GP100_USB_AUDIO_PACKET_BYTES], 32);
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_audioPlaybackPackets[GP100_USB_PLAYBACK_TRANSFERS]
                                         [GP100_USB_AUDIO_PACKET_BYTES], 32);
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_midiInEncoded[GP100_USB_MIDI_ENCODED_BYTES], 32);
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_midiOutPacket[GP100_USB_MIDI_OUT_PACKET_BYTES], 32);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_stringDescriptor[64], 32);

static const uint8_t s_volumeRange[] = {
    0x01u, 0x00u, 0x01u, 0x80u, 0x00u, 0x00u, 0x01u, 0x00u
};

static usb_status_t GP100_USB_DeviceCallback(
    usb_device_handle handle, uint32_t event, void *parameter);
static usb_status_t GP100_USB_CaptureCallback(
    class_handle_t handle, uint32_t event, void *parameter);
static usb_status_t GP100_USB_PlaybackCallback(
    class_handle_t handle, uint32_t event, void *parameter);

static usb_device_class_config_struct_t s_classConfig[] = {
    {GP100_USB_CaptureCallback, NULL, &s_captureClass},
    {GP100_USB_PlaybackCallback, NULL, &s_playbackClass}
};
static usb_device_class_config_list_struct_t s_classConfigList = {
    s_classConfig, GP100_USB_DeviceCallback, 2u
};

static void GP100_USB_RefreshPacketControls(void)
{
    s_playbackControls.master_mute = s_controls.mute[0][0] != 0u;
    s_playbackControls.left_mute = s_controls.mute[0][1] != 0u;
    s_playbackControls.right_mute = s_controls.mute[0][2] != 0u;
    s_playbackControls.master_volume_8_8 = s_controls.volume[0][0];
    s_playbackControls.left_volume_8_8 = s_controls.volume[0][1];
    s_playbackControls.right_volume_8_8 = s_controls.volume[0][2];
    s_captureControls.master_mute = s_controls.mute[1][0] != 0u;
    s_captureControls.left_mute = s_controls.mute[1][1] != 0u;
    s_captureControls.right_mute = s_controls.mute[1][2] != 0u;
    s_captureControls.master_volume_8_8 = s_controls.volume[1][0];
    s_captureControls.left_volume_8_8 = s_controls.volume[1][1];
    s_captureControls.right_volume_8_8 = s_controls.volume[1][2];
}

static bool GP100_USB_ControlTarget(
    usb_device_control_request_struct_t *request,
    uint8_t *direction,
    uint8_t *channel)
{
    uint8_t entity;

    if (request == NULL || request->setup == NULL ||
        direction == NULL || channel == NULL) {
        return false;
    }
    entity = (uint8_t)(request->setup->wIndex >> 8u);
    if (entity == GP100_USB_AUDIO_PLAYBACK_FEATURE) {
        *direction = 0u;
    } else if (entity == GP100_USB_AUDIO_CAPTURE_FEATURE) {
        *direction = 1u;
    } else {
        return false;
    }
    *channel = (uint8_t)request->setup->wValue;
    return *channel <= 2u;
}

static usb_status_t GP100_USB_AudioControlRequest(
    uint32_t event, void *parameter)
{
    usb_device_control_request_struct_t *request =
        (usb_device_control_request_struct_t *)parameter;
    uint8_t direction;
    uint8_t channel;
    uint32_t changes;

    if (request == NULL || request->setup == NULL) {
        return kStatus_USB_InvalidRequest;
    }
    switch (event) {
    case USB_DEVICE_AUDIO_CS_GET_CUR_SAMPLING_FREQ_CONTROL:
        request->buffer = (uint8_t *)&s_deviceState.sample_rate;
        request->length = sizeof(s_deviceState.sample_rate);
        return kStatus_USB_Success;
    case USB_DEVICE_AUDIO_CS_SET_CUR_SAMPLING_FREQ_CONTROL:
        if (request->isSetup != 0u) {
            s_controls.pending_sample_rate = s_deviceState.sample_rate;
            request->buffer = (uint8_t *)&s_controls.pending_sample_rate;
            request->length = sizeof(s_controls.pending_sample_rate);
            return kStatus_USB_Success;
        }
        if (!gp100_usb_device_set_sample_rate(
                &s_deviceState, s_controls.pending_sample_rate, &changes)) {
            return kStatus_USB_InvalidRequest;
        }
        if ((changes & GP100_USB_ENDPOINT_RATE_CHANGED) != 0u) {
            gp100_usb_capture_clear_buffer(&s_captureRing);
            gp100_usb_playback_clear_buffer(&s_playbackRing);
            gp100_usb_audio_schedule_reset(&s_captureSchedule);
        }
        return kStatus_USB_Success;
    case USB_DEVICE_AUDIO_CS_GET_CUR_CLOCK_VALID_CONTROL:
        request->buffer = &s_controls.clock_valid;
        request->length = sizeof(s_controls.clock_valid);
        return kStatus_USB_Success;
    case USB_DEVICE_AUDIO_CS_GET_RANGE_SAMPLING_FREQ_CONTROL: {
        gp100_usb_descriptor_view_t range =
            gp100_usb_clock_rate_range();

        request->buffer = (uint8_t *)(uintptr_t)range.bytes;
        request->length = (uint32_t)range.length;
        return kStatus_USB_Success;
    }
    case USB_DEVICE_AUDIO_FU_GET_RANGE_VOLUME_CONTROL:
        request->buffer = (uint8_t *)(uintptr_t)s_volumeRange;
        request->length = sizeof(s_volumeRange);
        return kStatus_USB_Success;
    case USB_DEVICE_AUDIO_FU_GET_CUR_MUTE_CONTROL:
    case USB_DEVICE_AUDIO_FU_SET_CUR_MUTE_CONTROL:
        if (!GP100_USB_ControlTarget(request, &direction, &channel)) {
            return kStatus_USB_InvalidRequest;
        }
        request->buffer = &s_controls.mute[direction][channel];
        request->length = 1u;
        return kStatus_USB_Success;
    case USB_DEVICE_AUDIO_FU_GET_CUR_VOLUME_CONTROL:
    case USB_DEVICE_AUDIO_FU_SET_CUR_VOLUME_CONTROL:
        if (!GP100_USB_ControlTarget(request, &direction, &channel)) {
            return kStatus_USB_InvalidRequest;
        }
        request->buffer = (uint8_t *)&s_controls.volume[direction][channel];
        request->length = 2u;
        return kStatus_USB_Success;
    default:
        return kStatus_USB_InvalidRequest;
    }
}

static bool GP100_USB_QueueCapture(class_handle_t handle, bool firstPacket)
{
    gp100_usb_audio_format_t format;
    int32_t frames[24u * 2u];
    uint8_t frameCount;
    size_t packetBytes;

    if (!gp100_usb_device_audio_format(
            &s_deviceState, GP100_USB_INTERFACE_CAPTURE, &format) ||
        !gp100_usb_audio_next_frame_count(
            &s_captureSchedule, s_deviceState.sample_rate, &frameCount) ||
        frameCount > sizeof(frames) / (2u * sizeof(frames[0]))) {
        return false;
    }
    if (firstPacket) {
        /* Prime the stream with silence. Gain advances on completion only. */
        packetBytes = (size_t)frameCount *
            (format == GP100_USB_AUDIO_PCM16_STEREO ? 4u : 8u);
        memset(s_audioCapturePacket, 0, packetBytes);
    } else {
        if (!s_audioActive) {
            memset(frames, 0, frameCount * 2u * sizeof(frames[0]));
        } else if (gp100_usb_capture_read(
                       &s_captureRing, frames, frameCount) ==
                   GP100_USB_CAPTURE_READ_ERROR) {
            return false;
        }
        GP100_USB_RefreshPacketControls();
        if (!gp100_usb_capture_encode_packet(
                &s_captureControls, format, frames, frameCount,
                s_audioCapturePacket, sizeof(s_audioCapturePacket),
                &packetBytes)) {
            return false;
        }
    }
    return USB_DeviceAudioSend(
               handle, GP100_USB_AUDIO_CAPTURE_ENDPOINT & 0x0fu,
               s_audioCapturePacket, (uint32_t)packetBytes) ==
           kStatus_USB_Success;
}

static bool GP100_USB_QueuePlayback(usb_device_handle handle, uint8_t *packet)
{
    const usb_device_struct_t *device = (const usb_device_struct_t *)handle;

    /* The SDK audio and device request APIs allow one pending receive.
     * Queue playback directly on EHCI so more microframes can arrive while
     * a completion is being serviced. The SDK still owns endpoint setup,
     * cancellation, and completion dispatch. */
    return USB_DeviceEhciRecv(device->controllerHandle,
                             GP100_USB_AUDIO_PLAYBACK_ENDPOINT, packet,
                             GP100_USB_AUDIO_PACKET_BYTES) == kStatus_USB_Success;
}

static usb_status_t GP100_USB_CaptureCallback(
    class_handle_t handle, uint32_t event, void *parameter)
{
    if (event == kUSB_DeviceAudioEventStreamSendResponse) {
        usb_device_endpoint_callback_message_struct_t *message =
            (usb_device_endpoint_callback_message_struct_t *)parameter;

        if (message == NULL || message->length == USB_CANCELLED_TRANSFER_LENGTH) {
            return kStatus_USB_Success;
        }
        ++s_status.audio_in_packets;
        if (!GP100_USB_QueueCapture(handle, false)) {
            ++s_status.transfer_errors;
            return kStatus_USB_Error;
        }
        return kStatus_USB_Success;
    }
    return GP100_USB_AudioControlRequest(event, parameter);
}

static usb_status_t GP100_USB_PlaybackCallback(
    class_handle_t handle, uint32_t event, void *parameter)
{
    (void)handle;
    if (event == kUSB_DeviceAudioEventStreamRecvResponse) {
        usb_device_endpoint_callback_message_struct_t *message =
            (usb_device_endpoint_callback_message_struct_t *)parameter;
        gp100_usb_audio_format_t format;
        int32_t frames[48u * 2u];
        size_t frameCount;

        if (message == NULL || message->length == USB_CANCELLED_TRANSFER_LENGTH) {
            return kStatus_USB_Success;
        }
        if (message->buffer == NULL) {
            ++s_status.transfer_errors;
            return kStatus_USB_Error;
        }
        GP100_USB_RefreshPacketControls();
        if (!gp100_usb_device_audio_format(
                &s_deviceState, GP100_USB_INTERFACE_PLAYBACK, &format) ||
            !gp100_usb_playback_decode_packet(
                &s_playbackControls, format, message->buffer,
                message->length, frames, 48u, &frameCount)) {
            ++s_status.transfer_errors;
        } else {
            if (!s_audioActive) {
                memset(frames, 0, frameCount * 2u * sizeof(frames[0]));
            }
            if (!gp100_usb_playback_publish(
                    &s_playbackRing, frames, frameCount, s_monitorLevelDb)) {
                ++s_status.transfer_errors;
            } else {
                ++s_status.audio_out_packets;
            }
        }
        if (!GP100_USB_QueuePlayback(s_deviceHandle, message->buffer)) {
            ++s_status.transfer_errors;
            return kStatus_USB_Error;
        }
        return kStatus_USB_Success;
    }
    return GP100_USB_AudioControlRequest(event, parameter);
}

static size_t GP100_USB_MidiEventLength(uint8_t cin)
{
    switch (cin & 0x0fu) {
    case 0x05u:
    case 0x0fu:
        return 1u;
    case 0x02u:
    case 0x06u:
    case 0x0cu:
    case 0x0du:
        return 2u;
    case 0x03u:
    case 0x04u:
    case 0x07u:
    case 0x08u:
    case 0x09u:
    case 0x0au:
    case 0x0bu:
    case 0x0eu:
        return 3u;
    default:
        return 0u;
    }
}

static usb_status_t GP100_USB_MidiOutCallback(
    usb_device_handle handle,
    usb_device_endpoint_callback_message_struct_t *message,
    void *parameter)
{
    size_t offset;

    (void)parameter;
    if (message == NULL || message->length == USB_CANCELLED_TRANSFER_LENGTH) {
        return kStatus_USB_Success;
    }
    for (offset = 0u; offset + 4u <= message->length; offset += 4u) {
        size_t length = GP100_USB_MidiEventLength(message->buffer[offset]);
        size_t index;

        for (index = 0u; index < length; ++index) {
            (void)GP100_MidiInputPush(message->buffer[offset + 1u + index]);
        }
    }
    ++s_status.midi_out_packets;
    if (USB_DeviceRecvRequest(
            handle, GP100_USB_MIDI_OUT_ENDPOINT,
            s_midiOutPacket, sizeof(s_midiOutPacket)) != kStatus_USB_Success) {
        ++s_status.transfer_errors;
        return kStatus_USB_Error;
    }
    return kStatus_USB_Success;
}

static usb_status_t GP100_USB_MidiInCallback(
    usb_device_handle handle,
    usb_device_endpoint_callback_message_struct_t *message,
    void *parameter)
{
    (void)handle;
    (void)parameter;
    if (message == NULL || message->length == USB_CANCELLED_TRANSFER_LENGTH) {
        s_midiInLength = 0u;
        s_midiInOffset = 0u;
        s_midiInTransferLength = 0u;
        s_midiInBusy = false;
        return kStatus_USB_Success;
    }
    ++s_status.midi_in_packets;
    s_midiInOffset += s_midiInTransferLength;
    if (s_midiInOffset < s_midiInLength) {
        size_t remaining = s_midiInLength - s_midiInOffset;

        s_midiInTransferLength = (uint32_t)(
            remaining < GP100_USB_MIDI_IN_PACKET_BYTES
                ? remaining : GP100_USB_MIDI_IN_PACKET_BYTES);
        if (USB_DeviceSendRequest(
                s_deviceHandle, GP100_USB_MIDI_IN_ENDPOINT & 0x0fu,
                &s_midiInEncoded[s_midiInOffset],
                s_midiInTransferLength) == kStatus_USB_Success) {
            return kStatus_USB_Success;
        }
        ++s_status.transfer_errors;
    }
    s_midiInLength = 0u;
    s_midiInOffset = 0u;
    s_midiInTransferLength = 0u;
    s_midiInBusy = false;
    return kStatus_USB_Success;
}

static usb_status_t GP100_USB_MidiStart(usb_device_handle handle)
{
    usb_device_endpoint_init_struct_t endpoint;
    usb_device_endpoint_callback_struct_t callback;

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpointAddress = GP100_USB_MIDI_IN_ENDPOINT;
    endpoint.transferType = USB_ENDPOINT_BULK;
    endpoint.maxPacketSize = GP100_USB_MIDI_IN_PACKET_BYTES;
    callback.callbackFn = GP100_USB_MidiInCallback;
    callback.callbackParam = NULL;
    if (USB_DeviceInitEndpoint(handle, &endpoint, &callback) !=
        kStatus_USB_Success) {
        return kStatus_USB_Error;
    }

    endpoint.endpointAddress = GP100_USB_MIDI_OUT_ENDPOINT;
    endpoint.maxPacketSize = GP100_USB_MIDI_OUT_PACKET_BYTES;
    callback.callbackFn = GP100_USB_MidiOutCallback;
    if (USB_DeviceInitEndpoint(handle, &endpoint, &callback) !=
        kStatus_USB_Success) {
        (void)USB_DeviceDeinitEndpoint(
            handle, GP100_USB_MIDI_IN_ENDPOINT);
        return kStatus_USB_Error;
    }
    return USB_DeviceRecvRequest(
        handle, GP100_USB_MIDI_OUT_ENDPOINT,
        s_midiOutPacket, sizeof(s_midiOutPacket));
}

static void GP100_USB_ApplyEndpointChanges(
    usb_device_handle handle, uint32_t changes)
{
    if ((changes & GP100_USB_ENDPOINT_MIDI_STOP) != 0u) {
        (void)USB_DeviceDeinitEndpoint(handle, GP100_USB_MIDI_IN_ENDPOINT);
        (void)USB_DeviceDeinitEndpoint(handle, GP100_USB_MIDI_OUT_ENDPOINT);
        s_midiInBusy = false;
        s_midiInLength = 0u;
        s_midiInOffset = 0u;
        s_midiInTransferLength = 0u;
    }
    if ((changes & GP100_USB_ENDPOINT_MIDI_START) != 0u &&
        GP100_USB_MidiStart(handle) != kStatus_USB_Success) {
        ++s_status.transfer_errors;
    }
    if ((changes & (GP100_USB_ENDPOINT_CAPTURE_START |
                    GP100_USB_ENDPOINT_CAPTURE_STOP)) != 0u) {
        gp100_usb_audio_schedule_reset(&s_captureSchedule);
    }
    if ((changes & GP100_USB_ENDPOINT_CAPTURE_START) != 0u) {
        gp100_usb_capture_clear_buffer(&s_captureRing);
        if (!GP100_USB_QueueCapture(s_classConfig[0].classHandle, true)) {
            ++s_status.transfer_errors;
        }
    }
    if ((changes & (GP100_USB_ENDPOINT_PLAYBACK_START |
                    GP100_USB_ENDPOINT_PLAYBACK_STOP)) != 0u) {
        gp100_usb_playback_clear_buffer(&s_playbackRing);
    }
    if ((changes & GP100_USB_ENDPOINT_PLAYBACK_START) != 0u) {
        for (size_t i = 0; i < GP100_USB_PLAYBACK_TRANSFERS; ++i) {
            if (!GP100_USB_QueuePlayback(handle, s_audioPlaybackPackets[i])) {
                ++s_status.transfer_errors;
                break;
            }
        }
    }
}

static usb_status_t GP100_USB_DeviceCallback(
    usb_device_handle handle, uint32_t event, void *parameter)
{
    uint32_t changes;

    switch (event) {
    case kUSB_DeviceEventBusReset:
    {
        uint8_t speed;

        gp100_usb_device_state_reset(&s_deviceState);
        memset(s_controls.mute, 0, sizeof(s_controls.mute));
        memset(s_controls.volume, 0, sizeof(s_controls.volume));
        gp100_usb_capture_clear_buffer(&s_captureRing);
        gp100_usb_playback_clear_buffer(&s_playbackRing);
        gp100_usb_audio_schedule_reset(&s_captureSchedule);
        s_midiInBusy = false;
        s_midiInLength = 0u;
        s_midiInOffset = 0u;
        s_midiInTransferLength = 0u;
        s_status.attached = false;
        s_status.configuration = 0u;
        ++s_status.bus_resets;
        if (USB_DeviceClassGetSpeed(GP100_USB_CONTROLLER, &speed) !=
            kStatus_USB_Success) {
            ++s_status.transfer_errors;
        } else {
            s_status.speed = speed;
        }
        return kStatus_USB_Success;
    }
    case kUSB_DeviceEventSetConfiguration:
        if (parameter == NULL ||
            !gp100_usb_device_set_configuration(
                &s_deviceState, *(uint8_t *)parameter, &changes)) {
            return kStatus_USB_InvalidRequest;
        }
        s_status.configuration = s_deviceState.configuration;
        s_status.attached = s_deviceState.configuration != 0u;
        GP100_USB_ApplyEndpointChanges(handle, changes);
        return kStatus_USB_Success;
    case kUSB_DeviceEventSetInterface:
        if (parameter != NULL) {
            uint16_t value = *(uint16_t *)parameter;
            uint8_t interfaceNumber = (uint8_t)(value >> 8u);
            uint8_t alternate = (uint8_t)value;

            if (gp100_usb_device_set_interface(
                    &s_deviceState, interfaceNumber, alternate, &changes)) {
                GP100_USB_ApplyEndpointChanges(handle, changes);
                return kStatus_USB_Success;
            }
        }
        return kStatus_USB_InvalidRequest;
    case kUSB_DeviceEventGetConfiguration:
        if (parameter != NULL) {
            *(uint8_t *)parameter = s_deviceState.configuration;
            return kStatus_USB_Success;
        }
        break;
    case kUSB_DeviceEventGetInterface:
        if (parameter != NULL) {
            uint16_t *value = (uint16_t *)parameter;
            uint8_t interfaceNumber = (uint8_t)(*value >> 8u);

            if (interfaceNumber < GP100_USB_INTERFACE_COUNT) {
                *value = (uint16_t)((*value & 0xff00u) |
                    s_deviceState.alternate[interfaceNumber]);
                return kStatus_USB_Success;
            }
        }
        break;
    case kUSB_DeviceEventGetDeviceDescriptor:
        if (parameter != NULL) {
            usb_device_get_device_descriptor_struct_t *descriptor =
                (usb_device_get_device_descriptor_struct_t *)parameter;
            gp100_usb_descriptor_view_t view =
                gp100_usb_device_descriptor();

            descriptor->buffer = (uint8_t *)(uintptr_t)view.bytes;
            descriptor->length = (uint32_t)view.length;
            return kStatus_USB_Success;
        }
        break;
    case kUSB_DeviceEventGetConfigurationDescriptor:
        if (parameter != NULL) {
            usb_device_get_configuration_descriptor_struct_t *descriptor =
                (usb_device_get_configuration_descriptor_struct_t *)parameter;
            gp100_usb_descriptor_view_t view;

            if (descriptor->configuration != 0u) {
                return kStatus_USB_InvalidRequest;
            }
            view = gp100_usb_configuration_descriptor();
            descriptor->buffer = (uint8_t *)(uintptr_t)view.bytes;
            descriptor->length = (uint32_t)view.length;
            return kStatus_USB_Success;
        }
        break;
    case kUSB_DeviceEventGetStringDescriptor:
        if (parameter != NULL) {
            usb_device_get_string_descriptor_struct_t *descriptor =
                (usb_device_get_string_descriptor_struct_t *)parameter;
            size_t length;

            if ((descriptor->stringIndex != 0u &&
                 descriptor->languageId != 0x0409u) ||
                !gp100_usb_build_string_descriptor(
                    descriptor->stringIndex, s_stringDescriptor,
                    sizeof(s_stringDescriptor), &length)) {
                return kStatus_USB_InvalidRequest;
            }
            descriptor->buffer = s_stringDescriptor;
            descriptor->length = (uint32_t)length;
            return kStatus_USB_Success;
        }
        break;
    default:
        break;
    }
    return kStatus_USB_InvalidRequest;
}

void USB_OTG1_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(s_deviceHandle);
    SDK_ISR_EXIT_BARRIER;
}

static void GP100_USB_ClockInitialize(void)
{
    usb_phy_config_struct_t phyConfig = {0x0cu, 0x06u, 0x06u};

    (void)CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000u);
    (void)CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000u);
    USB_EhciPhyInit(GP100_USB_CONTROLLER, BOARD_XTAL0_CLK_HZ, &phyConfig);
}

bool GP100_USB_DeviceInitialize(void)
{
    if (s_status.initialized) {
        return true;
    }
    memset((void *)&s_status, 0, sizeof(s_status));
    s_status.audio_active = s_audioActive;
    /* Keep gains already published by the audio binding before USB starts. */
    memset(&s_controls, 0, sizeof(s_controls));
    s_controls.clock_valid = 1u;
    gp100_usb_device_state_reset(&s_deviceState);
    gp100_usb_capture_reset(&s_captureRing);
    gp100_usb_playback_reset(&s_playbackRing);
    gp100_usb_audio_schedule_reset(&s_captureSchedule);
    gp100_usb_capture_controls_reset(&s_captureControls);
    gp100_usb_playback_controls_reset(&s_playbackControls);

    GP100_USB_ClockInitialize();
    if (USB_DeviceClassInit(
            GP100_USB_CONTROLLER, &s_classConfigList, &s_deviceHandle) !=
        kStatus_USB_Success) {
        return false;
    }
    NVIC_SetPriority(USB_OTG1_IRQn, GP100_USB_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(USB_OTG1_IRQn);
    EnableIRQ(USB_OTG1_IRQn);
    SDK_DelayAtLeastUs(5000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    if (USB_DeviceRun(s_deviceHandle) != kStatus_USB_Success) {
        DisableIRQ(USB_OTG1_IRQn);
        (void)USB_DeviceClassDeinit(GP100_USB_CONTROLLER);
        s_deviceHandle = NULL;
        return false;
    }
    s_status.initialized = true;
    g_gp100_board_state.initialized |= kGP100_InitUsbDevice;
    return true;
}

void GP100_USB_DeviceStop(void)
{
    if (!s_status.initialized) {
        return;
    }
    DisableIRQ(USB_OTG1_IRQn);
    (void)USB_DeviceStop(s_deviceHandle);
    (void)USB_DeviceClassDeinit(GP100_USB_CONTROLLER);
    s_deviceHandle = NULL;
    s_midiInBusy = false;
    s_midiInLength = 0u;
    s_midiInOffset = 0u;
    s_midiInTransferLength = 0u;
    s_status.initialized = false;
    s_status.attached = false;
    s_status.configuration = 0u;
    g_gp100_board_state.initialized &= ~kGP100_InitUsbDevice;
}

void GP100_USB_DeviceGetStatus(gp100_usb_device_status_t *status)
{
    uint32_t interruptMask;

    if (status == NULL) {
        return;
    }
    interruptMask = DisableGlobalIRQ();
    memcpy(status, (const void *)&s_status, sizeof(*status));
    EnableGlobalIRQ(interruptMask);
}

bool GP100_USB_SetAudioLevels(
    int16_t monitorLevelDb, int16_t recordLevelDb)
{
    uint32_t interruptMask;

    if (monitorLevelDb < -20 || monitorLevelDb > 6 ||
        recordLevelDb < GP100_USB_CAPTURE_GAIN_MIN_DB ||
        recordLevelDb > GP100_USB_CAPTURE_GAIN_MAX_DB) {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    s_monitorLevelDb = monitorLevelDb;
    s_recordLevelDb = recordLevelDb;
    __DMB();
    EnableGlobalIRQ(interruptMask);
    return true;
}

void GP100_USB_SetAudioActive(bool active)
{
    uint32_t interruptMask = DisableGlobalIRQ();

    if (s_audioActive != active) {
        /* The factory never deinitializes audio, but this standalone transport
         * is restartable. Drop pre-stop samples at that extra lifecycle seam. */
        gp100_usb_capture_reset(&s_captureRing);
        gp100_usb_playback_reset(&s_playbackRing);
    }
    s_audioActive = active;
    s_status.audio_active = active;
    __DMB();
    EnableGlobalIRQ(interruptMask);
}

bool GP100_USB_PlaybackRead(int32_t *interleavedStereo, size_t frameCount)
{
    gp100_usb_playback_read_result_t result;
    uint32_t interruptMask;

    if (interleavedStereo == NULL && frameCount != 0u) {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    if (!s_audioActive || !s_status.attached ||
        s_deviceState.alternate[GP100_USB_INTERFACE_PLAYBACK] == 0u) {
        if (interleavedStereo != NULL) {
            memset(interleavedStereo, 0,
                   frameCount * 2u * sizeof(*interleavedStereo));
        }
        EnableGlobalIRQ(interruptMask);
        return true;
    }
    result = gp100_usb_playback_read(
        &s_playbackRing, interleavedStereo, frameCount);
    EnableGlobalIRQ(interruptMask);
    return result != GP100_USB_PLAYBACK_READ_ERROR;
}

bool GP100_USB_CaptureWrite(
    const int32_t *interleavedStereo, size_t frameCount)
{
    bool result;
    uint32_t interruptMask;

    if (interleavedStereo == NULL && frameCount != 0u) {
        return false;
    }
    interruptMask = DisableGlobalIRQ();
    if (!s_audioActive) {
        EnableGlobalIRQ(interruptMask);
        return true;
    }
    /* Keep the capture producer gain current before a host opens the stream.
     * Alternate selection clears the ring while preserving that gain. */
    result = gp100_usb_capture_publish(
        &s_captureRing, interleavedStereo, frameCount, s_recordLevelDb);
    EnableGlobalIRQ(interruptMask);
    return result;
}

bool GP100_USB_MidiWrite(const uint8_t *message, size_t messageLength)
{
    size_t encodedLength;
    uint32_t interruptMask;
    bool encoded;

    interruptMask = DisableGlobalIRQ();
    if (!s_status.attached || s_midiInBusy) {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    encoded = gp100_usb_midi_encode(
        0u, message, messageLength, s_midiInEncoded,
        sizeof(s_midiInEncoded), &encodedLength);
    if (!encoded || encodedLength == 0u) {
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    s_midiInLength = encodedLength;
    s_midiInOffset = 0u;
    s_midiInTransferLength = (uint32_t)(
        encodedLength < GP100_USB_MIDI_IN_PACKET_BYTES
            ? encodedLength : GP100_USB_MIDI_IN_PACKET_BYTES);
    s_midiInBusy = true;
    if (USB_DeviceSendRequest(
            s_deviceHandle, GP100_USB_MIDI_IN_ENDPOINT & 0x0fu,
            s_midiInEncoded, s_midiInTransferLength) !=
        kStatus_USB_Success) {
        s_midiInBusy = false;
        s_midiInLength = 0u;
        s_midiInTransferLength = 0u;
        EnableGlobalIRQ(interruptMask);
        return false;
    }
    EnableGlobalIRQ(interruptMask);
    return true;
}

bool GP100_USB_MidiCanWrite(void)
{
    uint32_t interruptMask = DisableGlobalIRQ();
    bool ready = s_status.attached && !s_midiInBusy;

    EnableGlobalIRQ(interruptMask);
    return ready;
}
