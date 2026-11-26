#include "audio/engine/audio_route.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static gp100_audio_route_buffers_t make_buffers(
    int32_t *live,
    const int32_t *usb,
    const int32_t *pre_looper,
    const int32_t *rhythm,
    float *converted)
{
    gp100_audio_route_buffers_t buffers;

    buffers.live_stereo = live;
    buffers.usb_playback_stereo = usb;
    buffers.pre_looper_stereo = pre_looper;
    buffers.rhythm_mono = rhythm;
    buffers.float_stereo = converted;
    return buffers;
}

static int converted_matches(
    const int32_t *integer,
    const float *converted,
    size_t sample_count)
{
    size_t sample;

    for (sample = 0u; sample < sample_count; ++sample) {
        if (converted[sample] !=
            (float)integer[sample] *
                GP100_AUDIO_ROUTE_INT_TO_FLOAT) {
            return 0;
        }
    }
    return 1;
}

static int test_mute_route(void)
{
    int32_t live[4] = {101, 102, 103, 104};
    const int32_t usb[4] = {-11, 12, -13, 14};
    float converted[4] = {0.0f};
    gp100_audio_route_buffers_t buffers =
        make_buffers(live, usb, NULL, NULL, converted);

    if (!gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_TUNER_MUTE, &buffers, 2u) ||
        memcmp(live, usb, sizeof(live)) != 0 ||
        !converted_matches(live, converted, 4u)) {
        return 1;
    }
    return 0;
}

static int test_processed_route(void)
{
    int32_t live[4] = {10, -20, 30, -40};
    const int32_t usb[4] = {1, 2, 3, 4};
    const int32_t pre_looper[4] = {100, 200, 300, 400};
    const int32_t rhythm[2] = {5, -6};
    const int32_t expected[4] = {131, 202, 309, 340};
    float converted[4] = {0.0f};
    gp100_audio_route_buffers_t buffers = make_buffers(
        live, usb, pre_looper, rhythm, converted);

    if (!gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_NORMAL, &buffers, 2u) ||
        memcmp(live, expected, sizeof(live)) != 0 ||
        !converted_matches(live, converted, 4u)) {
        return 1;
    }
    return 0;
}

static int test_bypass_and_reserved_routes(void)
{
    const gp100_audio_route_state_t routes[] = {
        GP100_AUDIO_ROUTE_TUNER_BYPASS,
        GP100_AUDIO_ROUTE_RESERVED_3,
        GP100_AUDIO_ROUTE_RESERVED_4,
        GP100_AUDIO_ROUTE_RESERVED_5
    };
    const int32_t usb[2] = {7, -9};
    const int32_t rhythm[1] = {3};
    const int32_t expected[2] = {18, 17};
    size_t route;

    for (route = 0u; route < sizeof(routes) / sizeof(routes[0]); ++route) {
        int32_t live[2] = {5, 20};
        float converted[2] = {0.0f};
        gp100_audio_route_buffers_t buffers =
            make_buffers(live, usb, NULL, rhythm, converted);

        if (!gp100_audio_route_mix(routes[route], &buffers, 1u) ||
            memcmp(live, expected, sizeof(live)) != 0 ||
            !converted_matches(live, converted, 2u)) {
            return (int)route + 1;
        }
    }
    return 0;
}

static int test_wrapping_arithmetic(void)
{
    int32_t live[2] = {INT32_MAX, INT32_MIN};
    const int32_t usb[2] = {1, -1};
    const int32_t pre_looper[2] = {0, 0};
    const int32_t rhythm[1] = {0};
    const int32_t expected[2] = {INT32_MIN, INT32_MAX};
    float converted[2] = {0.0f};
    gp100_audio_route_buffers_t buffers = make_buffers(
        live, usb, pre_looper, rhythm, converted);

    if (!gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_NORMAL, &buffers, 1u) ||
        memcmp(live, expected, sizeof(live)) != 0 ||
        !converted_matches(live, converted, 2u)) {
        return 1;
    }
    return 0;
}

static int test_validation(void)
{
    const float factory_scale = GP100_AUDIO_ROUTE_INT_TO_FLOAT;
    uint32_t factory_scale_bits;
    int32_t live[2] = {0, 0};
    const int32_t usb[2] = {0, 0};
    const int32_t pre_looper[2] = {0, 0};
    const int32_t rhythm[1] = {0};
    float converted[2] = {0.0f};
    gp100_audio_route_buffers_t buffers = make_buffers(
        live, usb, pre_looper, rhythm, converted);

    memcpy(&factory_scale_bits, &factory_scale, sizeof(factory_scale_bits));
    if (factory_scale_bits != UINT32_C(0x340d0cc1)) {
        return 1;
    }
    if (!gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_TUNER_MUTE, NULL, 0u) ||
        gp100_audio_route_mix(
            (gp100_audio_route_state_t)6, &buffers, 0u)) {
        return 2;
    }
    if (gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_TUNER_MUTE, NULL, 1u)) {
        return 3;
    }
    buffers.pre_looper_stereo = NULL;
    if (gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_NORMAL, &buffers, 1u)) {
        return 4;
    }
    buffers.pre_looper_stereo = pre_looper;
    buffers.rhythm_mono = NULL;
    if (gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_TUNER_BYPASS, &buffers, 1u)) {
        return 5;
    }
    buffers.rhythm_mono = rhythm;
    buffers.float_stereo = NULL;
    if (gp100_audio_route_mix(
            GP100_AUDIO_ROUTE_TUNER_MUTE, &buffers, 1u)) {
        return 6;
    }
    return 0;
}

int main(void)
{
    int result = test_mute_route();

    if (result != 0) {
        fprintf(stderr, "mute-route test failed: %d\n", result);
        return 10 + result;
    }
    result = test_processed_route();
    if (result != 0) {
        fprintf(stderr, "processed-route test failed: %d\n", result);
        return 20 + result;
    }
    result = test_bypass_and_reserved_routes();
    if (result != 0) {
        fprintf(stderr, "bypass/reserved-route test failed: %d\n", result);
        return 30 + result;
    }
    result = test_wrapping_arithmetic();
    if (result != 0) {
        fprintf(stderr, "wrapping-arithmetic test failed: %d\n", result);
        return 40 + result;
    }
    result = test_validation();
    if (result != 0) {
        fprintf(stderr, "validation test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
