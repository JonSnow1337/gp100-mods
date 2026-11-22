#include "audio_output.h"
#include "protocol/usb/usb_audio_level.h"

#include <math.h>

#define GP100_AUDIO_OUTPUT_MAX_PERCENT 100u
#define GP100_AUDIO_INPUT_MIN_DB (-20)
#define GP100_AUDIO_INPUT_MAX_DB 20
#define GP100_AUDIO_INPUT_MAX_GAIN 10.0f
#define GP100_AUDIO_INPUT_GAIN_STEP 0.01f

void gp100_audio_output_level_reset(
    gp100_audio_output_level_t *level)
{
    if (level != NULL) {
        level->current_percent = 0.0f;
        level->target_percent = GP100_AUDIO_OUTPUT_MAX_PERCENT;
    }
}

bool gp100_audio_output_level_set_target(
    gp100_audio_output_level_t *level,
    uint8_t target_percent)
{
    if (level == NULL ||
        target_percent > GP100_AUDIO_OUTPUT_MAX_PERCENT) {
        return false;
    }
    level->target_percent = target_percent;
    return true;
}

bool gp100_audio_output_level_apply(
    gp100_audio_output_level_t *level,
    float *interleaved_stereo,
    size_t frame_count)
{
    float target;
    float scale;
    size_t sample;

    if (level == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        level->target_percent > GP100_AUDIO_OUTPUT_MAX_PERCENT ||
        level->current_percent < 0.0f ||
        level->current_percent >
            (float)GP100_AUDIO_OUTPUT_MAX_PERCENT ||
        level->current_percent != level->current_percent) {
        return false;
    }
    target = (float)level->target_percent;
    if (level->current_percent != target) {
        if (level->current_percent <= target) {
            level->current_percent += 1.0f;
        } else {
            level->current_percent -= 1.0f;
        }
    }
    scale = level->current_percent /
            (float)GP100_AUDIO_OUTPUT_MAX_PERCENT;
    for (sample = 0u; sample < frame_count * 2u; ++sample) {
        interleaved_stereo[sample] *= scale;
    }
    return true;
}

void gp100_audio_volume_pedal_reset(gp100_audio_volume_pedal_t *pedal)
{
    if (pedal != NULL) {
        pedal->current_gain = 0u;
        pedal->target_percent = GP100_AUDIO_OUTPUT_MAX_PERCENT;
    }
}

static bool step_integer_gain(gp100_audio_volume_pedal_t *level,
                              const float *interleaved_stereo,
                              size_t frame_count)
{
    unsigned target;

    if (level == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        level->target_percent > GP100_AUDIO_OUTPUT_MAX_PERCENT) {
        return false;
    }
    target = level->target_percent * UINT8_MAX / GP100_AUDIO_OUTPUT_MAX_PERCENT;
    if (level->current_gain < target) {
        ++level->current_gain;
    } else if (level->current_gain > target) {
        --level->current_gain;
    }
    return true;
}

static bool apply_integer_gain(gp100_audio_volume_pedal_t *level,
                               float *interleaved_stereo,
                               size_t frame_count,
                               float divisor)
{
    if (!step_integer_gain(level, interleaved_stereo, frame_count)) {
        return false;
    }
    const float scale = (float)level->current_gain / divisor;
    size_t sample;
    for (sample = 0u; sample < frame_count * 2u; ++sample) {
        interleaved_stereo[sample] *= scale;
    }
    return true;
}

bool gp100_audio_volume_pedal_apply(
    gp100_audio_volume_pedal_t *pedal,
    float *interleaved_stereo,
    size_t frame_count)
{
    return apply_integer_gain(pedal, interleaved_stereo, frame_count, UINT8_MAX);
}

bool gp100_audio_volume_pedal_apply_pcm(
    gp100_audio_volume_pedal_t *pedal,
    float *interleaved_stereo,
    size_t frame_count)
{
    if (!step_integer_gain(pedal, interleaved_stereo, frame_count)) {
        return false;
    }
    for (size_t sample = 0u; sample < frame_count * 2u; ++sample) {
        const float scaled = interleaved_stereo[sample] * 0x1p23f;
        int32_t pcm;
        if (!isfinite(scaled)) {
            pcm = 0;
        } else if (scaled >= 0x1p31f) {
            pcm = INT32_MAX;
        } else if (scaled <= -0x1p31f) {
            pcm = INT32_MIN;
        } else {
            pcm = (int32_t)scaled;
        }
        /* The Cortex-M MUL keeps the low 32 bits. Apply the signed division
         * to that bit pattern without relying on signed overflow in C. */
        const uint32_t bits = (uint32_t)pcm * pedal->current_gain;
        const int32_t product = bits <= INT32_MAX ? (int32_t)bits
            : INT32_MIN + (int32_t)(bits - UINT32_C(0x80000000));
        interleaved_stereo[sample] = (float)(product / 255) * 0x1p-23f;
    }
    return true;
}

void gp100_audio_master_level_reset(gp100_audio_master_level_t *level)
{
    gp100_audio_volume_pedal_reset(level);
}

bool gp100_audio_master_level_apply(
    gp100_audio_master_level_t *level,
    float *interleaved_stereo,
    size_t frame_count)
{
    return apply_integer_gain(level, interleaved_stereo, frame_count, 256.0f);
}

void gp100_audio_input_level_reset(
    gp100_audio_input_level_t *level)
{
    if (level != NULL) {
        level->current_gain = 0.0f;
        level->target_db = 0;
    }
}

bool gp100_audio_input_level_set_target(
    gp100_audio_input_level_t *level,
    int16_t target_db)
{
    if (level == NULL || target_db < GP100_AUDIO_INPUT_MIN_DB ||
        target_db > GP100_AUDIO_INPUT_MAX_DB) {
        return false;
    }
    level->target_db = target_db;
    return true;
}

bool gp100_audio_input_level_apply(
    gp100_audio_input_level_t *level,
    float *interleaved_stereo,
    size_t frame_count)
{
    float target_gain;
    size_t frame;

    if (level == NULL ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u ||
        level->target_db < GP100_AUDIO_INPUT_MIN_DB ||
        level->target_db > GP100_AUDIO_INPUT_MAX_DB ||
        level->current_gain < 0.0f ||
        level->current_gain > GP100_AUDIO_INPUT_MAX_GAIN ||
        level->current_gain != level->current_gain ||
        !gp100_usb_audio_gain_for_db(
            level->target_db, &target_gain)) {
        return false;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        const size_t sample = frame * 2u;
        float left = interleaved_stereo[sample] * level->current_gain;
        float right = interleaved_stereo[sample + 1u] * level->current_gain;

        if (left > 1.0f) {
            left = 1.0f;
        } else if (left < -1.0f) {
            left = -1.0f;
        }
        if (right > 1.0f) {
            right = 1.0f;
        } else if (right < -1.0f) {
            right = -1.0f;
        }
        interleaved_stereo[sample] = left;
        interleaved_stereo[sample + 1u] = right;

        /* The factory performs two comparisons, preserving its near-target
         * no-progress quirk when the remaining difference is below 0.01. */
        if (level->current_gain < target_gain) {
            level->current_gain += GP100_AUDIO_INPUT_GAIN_STEP;
        }
        if (level->current_gain > target_gain) {
            level->current_gain -= GP100_AUDIO_INPUT_GAIN_STEP;
        }
    }
    return true;
}
