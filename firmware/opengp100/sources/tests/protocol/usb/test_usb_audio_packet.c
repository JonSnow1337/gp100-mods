#include "protocol/usb/usb_audio_packet.h"

#include <math.h>
#include <stdio.h>

static int close_enough(float actual, float expected)
{
    return fabsf(actual - expected) < 0.000001f;
}

static int test_volume_decoder(void)
{
    if (!close_enough(gp100_usb_volume_db(0x0000u), 0.0f) ||
        !close_enough(gp100_usb_volume_db(0x0180u), 1.5f) ||
        !close_enough(gp100_usb_volume_db(0xff00u), -1.0f) ||
        !close_enough(gp100_usb_volume_db(0xff80u), -0.5f)) {
        return 1;
    }
    return 0;
}

static int test_pcm16_and_control_slew(void)
{
    gp100_usb_playback_controls_t controls;
    const uint8_t packet[4] = {0xffu, 0x7fu, 0x00u, 0x80u};
    int32_t output[2];
    size_t frames = 0u;

    gp100_usb_playback_controls_reset(&controls);
    controls.current_master_q7 = 128;
    controls.current_left_q7 = 128;
    controls.current_right_q7 = 128;
    if (!gp100_usb_playback_decode_packet(
            &controls,
            GP100_USB_AUDIO_PCM16_STEREO,
            packet,
            sizeof(packet),
            output,
            1u,
            &frames) ||
        frames != 1u || output[0] != 8388352 || output[1] != -8388608 ||
        controls.current_master_q7 != 127 ||
        controls.current_left_q7 != 127 ||
        controls.current_right_q7 != 127) {
        return 1;
    }
    return 0;
}

static int test_pcm32_mute_and_scaling(void)
{
    gp100_usb_playback_controls_t controls;
    const uint8_t packet[8] = {
        0x00u, 0xffu, 0xffu, 0x7fu,
        0x00u, 0x00u, 0x00u, 0x80u
    };
    int32_t output[2];
    size_t frames = 0u;

    gp100_usb_playback_controls_reset(&controls);
    controls.left_mute = true;
    controls.current_master_q7 = 64;
    controls.current_left_q7 = 128;
    controls.current_right_q7 = 64;
    if (!gp100_usb_playback_decode_packet(
            &controls,
            GP100_USB_AUDIO_PCM32_STEREO,
            packet,
            sizeof(packet),
            output,
            1u,
            &frames) ||
        frames != 1u || output[0] != 0 || output[1] != -2097152) {
        return 1;
    }
    return 0;
}

static int test_factory_q7_wrap(void)
{
    gp100_usb_playback_controls_t controls;
    const uint8_t packet[8] = {
        0xffu, 0xffu, 0xffu, 0x7fu,
        0xffu, 0xffu, 0xffu, 0x7fu
    };
    int32_t output[2];
    size_t frames = 0u;

    gp100_usb_playback_controls_reset(&controls);
    controls.current_master_q7 = 254;
    controls.current_left_q7 = 254;
    controls.current_right_q7 = 254;
    if (!gp100_usb_playback_decode_packet(
            &controls,
            GP100_USB_AUDIO_PCM32_STEREO,
            packet,
            sizeof(packet),
            output,
            1u,
            &frames) ||
        frames != 1u || output[0] != -522244 || output[1] != -522244) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    gp100_usb_playback_controls_t controls;
    const uint8_t packet[4] = {0u, 0u, 0u, 0u};
    int32_t output[2];
    size_t frames;

    gp100_usb_playback_controls_reset(&controls);
    if (gp100_usb_playback_decode_packet(
            NULL, GP100_USB_AUDIO_PCM16_STEREO,
            packet, sizeof(packet), output, 1u, &frames) ||
        gp100_usb_playback_decode_packet(
            &controls, (gp100_usb_audio_format_t)0,
            packet, sizeof(packet), output, 1u, &frames) ||
        gp100_usb_playback_decode_packet(
            &controls, GP100_USB_AUDIO_PCM16_STEREO,
            packet, 3u, output, 1u, &frames) ||
        gp100_usb_playback_decode_packet(
            &controls, GP100_USB_AUDIO_PCM16_STEREO,
            packet, sizeof(packet), output, 0u, &frames) ||
        !gp100_usb_playback_decode_packet(
            &controls, GP100_USB_AUDIO_PCM16_STEREO,
            NULL, 0u, NULL, 0u, &frames)) {
        return 1;
    }
    return 0;
}

static int test_capture_pcm16_and_slew(void)
{
    gp100_usb_capture_controls_t controls;
    const int32_t input[2] = {8388607, -8388608};
    uint8_t packet[4];
    size_t packet_bytes = 0u;

    gp100_usb_capture_controls_reset(&controls);
    controls.current_master_q7 = 127;
    controls.current_left_q7 = 127;
    controls.current_right_q7 = 127;
    controls.master_volume_8_8 = 0xff00u;
    controls.left_volume_8_8 = 0xff00u;
    controls.right_volume_8_8 = 0xff00u;
    if (!gp100_usb_capture_encode_packet(
            &controls,
            GP100_USB_AUDIO_PCM16_STEREO,
            input,
            1u,
            packet,
            sizeof(packet),
            &packet_bytes) ||
        packet_bytes != sizeof(packet) ||
        packet[0] != 0x01u || packet[1] != 0x7eu ||
        packet[2] != 0xfeu || packet[3] != 0x81u ||
        controls.current_master_q7 != 126 ||
        controls.current_left_q7 != 126 ||
        controls.current_right_q7 != 126) {
        return 1;
    }
    return 0;
}

