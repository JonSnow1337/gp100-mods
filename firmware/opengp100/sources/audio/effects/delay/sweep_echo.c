#include "sweep_echo.h"

#include "audio/dsp/delay_line_internal.h"

#include "audio/effects/effect_catalog.h"

#include <math.h>
#include <string.h>

#define SWEEP_ECHO_CONTROL_GLIDE 0x1.db8cdap-13f

static const float free_sweep_steps[100] = {
    2.267599939e-06f, 2.267599939e-06f, 2.267599939e-06f,
    2.267599939e-06f, 2.267599939e-06f, 2.267599939e-06f,
    2.290049224e-06f, 2.312498509e-06f, 2.334947794e-06f,
    2.379846137e-06f, 2.424744707e-06f, 2.51454162e-06f,
    2.55944019e-06f, 2.671686389e-06f, 2.761483302e-06f,
    2.91862807e-06f, 3.030874268e-06f, 3.232917379e-06f,
    3.39006192e-06f, 3.637003601e-06f, 3.928843853e-06f,
    4.130887191e-06f, 4.490074843e-06f, 4.75946581e-06f,
    5.163552032e-06f, 5.477841569e-06f, 5.971724931e-06f,
    6.330913038e-06f, 6.914593087e-06f, 7.341128821e-06f,
    8.014605555e-06f, 8.732980859e-06f, 9.249313734e-06f,
    1.007993615e-05f, 1.066361619e-05f, 1.160648389e-05f,
    1.225751203e-05f, 1.331262592e-05f, 1.40534512e-05f,
    1.52208122e-05f, 1.645552038e-05f, 1.733104182e-05f,
    1.870044434e-05f, 1.964331204e-05f, 2.114741255e-05f,
    2.220252645e-05f, 2.384132131e-05f, 2.498623144e-05f,
    2.678217061e-05f, 2.803932875e-05f, 2.999241224e-05f,
    3.203529195e-05f, 3.344959623e-05f, 3.564962026e-05f,
    3.71761671e-05f, 3.955578723e-05f, 4.121703387e-05f,
    4.377624646e-05f, 4.552728569e-05f, 4.828854435e-05f,
    5.113959924e-05f, 5.30926809e-05f, 5.614577822e-05f,
    5.825600965e-05f, 6.15111494e-05f, 6.373361975e-05f,
    6.719080557e-05f, 6.957042933e-05f, 7.325210026e-05f,
    7.576641656e-05f, 7.965013356e-05f, 8.366855036e-05f,
    8.642980538e-05f, 9.069516091e-05f, 9.361356206e-05f,
    9.810340998e-05f, 0.0001011789573f, 0.0001059157439f,
    0.0001091484373f, 0.0001141321627f, 0.0001192730415f,
    0.0001227751345f, 0.0001281853911f, 0.0001318670693f,
    0.0001375242864f, 0.0001413855498f, 0.0001473121374f,
    0.0001513754542f, 0.0001575714559f, 0.0001618143433f,
    0.0001683246373f, 0.000174992063f, 0.0001795268035f,
    0.0001865085214f, 0.0001912453008f, 0.0001985188574f,
    0.0002034577046f, 0.0002110230853f, 0.0002161864104f,
    0.0002240885515f
};

static bool parameters_valid(uint8_t mix,
                             uint8_t feedback,
                             float time,
                             uint8_t depth,
                             float rate,
                             bool time_sync,
                             bool sweep_sync,
                             uint16_t tempo_period_ms)
{
    return mix <= 99u && feedback <= 99u && depth <= 99u &&
           time >= 0.0f && time <= 4000.0f && rate >= 0.0f && rate <= 99.0f &&
           (!(time_sync || sweep_sync) || tempo_period_ms != 0u);
}

static float sweep_step(float rate,
                        bool sweep_sync,
                        uint16_t tempo_period_ms)
{
    if (sweep_sync) {
        return 1.0f / ((float)tempo_period_ms * 44.1f *
                       gp100_effect_tempo_beats((size_t)rate));
    }
    return free_sweep_steps[(size_t)rate];
}

static float swept_delay(gp100_sweep_echo_t *echo,
                         size_t channel,
                         float input)
{
    gp100_sweep_echo_filter_t *filter = &echo->sweep_filter[channel];
    float delayed;
    float tangent;
    float squared;
    float denominator;
    float output;

    if (input > 1.7f) {
        input = 1.7f;
    } else if (input < -1.7f) {
        input = -1.7f;
    }
    gp100_delay_write(echo->delay_line[channel], GP100_SWEEP_ECHO_SAMPLES,
                           &echo->write_index[channel], &echo->delay_full[channel], input);
    delayed = gp100_delay_read_linear_valid(
        echo->delay_line[channel], GP100_SWEEP_ECHO_SAMPLES,
        echo->write_index[channel], echo->current_delay_samples, echo->delay_full[channel]);
    echo->feedback_sample[channel] = delayed;

    echo->sweep_phase[channel] += echo->sweep_direction[channel] *
                                  echo->current_sweep_step;
    if (echo->sweep_phase[channel] >= 1.0f) {
        echo->sweep_direction[channel] = -1.0f;
        echo->sweep_phase[channel] = 1.0f;
    } else if (echo->sweep_phase[channel] < 0.0f) {
        echo->sweep_direction[channel] = 1.0f;
        echo->sweep_phase[channel] = 0.0f;
    }
    tangent = tanf((echo->sweep_phase[channel] * 2900.0f *
                    echo->current_depth + 100.0f) *
                   3.1415927f / 44100.0f);
    squared = tangent * tangent;
    denominator = tangent + squared * 5.0f + 5.0f;
    output = delayed * (tangent / denominator) -
             filter->x[1] * (tangent / denominator) -
             filter->y[0] * ((squared - 1.0f) * 10.0f / denominator) -
             filter->y[1] * ((squared * 5.0f - tangent + 5.0f) /
                             denominator);
    filter->x[1] = filter->x[0];
    filter->x[0] = delayed;
    filter->y[1] = filter->y[0];
    filter->y[0] = output;
    return output * 4.0f;
}

