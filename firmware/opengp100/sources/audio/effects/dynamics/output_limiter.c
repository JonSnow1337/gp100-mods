#include "output_limiter.h"

#include <math.h>

#define PEAK_QUEUE_SIZE (GP100_LIMITER_WINDOW_FRAMES + 1u)

void gp100_output_limiter_configure(gp100_output_limiter_config_t *config,
                                   bool enabled, int16_t threshold_db,
                                   int16_t ceiling_db, int16_t release_db_per_sec,
                                   float full_scale)
{
    if (!enabled) {
        config->enabled = false;
        return;
    }
    if (config->enabled && config->threshold_db == threshold_db &&
        config->ceiling_db == ceiling_db &&
        config->release_db_per_sec == release_db_per_sec &&
        config->full_scale == full_scale) {
        return;
    }
    const float threshold = powf(10.0f, (float)threshold_db / 20.0f);
    const float ceiling = powf(10.0f, (float)ceiling_db / 20.0f);
    config->threshold = threshold * full_scale;
    config->ceiling = ceiling * full_scale;
    config->makeup = ceiling / threshold;
    config->release_multiplier =
        powf(10.0f, (float)release_db_per_sec / (20.0f * GP100_LIMITER_SAMPLE_RATE));
    config->threshold_db = threshold_db;
    config->ceiling_db = ceiling_db;
    config->release_db_per_sec = release_db_per_sec;
    config->full_scale = full_scale;
    config->enabled = true;
}

static float stereo_peak(float left, float right)
{
    left = fabsf(left);
    right = fabsf(right);
    return left > right ? left : right;
}

/* Keep a monotonic queue so tracking the largest peak in the window does
 * not require rescanning the delay line for each stereo frame. */
static float window_peak(gp100_output_limiter_t *limiter, float peak)
{
    unsigned head = limiter->peak_head;
    unsigned tail = limiter->peak_tail;
    const uint32_t frame = limiter->frame++;

    if (head != tail &&
        frame - limiter->peaks[head].frame > GP100_LIMITER_LOOKAHEAD_FRAMES) {
        head = (head + 1u) % PEAK_QUEUE_SIZE;
    }
    while (head != tail) {
        const unsigned previous = (tail + PEAK_QUEUE_SIZE - 1u) % PEAK_QUEUE_SIZE;
        if (limiter->peaks[previous].level > peak) {
            break;
        }
        tail = previous;
    }
    limiter->peaks[tail] = (gp100_output_limiter_peak_t){peak, frame};
    limiter->peak_head = head;
    limiter->peak_tail = (tail + 1u) % PEAK_QUEUE_SIZE;
    return limiter->peaks[head].level;
}

static float clamp_sample(float sample, float ceiling)
{
    return sample > ceiling ? ceiling : sample < -ceiling ? -ceiling : sample;
}

static float finite_sample(float sample)
{
    return isfinite(sample) ? sample : 0.0f;
}

static void delay_stereo(gp100_output_limiter_t *limiter, float left, float right)
{
    const unsigned position = limiter->position;
    limiter->delay[position][0] = left;
    limiter->delay[position][1] = right;
    limiter->position = (position + 1u) % GP100_LIMITER_WINDOW_FRAMES;
}

void gp100_output_limiter_process(gp100_output_limiter_t *limiter,
                                 const gp100_output_limiter_config_t *config,
                                 float *samples, size_t frames)
{
    if (!config->enabled && !limiter->active) {
        /* Leave the live signal untouched and keep history for a quiet toggle. */
        for (size_t frame = 0u; frame < frames; ++frame, samples += 2) {
            delay_stereo(limiter, finite_sample(samples[0]), finite_sample(samples[1]));
        }
        limiter->bypass_history = true;
        return;
    }
    if (!limiter->active) {
        limiter->peak_head = 0u;
        limiter->peak_tail = 0u;
        for (size_t i = 0u; i < GP100_LIMITER_WINDOW_FRAMES; ++i) {
            limiter->gain_history[i] = 1.0f;
        }
        limiter->gain_sum = GP100_LIMITER_WINDOW_FRAMES;
        limiter->gain = 1.0f;
        limiter->wet_frames = limiter->bypass_history ? 0u : GP100_LIMITER_LOOKAHEAD_FRAMES;
        limiter->active = true;
    }

    for (size_t frame = 0u; frame < frames; ++frame, samples += 2) {
        const float left = finite_sample(samples[0]);
        const float right = finite_sample(samples[1]);
        const float peak = window_peak(limiter, stereo_peak(left, right));
        const float required_gain = peak > config->threshold
            ? config->threshold / peak : 1.0f;
        const float released_gain = limiter->gain * config->release_multiplier;
        limiter->gain = required_gain < released_gain ? required_gain : released_gain;

        /* Every gain in this moving average saw the delayed frame in its
         * peak window. Averaging gives a smooth attack without overshoot. */
        const unsigned position = limiter->position;
        limiter->gain_sum += (double)limiter->gain - limiter->gain_history[position];
        limiter->gain_history[position] = limiter->gain;
        delay_stereo(limiter, left, right);

        const float gain = config->makeup *
            (float)(limiter->gain_sum / GP100_LIMITER_WINDOW_FRAMES);
        float output_left = limiter->delay[limiter->position][0] * gain;
        float output_right = limiter->delay[limiter->position][1] * gain;

        if (config->enabled) {
            if (limiter->wet_frames < GP100_LIMITER_LOOKAHEAD_FRAMES) {
                ++limiter->wet_frames;
            }
        } else if (limiter->wet_frames != 0u) {
            --limiter->wet_frames;
        }
        if (limiter->wet_frames < GP100_LIMITER_LOOKAHEAD_FRAMES) {
            const float wet = (float)limiter->wet_frames / GP100_LIMITER_LOOKAHEAD_FRAMES;
            output_left = left + (output_left - left) * wet;
            output_right = right + (output_right - right) * wet;
        }

        if (config->enabled) {
            /* Enforce the ceiling during a toggle or live settings change, when
             * the smoothing window can contain gains from the previous settings. */
            const float output_peak = stereo_peak(output_left, output_right);
            if (output_peak > config->ceiling) {
                const float ceiling_gain = config->ceiling / output_peak;
                output_left *= ceiling_gain;
                output_right *= ceiling_gain;
            }
            output_left = clamp_sample(output_left, config->ceiling);
            output_right = clamp_sample(output_right, config->ceiling);
        }
        samples[0] = output_left;
        samples[1] = output_right;
    }
    if (!config->enabled && limiter->wet_frames == 0u) {
        limiter->active = false;
        limiter->bypass_history = true;
    }
}
