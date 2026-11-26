#include "usb_audio_packet.h"

#include <string.h>

float gp100_usb_volume_db(uint16_t encoded_8_8)
{
    bool negative = (encoded_8_8 & 0x8000u) != 0u;
    uint16_t magnitude = encoded_8_8;
    float integer_part;
    float fractional_part;

    if (negative) {
        magnitude = (uint16_t)(0u - magnitude);
        magnitude |= 0x8000u;
    }
    integer_part = (float)((magnitude & 0x7fffu) >> 8u);
    fractional_part = (float)(magnitude & 0xffu) / 256.0f;
    return negative
        ? -integer_part - fractional_part
        : integer_part + fractional_part;
}

void gp100_usb_playback_controls_reset(
    gp100_usb_playback_controls_t *controls)
{
    if (controls != NULL) {
        memset(controls, 0, sizeof(*controls));
    }
}

void gp100_usb_capture_controls_reset(
    gp100_usb_capture_controls_t *controls)
{
    if (controls != NULL) {
        memset(controls, 0, sizeof(*controls));
    }
}

static int16_t playback_target_q7(uint16_t encoded_8_8)
{
    int volume_db = (int)gp100_usb_volume_db(encoded_8_8);

    return (int16_t)(127 - volume_db);
}

static int16_t capture_target_q7(uint16_t encoded_8_8)
{
    int volume_db = (int)gp100_usb_volume_db(encoded_8_8);

    return (int16_t)(127 + volume_db);
}

static void approach_target(int16_t *current, int16_t target)
{
    if (*current < target) {
        ++*current;
    }
    if (*current > target) {
        --*current;
    }
}

static int16_t read_i16_le(const uint8_t *bytes)
{
    uint16_t value = (uint16_t)bytes[0] |
                     (uint16_t)((uint16_t)bytes[1] << 8u);

    return (int16_t)value;
}

static int32_t read_i32_le(const uint8_t *bytes)
{
    uint32_t value = (uint32_t)bytes[0] |
                     ((uint32_t)bytes[1] << 8u) |
                     ((uint32_t)bytes[2] << 16u) |
                     ((uint32_t)bytes[3] << 24u);

    return (int32_t)value;
}

static int32_t arithmetic_shift_right_7(uint32_t bits)
{
    int32_t value;

    bits = (bits >> 7u) |
           ((bits & UINT32_C(0x80000000)) != 0u
                ? UINT32_C(0xfe000000)
                : 0u);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int32_t multiply_q7(int32_t sample, int16_t gain_q7)
{
    uint32_t product;

    /* Cortex-M7 MUL keeps the low 32 bits before the following ASR. */
    product = (uint32_t)sample * (uint32_t)(int32_t)gain_q7;
    return arithmetic_shift_right_7(product);
}

static int32_t apply_channel_controls(
    int32_t sample,
    bool muted,
    int16_t master_q7,
    int16_t channel_q7)
{
    if (muted) {
        return 0;
    }
    sample = multiply_q7(sample, master_q7);
    return multiply_q7(sample, channel_q7);
}

bool gp100_usb_playback_decode_packet(
    gp100_usb_playback_controls_t *controls,
    gp100_usb_audio_format_t format,
    const uint8_t *packet,
    size_t packet_bytes,
    int32_t *interleaved_stereo,
    size_t output_capacity_frames,
    size_t *output_frames)
{
    size_t bytes_per_frame;
    size_t frame_count;
    size_t frame;

    if (controls == NULL || output_frames == NULL) {
        return false;
    }
    if (format == GP100_USB_AUDIO_PCM16_STEREO) {
        bytes_per_frame = 4u;
    } else if (format == GP100_USB_AUDIO_PCM32_STEREO) {
        bytes_per_frame = 8u;
    } else {
        return false;
    }
    if (packet_bytes % bytes_per_frame != 0u) {
        return false;
    }
    frame_count = packet_bytes / bytes_per_frame;
    if (frame_count > output_capacity_frames ||
        (frame_count != 0u && (packet == NULL || interleaved_stereo == NULL))) {
        return false;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        const uint8_t *input = packet + frame * bytes_per_frame;
        int32_t left;
        int32_t right;

        if (format == GP100_USB_AUDIO_PCM16_STEREO) {
            left = (int32_t)read_i16_le(input) * 256;
            right = (int32_t)read_i16_le(input + 2u) * 256;
        } else {
            left = read_i32_le(input) >> 8;
            right = read_i32_le(input + 4u) >> 8;
        }
        interleaved_stereo[frame * 2u] = apply_channel_controls(
            left,
            controls->master_mute || controls->left_mute,
            controls->current_master_q7,
            controls->current_left_q7);
        interleaved_stereo[frame * 2u + 1u] = apply_channel_controls(
            right,
            controls->master_mute || controls->right_mute,
            controls->current_master_q7,
            controls->current_right_q7);
    }

    approach_target(
        &controls->current_master_q7,
        playback_target_q7(controls->master_volume_8_8));
    approach_target(
        &controls->current_left_q7,
        playback_target_q7(controls->left_volume_8_8));
    approach_target(
        &controls->current_right_q7,
        playback_target_q7(controls->right_volume_8_8));
    *output_frames = frame_count;
    return true;
}

static void write_i16_le(uint8_t *bytes, int16_t value)
{
    uint16_t bits = (uint16_t)value;

    bytes[0] = (uint8_t)bits;
    bytes[1] = (uint8_t)(bits >> 8u);
}

static void write_i32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
}

