#include "studio_comp.h"

#include "audio/dsp/audio_pcm.h"

#include <math.h>
#include <string.h>

#define SAMPLES_PER_MS 44.1f
#define DB_PER_LOG2 6.020599913f

static float fader_gain(float db)
{
    return db <= GP100_STUDIO_COMP_MUTE_DB ? 0.0f : exp2f(db / DB_PER_LOG2);
}

static float smoothing_step(float ms)
{
    /* The time marks a 63% move toward the new gain reduction. */
    return ms <= 0.0f ? 1.0f : -expm1f(-1.0f / (ms * SAMPLES_PER_MS));
}

static void configure(gp100_studio_comp_t *comp, const float controls[])
{
    memcpy(comp->controls, controls, sizeof(comp->controls));
    comp->wet_target = fader_gain(controls[GP100_STUDIO_COMP_WET]);
    comp->dry_target = fader_gain(controls[GP100_STUDIO_COMP_DRY]);
    comp->threshold_peak = GP100_EFFECT_DSP_FULL_SCALE *
                           exp2f(controls[GP100_STUDIO_COMP_THRESHOLD] / DB_PER_LOG2);
    comp->attack_step = smoothing_step(controls[GP100_STUDIO_COMP_ATTACK]);
    comp->release_step = smoothing_step(controls[GP100_STUDIO_COMP_RELEASE]);
    const float ratio = controls[GP100_STUDIO_COMP_RATIO];
    comp->ratio_slope = ratio >= GP100_STUDIO_COMP_INFINITE_RATIO
        ? 1.0f : 1.0f - 1.0f / fminf(ratio, GP100_STUDIO_COMP_MAX_RATIO);
    comp->knee_log2 = controls[GP100_STUDIO_COMP_KNEE] / DB_PER_LOG2;
}

void gp100_studio_comp_reset(gp100_studio_comp_t *comp,
                             const float controls[GP100_STUDIO_COMP_CONTROL_COUNT])
{
    *comp = (gp100_studio_comp_t){0};
    configure(comp, controls);
}

void gp100_studio_comp_process(gp100_studio_comp_t *comp,
                               const float controls[GP100_STUDIO_COMP_CONTROL_COUNT],
                               float *samples, size_t frames, bool enabled)
{
    if (!enabled) {
        comp->active = false;
        comp->reduction_log2 = 0.0f;
        return;
    }
    if (frames == 0u) {
        return;
    }
    if (memcmp(comp->controls, controls, sizeof(comp->controls)) != 0) {
        configure(comp, controls);
    }

    float wet = comp->active ? comp->wet_gain : comp->wet_target;
    float dry = comp->active ? comp->dry_gain : comp->dry_target;
    const float inverse_frames = 1.0f / (float)frames;
    const float wet_step = (comp->wet_target - wet) * inverse_frames;
    const float dry_step = (comp->dry_target - dry) * inverse_frames;
    const float inverse_threshold = 1.0f / comp->threshold_peak;
    const float half_knee = 0.5f * comp->knee_log2;
    const float knee_factor = half_knee > 0.0f ? 0.25f / half_knee : 0.0f;
    float reduction = comp->reduction_log2;

    for (size_t frame = 0u; frame < frames; ++frame, samples += 2) {
        const float peak = fmaxf(fabsf(samples[0]), fabsf(samples[1]));
        float target = 0.0f;
        if (peak > comp->threshold_peak && comp->ratio_slope > 0.0f) {
            const float excess = log2f(peak * inverse_threshold);
            /* The soft knee starts at threshold. It reaches full ratio
             * one knee width above threshold with a continuous slope. */
            target = comp->ratio_slope * (excess < comp->knee_log2
                ? knee_factor * excess * excess
                : excess - half_knee);
        }
        const float step = target > reduction ? comp->attack_step : comp->release_step;
        reduction += step * (target - reduction);
        if (target == 0.0f && reduction < 0.000001f) {
            reduction = 0.0f;
        }
        wet += wet_step;
        dry += dry_step;
        const float gain = dry + wet * (reduction > 0.0f ? exp2f(-reduction) : 1.0f);
        samples[0] *= gain;
        samples[1] *= gain;
    }
    comp->wet_gain = comp->wet_target;
    comp->dry_gain = comp->dry_target;
    comp->reduction_log2 = reduction;
    comp->active = true;
}
