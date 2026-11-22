#include "tube_clipper.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct tube_biquad_coefficients {
    float b0, b1, b2, a1, a2;
} tube_biquad_coefficients_t;

typedef struct tube_tone_coefficients {
    float b0, b1, b2, b3, a0, a1, a2, a3;
} tube_tone_coefficients_t;

/* The factory indexes 256-entry curves by raw * 2.55. These are the 100
 * entries reachable through the integer-valued product controls. */
static const float bass_treble_curve[100] = {
    0.000996f, 0.000996f, 0.002988f, 0.004980f, 0.007968f,
    0.009960f, 0.012948f, 0.014940f, 0.017928f, 0.019920f,
    0.022908f, 0.025897f, 0.027889f, 0.030877f, 0.032869f,
    0.035858f, 0.037850f, 0.040839f, 0.042832f, 0.045822f,
    0.048813f, 0.050808f, 0.053801f, 0.055798f, 0.058795f,
    0.060795f, 0.063798f, 0.065802f, 0.068814f, 0.070826f,
    0.073850f, 0.076885f, 0.078915f, 0.081971f, 0.084018f,
    0.087104f, 0.089174f, 0.092300f, 0.094400f, 0.097579f,
    0.100797f, 0.102966f, 0.106263f, 0.108493f, 0.111890f,
    0.114195f, 0.117720f, 0.120120f, 0.123804f, 0.126321f,
    0.130200f, 0.134217f, 0.136980f, 0.141267f, 0.144227f,
    0.148840f, 0.152038f, 0.157042f, 0.160526f, 0.165996f,
    0.171787f, 0.175843f, 0.182245f, 0.186743f, 0.193865f,
    0.198881f, 0.206844f, 0.212465f, 0.221407f, 0.227731f,
    0.237807f, 0.248645f, 0.256328f, 0.268591f, 0.277290f,
    0.291186f, 0.301049f, 0.316810f, 0.328001f, 0.345885f,
    0.365180f, 0.378881f, 0.400773f, 0.416314f, 0.441139f,
    0.458755f, 0.486882f, 0.506832f, 0.538668f, 0.561236f,
    0.597226f, 0.635939f, 0.663352f, 0.707019f, 0.737917f,
    0.787099f, 0.821871f, 0.877176f, 0.916246f, 0.978336f
};

static const float gain_curve[100] = {
    0.000400f, 0.000500f, 0.000954f, 0.001190f, 0.001943f,
    0.002679f, 0.003333f, 0.003969f, 0.004823f, 0.005859f,
    0.007212f, 0.008466f, 0.009102f, 0.010056f, 0.010692f,
    0.011647f, 0.012283f, 0.013238f, 0.013875f, 0.014832f,
    0.015789f, 0.016428f, 0.017389f, 0.018031f, 0.018996f,
    0.019642f, 0.020615f, 0.021267f, 0.022251f, 0.022912f,
    0.023912f, 0.024925f, 0.025609f, 0.026649f, 0.027354f,
    0.028431f, 0.029165f, 0.030292f, 0.031064f, 0.032257f,
    0.033498f, 0.034356f, 0.035696f, 0.036628f, 0.038093f,
    0.039119f, 0.040742f, 0.041886f, 0.043705f, 0.044994f,
    0.047055f, 0.049286f, 0.050879f, 0.053443f, 0.055280f,
    0.058246f, 0.060377f, 0.063826f, 0.066307f, 0.070331f,
    0.074750f, 0.077935f, 0.083106f, 0.086836f, 0.092891f,
    0.097258f, 0.104347f, 0.109458f, 0.117748f, 0.123722f,
    0.133405f, 0.144025f, 0.151665f, 0.164031f, 0.172919f,
    0.187286f, 0.197600f, 0.214254f, 0.226194f, 0.245450f,
    0.266432f, 0.281446f, 0.305607f, 0.322873f, 0.350623f,
    0.370428f, 0.402218f, 0.424877f, 0.461203f, 0.487062f,
    0.528466f, 0.573197f, 0.604978f, 0.655763f, 0.691802f,
    0.749323f, 0.790093f, 0.855091f, 0.901110f, 0.974392f
};