static float mix_sample(float dry, float wet, float mix,
                        bool enabled, bool trail)
{
    if (mix < 0.5f) {
        return dry + wet * mix * 2.0f;
    }
    if (!enabled && trail) {
        return dry + wet;
    }
    return wet + dry * 2.0f * (1.0f - mix);
}

bool gp100_sweep_echo_reset(gp100_sweep_echo_t *echo,
                                 uint8_t mix,
                                 uint8_t feedback,
                                 float time,
                                 uint8_t depth,
                                 float rate,
                                 bool time_sync,
                                 bool sweep_sync,
                                 bool trail)
{
    if (echo == NULL ||
        !parameters_valid(mix, feedback, time, depth, rate,
                          time_sync, sweep_sync, 1u)) {
        return false;
    }
    memset(echo, 0, offsetof(gp100_sweep_echo_t, delay_line));
    echo->target_mix = echo->current_mix = (float)mix * 0.01f;
    echo->target_feedback = echo->current_feedback =
        (float)feedback * 0.01f;
    echo->target_depth = echo->current_depth = (float)depth * 0.01f;
    echo->current_delay_samples = gp100_effect_delay_samples((uint32_t)time, time_sync, 1u);
    echo->target_delay_samples = echo->current_delay_samples;
    echo->current_sweep_step = sweep_step(rate, sweep_sync, 1u);
    echo->target_sweep_step = echo->current_sweep_step;
    echo->sweep_direction[0] = 1.0f;
    echo->sweep_direction[1] = -1.0f;
    echo->sweep_phase[1] = 1.0f;
    echo->time_sync = time_sync;
    echo->sweep_sync = sweep_sync;
    echo->trail = trail;
    echo->configured = true;
    return true;
}

bool gp100_sweep_echo_process(gp100_sweep_echo_t *echo,
                                   uint8_t mix,
                                   uint8_t feedback,
                                   float time,
                                   uint8_t depth,
                                   float rate,
                                   bool time_sync,
                                   bool sweep_sync,
                                   bool trail,
                                   uint16_t tempo_period_ms,
                                   float *audio,
                                   size_t frames,
                                   bool enabled)
{
    float mix_delta;
    float feedback_delta;
    float time_delta;
    float depth_delta;
    float sweep_delta;
    size_t frame;

    if (echo == NULL || !echo->configured ||
        !parameters_valid(mix, feedback, time, depth, rate,
                          time_sync, sweep_sync, tempo_period_ms) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) {
        return false;
    }
    echo->target_mix = (float)mix * 0.01f;
    echo->target_feedback = (float)feedback * 0.01f;
    echo->target_delay_samples =
        gp100_effect_delay_samples((uint32_t)time, time_sync, tempo_period_ms);
    echo->target_depth = (float)depth * 0.01f;
    echo->target_sweep_step = sweep_step(rate, sweep_sync, tempo_period_ms);
    if (time_sync) {
        echo->current_delay_samples = echo->target_delay_samples;
    }
    if (sweep_sync) {
        echo->current_sweep_step = echo->target_sweep_step;
    }
    echo->time_sync = time_sync;
    echo->sweep_sync = sweep_sync;
    echo->trail = trail;
    mix_delta = (echo->target_mix - echo->current_mix) *
                SWEEP_ECHO_CONTROL_GLIDE;
    feedback_delta = (echo->target_feedback - echo->current_feedback) *
                     SWEEP_ECHO_CONTROL_GLIDE;
    time_delta = (echo->target_delay_samples - echo->current_delay_samples) *
                 SWEEP_ECHO_CONTROL_GLIDE;
    depth_delta = (echo->target_depth - echo->current_depth) *
                  SWEEP_ECHO_CONTROL_GLIDE;
    sweep_delta = (echo->target_sweep_step - echo->current_sweep_step) *
                  SWEEP_ECHO_CONTROL_GLIDE;

    if (frames != 0u && !enabled && !trail && echo->rings_active) {
        memset(echo->delay_full, 0, sizeof(echo->delay_full));
        memset(echo->write_index, 0, sizeof(echo->write_index));
        memset(echo->feedback_sample, 0, sizeof(echo->feedback_sample));
    }
    if (frames != 0u) {
        echo->rings_active = enabled || trail;
    }

    for (frame = 0u; frame < frames; ++frame) {
        float input[2] = {audio[frame * 2u], audio[frame * 2u + 1u]};
        size_t channel;

        echo->current_mix += mix_delta;
        echo->current_feedback += feedback_delta;
        echo->current_depth += depth_delta;
        if (!time_sync) {
            echo->current_delay_samples += time_delta;
        }
        if (!sweep_sync) {
            echo->current_sweep_step += sweep_delta;
        }
        if (!enabled && !trail) {
            continue;
        }
        for (channel = 0u; channel < 2u; ++channel) {
            float write_value = echo->feedback_sample[channel] *
                                echo->current_feedback;
            float wet;

            if (enabled) {
                write_value += input[channel];
            }
            wet = swept_delay(echo, channel, write_value) * 0.5f;
            audio[frame * 2u + channel] =
                mix_sample(input[channel], wet, echo->current_mix,
                           enabled, trail);
        }
    }
    return true;
}