bool gp100_usb_capture_encode_packet(
    gp100_usb_capture_controls_t *controls,
    gp100_usb_audio_format_t format,
    const int32_t *interleaved_stereo,
    size_t frame_count,
    uint8_t *packet,
    size_t packet_capacity_bytes,
    size_t *packet_bytes)
{
    size_t bytes_per_frame;
    size_t required_bytes;
    size_t frame;

    if (controls == NULL || packet_bytes == NULL) {
        return false;
    }
    if (format == GP100_USB_AUDIO_PCM16_STEREO) {
        bytes_per_frame = 4u;
    } else if (format == GP100_USB_AUDIO_PCM32_STEREO) {
        bytes_per_frame = 8u;
    } else {
        return false;
    }
    if (frame_count > SIZE_MAX / bytes_per_frame) {
        return false;
    }
    required_bytes = frame_count * bytes_per_frame;
    if (required_bytes > packet_capacity_bytes ||
        (frame_count != 0u &&
         (interleaved_stereo == NULL || packet == NULL))) {
        return false;
    }

    for (frame = 0u; frame < frame_count; ++frame) {
        int32_t left = apply_channel_controls(
            interleaved_stereo[frame * 2u],
            controls->master_mute || controls->left_mute,
            controls->current_master_q7,
            controls->current_left_q7);
        int32_t right = apply_channel_controls(
            interleaved_stereo[frame * 2u + 1u],
            controls->master_mute || controls->right_mute,
            controls->current_master_q7,
            controls->current_right_q7);
        uint8_t *output = packet + frame * bytes_per_frame;

        if (format == GP100_USB_AUDIO_PCM16_STEREO) {
            write_i16_le(output, (int16_t)((uint32_t)left >> 8u));
            write_i16_le(output + 2u, (int16_t)((uint32_t)right >> 8u));
        } else {
            write_i32_le(output, (uint32_t)left << 8u);
            write_i32_le(output + 4u, (uint32_t)right << 8u);
        }
    }

    approach_target(
        &controls->current_master_q7,
        capture_target_q7(controls->master_volume_8_8));
    approach_target(
        &controls->current_left_q7,
        capture_target_q7(controls->left_volume_8_8));
    approach_target(
        &controls->current_right_q7,
        capture_target_q7(controls->right_volume_8_8));
    *packet_bytes = required_bytes;
    return true;
}
