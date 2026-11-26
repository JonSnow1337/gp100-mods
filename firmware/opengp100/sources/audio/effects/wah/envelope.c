#include "envelope.h"

#include <math.h>

#define SAMPLE_RATE 44100.0f
#define MIN_CENTER_HZ 76.0f
#define MAX_CENTER_HZ 3200.0f
#define MAX_STOP_HZ 1300.0f

/* Behavioral fit to the published M82 limits. The attack/release times and
 * conversion from GP-100 sample level to detector drive are model choices. */
#define DETECTOR_ATTACK 0.01127477f  /* 2 ms at 44.1 kHz */
#define DETECTOR_RELEASE 0.00028341f /* 80 ms at 44.1 kHz */
#define KNOB_SMOOTHING 0.00452488f   /* 5 ms at 44.1 kHz */
#define FULL_SWEEP_PEAK 0.09f       /* Approximate DSP input at maximum Sens */
#define SWEEP_ONSET 0.078f

static float unit(float value)
{
    return fminf(1.0f, fmaxf(0.0f, value));
}

static gp100_envelope_settings_t settings_for_controls(const float controls[5])
{
    const float dry = unit(controls[0] / 99.0f);
    const float fx = unit(controls[1] / 99.0f);
    const float decay = unit(controls[2] / 99.0f);
    const float q = unit(controls[3] / 99.0f);
    const float sensitivity_backoff = 1.0f - unit(controls[4] / 99.0f);
    return (gp100_envelope_settings_t){
        .dry = 1.995262f * dry, /* +6 dB at maximum and unity at midpoint */
        .wet = fx,            /* Unity at maximum and -6 dB at midpoint */
        .stop_hz = MIN_CENTER_HZ * powf(MAX_STOP_HZ / MIN_CENTER_HZ, decay),
        .resonance = q,
        .sensitivity = powf(10.0f, -2.3f * sensitivity_backoff * sensitivity_backoff)
                     / FULL_SWEEP_PEAK,
    };
}

void gp100_envelope_reset(gp100_envelope_t *effect, const float controls[5])
{
    *effect = (gp100_envelope_t){.settings = settings_for_controls(controls)};
}

static float update_detector(gp100_envelope_t *effect, float left, float right)
{
    /* A linked detector keeps both channels on the same sweep. Their
     * filter histories and dry paths are independent. */
    const float peak = fmaxf(fabsf(left), fabsf(right));
    const float delta = peak - effect->detector;
    effect->detector += (delta > 0.0f ? DETECTOR_ATTACK : DETECTOR_RELEASE) * delta;
    return effect->detector;
}

static float bandpass(gp100_envelope_channel_t *channel, float input,
                      float g, float a1, float a2)
{
    /* Trapezoidal state-variable filter. Retain the integrator states as
     * the center frequency changes under the envelope. */
    const float high_input = input - channel->low_history;
    const float band = a1 * channel->band_history + a2 * high_input;
    channel->band_history = 2.0f * band - channel->band_history;
    channel->low_history += 2.0f * g * band;
    return band;
}

void gp100_envelope_process(gp100_envelope_t *effect, const float controls[5],
                            float *audio, size_t frames, bool enabled)
{
    if (!enabled) {
        if (effect->enabled) {
            gp100_envelope_reset(effect, controls);
        }
        return;
    }
    effect->enabled = true;
    const gp100_envelope_settings_t target = settings_for_controls(controls);
    gp100_envelope_settings_t *settings = &effect->settings;
    for (size_t i = 0; i < frames; ++i, audio += 2) {
        settings->dry += KNOB_SMOOTHING * (target.dry - settings->dry);
        settings->wet += KNOB_SMOOTHING * (target.wet - settings->wet);
        settings->stop_hz += KNOB_SMOOTHING * (target.stop_hz - settings->stop_hz);
        settings->resonance += KNOB_SMOOTHING * (target.resonance - settings->resonance);
        settings->sensitivity += KNOB_SMOOTHING * (target.sensitivity - settings->sensitivity);

        const float level = update_detector(effect, audio[0], audio[1]);
        const float sweep = unit((level * settings->sensitivity - SWEEP_ONSET)
                               / (1.0f - SWEEP_ONSET));
        const float center = settings->stop_hz
                           + (MAX_CENTER_HZ - settings->stop_hz) * sweep * sweep;
        const float x = (3.14159265f / SAMPLE_RATE) * center;
        const float x2 = x * x;
        /* tan(x) through x^5 is accurate over the published sweep and keeps
         * the sample loop independent of the general-purpose math library. */
        const float g = x * (1.0f + x2 * (1.0f / 3.0f + x2 * (2.0f / 15.0f)));
        const float damping = 1.176471f - 0.976471f * settings->resonance;
        const float a1 = 1.0f / (1.0f + g * (g + damping));
        const float a2 = g * a1;

        /* Match the reported center gain (+8 dB at minimum Q, up to
         * +18 dB at 76 Hz or +26 dB at 3.2 kHz at maximum Q). The damping
         * factor normalizes the band-pass before applying that gain. */
        const float position = (center - MIN_CENTER_HZ) / (MAX_CENTER_HZ - MIN_CENTER_HZ);
        const float peak_gain = 2.511886f + settings->resonance * settings->resonance *
            (5.431396f + 12.009341f * position);
        const float wet = settings->wet * damping * peak_gain;
        audio[0] = settings->dry * audio[0] + wet * bandpass(&effect->channel[0], audio[0], g, a1, a2);
        audio[1] = settings->dry * audio[1] + wet * bandpass(&effect->channel[1], audio[1], g, a1, a2);
    }
}