static const float volume_curve[100] = {
    0.000000f, 0.000500f, 0.002884f, 0.004474f, 0.006859f,
    0.008448f, 0.010833f, 0.012422f, 0.014807f, 0.016396f,
    0.018781f, 0.021165f, 0.022755f, 0.025140f, 0.026730f,
    0.029115f, 0.030705f, 0.033090f, 0.034681f, 0.037068f,
    0.039456f, 0.041048f, 0.043439f, 0.045033f, 0.047428f,
    0.049026f, 0.051428f, 0.053031f, 0.055442f, 0.057053f,
    0.059478f, 0.061914f, 0.063545f, 0.066005f, 0.067654f,
    0.070146f, 0.071821f, 0.074356f, 0.076064f, 0.078656f,
    0.081291f, 0.083074f, 0.085794f, 0.087641f, 0.090470f,
    0.092399f, 0.095366f, 0.097397f, 0.100534f, 0.102691f,
    0.106038f, 0.109534f, 0.111956f, 0.115741f, 0.118375f,
    0.122509f, 0.125398f, 0.129952f, 0.133147f, 0.138201f,
    0.143598f, 0.147405f, 0.153457f, 0.157737f, 0.164557f,
    0.169392f, 0.177111f, 0.182591f, 0.191356f, 0.197587f,
    0.207562f, 0.218352f, 0.226032f, 0.238341f, 0.247105f,
    0.261154f, 0.271159f, 0.287196f, 0.298615f, 0.316915f,
    0.336715f, 0.350807f, 0.373372f, 0.389422f, 0.415108f,
    0.433367f, 0.462565f, 0.483306f, 0.516448f, 0.539971f,
    0.577529f, 0.617982f, 0.646654f, 0.692370f, 0.724745f,
    0.776318f, 0.812807f, 0.870882f, 0.911935f, 0.977214f
};

static const tube_biquad_coefficients_t pre_coefficients = {
    -46.5391808f, 93.0783615f, -46.5391808f,
    -1.98501062f, 0.985041738f
};

static const tube_biquad_coefficients_t post_coefficients = {
    1.0f, -2.0f, 1.0f, -1.98791063f, 0.987983286f
};

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

