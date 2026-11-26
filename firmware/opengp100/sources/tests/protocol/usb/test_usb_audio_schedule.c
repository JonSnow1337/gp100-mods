#include "protocol/usb/usb_audio_schedule.h"

#include <stdio.h>

static int check_cycle(
    gp100_usb_audio_schedule_t *schedule,
    uint32_t sample_rate,
    uint16_t period,
    unsigned expected_total)
{
    unsigned total = 0u;
    uint16_t index;

    for (index = 0u; index < period; ++index) {
        uint8_t frames;

        if (!gp100_usb_audio_next_frame_count(
                schedule, sample_rate, &frames)) {
            return 1;
        }
        total += frames;
    }
    if (total != expected_total || schedule->index != 0u) {
        return 2;
    }
    return 0;
}

static int check_artifact_cycle(
    FILE *artifact,
    gp100_usb_audio_schedule_t *schedule,
    uint32_t sample_rate,
    long offset,
    uint16_t period)
{
    uint16_t index;

    gp100_usb_audio_schedule_reset(schedule);
    if (fseek(artifact, offset, SEEK_SET) != 0) {
        return 1;
    }
    for (index = 0u; index < period; ++index) {
        int artifact_frames = fgetc(artifact);
        uint8_t reference_frames;

        if (artifact_frames == EOF ||
            !gp100_usb_audio_next_frame_count(
                schedule, sample_rate, &reference_frames) ||
            reference_frames != (uint8_t)artifact_frames) {
            return 2;
        }
    }
    return 0;
}

static int test_cycles(void)
{
    gp100_usb_audio_schedule_t schedule;
    uint8_t frames;

    gp100_usb_audio_schedule_reset(&schedule);
    if (check_cycle(&schedule, 44100u, 80u, 441u) != 0 ||
        check_cycle(&schedule, 88200u, 40u, 441u) != 0 ||
        check_cycle(&schedule, 176400u, 20u, 441u) != 0 ||
        check_cycle(&schedule, 48000u, 1u, 6u) != 0 ||
        check_cycle(&schedule, 96000u, 1u, 12u) != 0 ||
        check_cycle(&schedule, 192000u, 1u, 24u) != 0 ||
        !gp100_usb_audio_next_frame_count(
            &schedule, 12345u, &frames) ||
        frames != 5u) {
        return 1;
    }
    return 0;
}

static int test_rate_change_reset(void)
{
    gp100_usb_audio_schedule_t schedule;
    uint8_t frames;

    gp100_usb_audio_schedule_reset(&schedule);
    if (!gp100_usb_audio_next_frame_count(
            &schedule, 44100u, &frames) || frames != 5u ||
        !gp100_usb_audio_next_frame_count(
            &schedule, 44100u, &frames) || frames != 6u ||
        !gp100_usb_audio_next_frame_count(
            &schedule, 88200u, &frames) || frames != 11u ||
        schedule.index != 1u ||
        gp100_usb_audio_next_frame_count(NULL, 44100u, &frames) ||
        gp100_usb_audio_next_frame_count(
            &schedule, 44100u, NULL)) {
        return 1;
    }
    return 0;
}

static int test_against_artifact(const char *artifact_path)
{
    gp100_usb_audio_schedule_t schedule;
    FILE *artifact = fopen(artifact_path, "rb");

    if (artifact == NULL) {
        return 1;
    }
    if (check_artifact_cycle(
            artifact, &schedule, 44100u, 0x1cbd7fL, 80u) != 0 ||
        check_artifact_cycle(
            artifact, &schedule, 88200u, 0x1cbdcfL, 40u) != 0 ||
        check_artifact_cycle(
            artifact, &schedule, 176400u, 0x1cbdf7L, 20u) != 0 ||
        check_artifact_cycle(
            artifact, &schedule, 48000u, 0x1cbd78L, 1u) != 0 ||
        check_artifact_cycle(
            artifact, &schedule, 96000u, 0x1cbd79L, 1u) != 0 ||
        check_artifact_cycle(
            artifact, &schedule, 192000u, 0x1cbd7aL, 1u) != 0) {
        fclose(artifact);
        return 2;
    }
    fclose(artifact);
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s APP_SDRAM_BIN\n", argv[0]);
        return 1;
    }
    result = test_cycles();
    if (result != 0) {
        fprintf(stderr, "cycle test failed: %d\n", result);
        return 10 + result;
    }
    result = test_rate_change_reset();
    if (result != 0) {
        fprintf(stderr, "rate-change test failed: %d\n", result);
        return 20 + result;
    }
    result = test_against_artifact(argv[1]);
    if (result != 0) {
        fprintf(stderr, "artifact test failed: %d\n", result);
        return 30 + result;
    }
    return 0;
}