static int test_capture_pcm32_and_mute(void)
{
    gp100_usb_capture_controls_t controls;
    const int32_t input[2] = {8388607, -8388608};
    uint8_t packet[8];
    size_t packet_bytes = 0u;

    gp100_usb_capture_controls_reset(&controls);
    controls.left_mute = true;
    controls.current_master_q7 = 128;
    controls.current_left_q7 = 128;
    controls.current_right_q7 = 128;
    if (!gp100_usb_capture_encode_packet(
            &controls,
            GP100_USB_AUDIO_PCM32_STEREO,
            input,
            1u,
            packet,
            sizeof(packet),
            &packet_bytes) ||
        packet_bytes != sizeof(packet) ||
        packet[0] != 0u || packet[1] != 0u ||
        packet[2] != 0u || packet[3] != 0u ||
        packet[4] != 0u || packet[5] != 0u ||
        packet[6] != 0u || packet[7] != 0x80u) {
        return 1;
    }
    return 0;
}

static int test_capture_factory_q7_wrap(void)
{
    gp100_usb_capture_controls_t controls;
    const int32_t input[2] = {8388607, 8388607};
    const uint8_t expected[8] = {
        0x00u, 0xfcu, 0x07u, 0xf8u,
        0x00u, 0xfcu, 0x07u, 0xf8u
    };
    uint8_t packet[8];
    size_t packet_bytes = 0u;
    size_t byte;

    gp100_usb_capture_controls_reset(&controls);
    controls.current_master_q7 = 254;
    controls.current_left_q7 = 254;
    controls.current_right_q7 = 254;
    if (!gp100_usb_capture_encode_packet(
            &controls,
            GP100_USB_AUDIO_PCM32_STEREO,
            input,
            1u,
            packet,
            sizeof(packet),
            &packet_bytes) ||
        packet_bytes != sizeof(packet)) {
        return 1;
    }
    for (byte = 0u; byte < sizeof(packet); ++byte) {
        if (packet[byte] != expected[byte]) {
            return 2;
        }
    }
    return 0;
}

static int test_capture_validation(void)
{
    gp100_usb_capture_controls_t controls;
    const int32_t input[2] = {0, 0};
    uint8_t packet[8];
    size_t packet_bytes;

    gp100_usb_capture_controls_reset(&controls);
    if (gp100_usb_capture_encode_packet(
            NULL, GP100_USB_AUDIO_PCM16_STEREO,
            input, 1u, packet, sizeof(packet), &packet_bytes) ||
        gp100_usb_capture_encode_packet(
            &controls, (gp100_usb_audio_format_t)0,
            input, 1u, packet, sizeof(packet), &packet_bytes) ||
        gp100_usb_capture_encode_packet(
            &controls, GP100_USB_AUDIO_PCM32_STEREO,
            input, 1u, packet, 7u, &packet_bytes) ||
        gp100_usb_capture_encode_packet(
            &controls, GP100_USB_AUDIO_PCM32_STEREO,
            input, SIZE_MAX, packet, sizeof(packet), &packet_bytes) ||
        !gp100_usb_capture_encode_packet(
            &controls, GP100_USB_AUDIO_PCM16_STEREO,
            NULL, 0u, NULL, 0u, &packet_bytes)) {
        return 1;
    }
    return 0;
}

int main(void)
{
    int result = test_volume_decoder();
    if (result != 0) {
        fprintf(stderr, "volume-decoder test failed: %d\n", result);
        return 10 + result;
    }
    result = test_pcm16_and_control_slew();
    if (result != 0) {
        fprintf(stderr, "PCM16/control test failed: %d\n", result);
        return 20 + result;
    }
    result = test_pcm32_mute_and_scaling();
    if (result != 0) {
        fprintf(stderr, "PCM32/mute test failed: %d\n", result);
        return 30 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 40 + result;
    }
    result = test_factory_q7_wrap();
    if (result != 0) {
        fprintf(stderr, "factory Q7 wrap test failed: %d\n", result);
        return 45 + result;
    }
    result = test_capture_pcm16_and_slew();
    if (result != 0) {
        fprintf(stderr, "capture PCM16 test failed: %d\n", result);
        return 50 + result;
    }
    result = test_capture_pcm32_and_mute();
    if (result != 0) {
        fprintf(stderr, "capture PCM32 test failed: %d\n", result);
        return 60 + result;
    }
    result = test_capture_factory_q7_wrap();
    if (result != 0) {
        fprintf(stderr, "capture factory Q7 wrap test failed: %d\n", result);
        return 65 + result;
    }
    result = test_capture_validation();
    if (result != 0) {
        fprintf(stderr, "capture validation test failed: %d\n", result);
        return 70 + result;
    }
    return 0;
}