static float biquad(gp100_tube_biquad_state_t *state,
                    tube_biquad_coefficients_t c, float input)
{
    float output = input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                   state->y1 * c.a1 - state->y2 * c.a2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float gain_stage(gp100_tube_clipper_t *effect, float input)
{
    float b = effect->gain * -177.37592f;
    float a0 = effect->gain + 0.4824625f;
    float a1 = 0.4824625f - effect->gain;
    float output = (input * b + effect->input_x1 * b -
                    effect->input_y1 * a1) / a0;
    effect->input_x1 = input;
    effect->input_y1 = output;
    return output * 0.06f;
}

static float tube_shape(float input)
{
    const float magnitude = fabsf(input);
    const float output = magnitude * 0.5934f / (magnitude + 0.2021f);

    return copysignf(output, input);
}

static float oversampled_clip(gp100_tube_clipper_t *effect, float input)
{
    float step = (input - effect->previous_clip_input) / 3.0f;
    float shaped[4];
    size_t index;
    for (index = 0u; index < 4u; ++index)
        shaped[index] = tube_shape(effect->previous_clip_input +
                                   step * (float)index);
    effect->previous_clip_input = input;
    return gp100_decimate_four(effect->oversample_history, shaped);
}

static tube_tone_coefficients_t tone_coefficients(float bass, float treble)
{
    float treble2 = treble * treble;
    float bass_treble = bass * treble;
    float bass_treble2 = bass * treble2;
    tube_tone_coefficients_t c;
    c.b0 = bass * 0.09604503f + treble * 0.0034563192f + bass_treble -
           bass_treble2 * 0.19613272f - treble2 * 0.0006267794f +
           0.00030733558f;
    c.b1 = bass_treble2 * 0.5496363f - treble * 0.0033680547f -
           bass_treble * 2.921801f - bass * 0.25725463f +
           treble2 * 0.0006267794f - 0.00026849916f;
    c.b2 = bass * 0.22872576f - treble * 0.0034563192f +
           bass_treble * 2.8489468f - bass_treble2 * 0.51087445f +
           treble2 * 0.0006267794f - 0.00030733558f;
    c.b3 = treble * 0.0033680547f - bass * 0.06751617f -
           bass_treble * 0.9271457f + bass_treble2 * 0.15737085f -
           treble2 * 0.0006267794f + 0.00026849916f;
    c.a0 = bass * 0.95187074f + treble * 0.023697333f +
           bass_treble * 6.7825556f - bass_treble2 * 6.356202f -
           treble2 * 0.022205843f + 0.0033220092f;
    c.a1 = bass_treble2 * 18.877254f - treble * 0.020636799f -
           bass_treble * 19.271805f - bass * 2.7018726f +
           treble2 * 0.022195209f - 0.002890507f;
    c.a2 = bass * 2.5521724f - treble * 0.023695871f +
           bass_treble * 18.222313f - bass_treble2 * 18.685991f +
           treble2 * 0.022205843f - 0.0033213662f;
    c.a3 = treble * 0.020638261f - bass * 0.8021654f -
           bass_treble * 5.733052f + bass_treble2 * 6.164938f -
           treble2 * 0.022195209f + 0.0028911503f;
    return c;
}

static float third_order(gp100_tube_third_order_state_t *state,
                         tube_tone_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 +
                    state->x3 * c.b3 - state->y1 * c.a1 -
                    state->y2 * c.a2 - state->y3 * c.a3) / c.a0;
    state->x3 = state->x2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y3 = state->y2;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static bool parameters_valid(uint8_t gain, uint8_t volume,
                             uint8_t bass, uint8_t treble)
{
    return gain <= 99u && volume <= 99u && bass <= 99u && treble <= 99u;
}

bool gp100_tube_clipper_reset(gp100_tube_clipper_t *effect,
                                   uint8_t gain, uint8_t volume,
                                   uint8_t bass, uint8_t treble)
{
    if (effect == NULL || !parameters_valid(gain, volume, bass, treble))
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = effect->gain_target = gain_curve[gain];
    effect->volume = effect->volume_target = volume_curve[volume];
    effect->bass = effect->bass_target = bass_treble_curve[bass];
    effect->treble = effect->treble_target = bass_treble_curve[treble];
    effect->configured = true;
    return true;
}

bool gp100_tube_clipper_process(gp100_tube_clipper_t *effect,
                                     uint8_t gain, uint8_t volume,
                                     uint8_t bass, uint8_t treble,
                                     float *audio, size_t frames, bool enabled)
{
    tube_tone_coefficients_t tone;
    size_t frame;
    if (effect == NULL || !effect->configured ||
        !parameters_valid(gain, volume, bass, treble) ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    effect->gain_target = gain_curve[gain];
    effect->volume_target = volume_curve[volume];
    effect->bass_target = bass_treble_curve[bass];
    effect->treble_target = bass_treble_curve[treble];
    effect->volume = glide(effect->volume, effect->volume_target);
    effect->bass = glide(effect->bass, effect->bass_target);
    effect->treble = glide(effect->treble, effect->treble_target);
    tone = tone_coefficients(effect->bass, effect->treble);
    for (frame = 0u; frame < frames; ++frame) {
        float output;
        if (!enabled) continue;
        effect->gain = effect->gain_target;
        output = gain_stage(effect, audio[frame * 2u]);
        output = biquad(&effect->pre_filter, pre_coefficients, output);
        output = oversampled_clip(effect, output);
        output = biquad(&effect->post_filter, post_coefficients, output);
        output = third_order(&effect->tone_stack, tone, output);
        output *= effect->volume * 2.75f;
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
