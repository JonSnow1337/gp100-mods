#include "bass_distortion.h"

#include <math.h>
#include <string.h>

typedef struct bass_distortion_coefficients {
    float b[5];
    float a[5];
    size_t order;
} bass_distortion_coefficients_t;

/* The UI indexes the first 100 reachable values of two 256-entry factory
 * curves through trunc(value * 2.55). Gain and Volume share the first curve;
 * Blend, Bass, and Treble share the second. */
static const float factory_gain_volume[100] = {
    0.0000f, 0.0000f, 0.0059f, 0.0099f, 0.0159f,
    0.0199f, 0.0258f, 0.0298f, 0.0358f, 0.0398f,
    0.0458f, 0.0517f, 0.0557f, 0.0617f, 0.0657f,
    0.0717f, 0.0756f, 0.0816f, 0.0856f, 0.0916f,
    0.0976f, 0.1016f, 0.1075f, 0.1115f, 0.1175f,
    0.1215f, 0.1275f, 0.1315f, 0.1375f, 0.1415f,
    0.1475f, 0.1535f, 0.1575f, 0.1635f, 0.1675f,
    0.1736f, 0.1776f, 0.1837f, 0.1877f, 0.1938f,
    0.2000f, 0.2041f, 0.2102f, 0.2144f, 0.2206f,
    0.2248f, 0.2312f, 0.2354f, 0.2418f, 0.2462f,
    0.2528f, 0.2594f, 0.2639f, 0.2708f, 0.2754f,
    0.2824f, 0.2872f, 0.2946f, 0.2995f, 0.3072f,
    0.3150f, 0.3204f, 0.3286f, 0.3343f, 0.3430f,
    0.3490f, 0.3583f, 0.3647f, 0.3746f, 0.3815f,
    0.3922f, 0.4034f, 0.4112f, 0.4234f, 0.4318f,
    0.4451f, 0.4543f, 0.4688f, 0.4789f, 0.4948f,
    0.5117f, 0.5235f, 0.5420f, 0.5550f, 0.5756f,
    0.5900f, 0.6127f, 0.6287f, 0.6539f, 0.6716f,
    0.6996f, 0.7294f, 0.7503f, 0.7834f, 0.8066f,
    0.8434f, 0.8692f, 0.9101f, 0.9388f, 0.9842f
};

static const float factory_mix_tone[100] = {
    0.0000f, 0.0000f, 0.0119f, 0.0199f, 0.0318f,
    0.0398f, 0.0517f, 0.0597f, 0.0717f, 0.0796f,
    0.0916f, 0.1035f, 0.1115f, 0.1235f, 0.1314f,
    0.1434f, 0.1513f, 0.1633f, 0.1713f, 0.1832f,
    0.1952f, 0.2031f, 0.2151f, 0.2231f, 0.2350f,
    0.2430f, 0.2549f, 0.2629f, 0.2749f, 0.2828f,
    0.2948f, 0.3067f, 0.3147f, 0.3266f, 0.3346f,
    0.3466f, 0.3545f, 0.3665f, 0.3745f, 0.3864f,
    0.3984f, 0.4063f, 0.4183f, 0.4262f, 0.4382f,
    0.4462f, 0.4581f, 0.4661f, 0.4780f, 0.4860f,
    0.4980f, 0.5099f, 0.5179f, 0.5298f, 0.5378f,
    0.5498f, 0.5577f, 0.5697f, 0.5776f, 0.5896f,
    0.6015f, 0.6095f, 0.6215f, 0.6294f, 0.6414f,
    0.6494f, 0.6613f, 0.6693f, 0.6812f, 0.6892f,
    0.7011f, 0.7131f, 0.7211f, 0.7330f, 0.7410f,
    0.7529f, 0.7609f, 0.7729f, 0.7808f, 0.7928f,
    0.8047f, 0.8127f, 0.8247f, 0.8326f, 0.8446f,
    0.8525f, 0.8645f, 0.8725f, 0.8844f, 0.8924f,
    0.9043f, 0.9163f, 0.9243f, 0.9362f, 0.9442f,
    0.9561f, 0.9641f, 0.9760f, 0.9840f, 0.9960f
};

static const float reconstruction_fir[40] = {
    -0.0011f, -0.0007f, 0.0001f, 0.0015f, 0.0032f,
    0.0048f, 0.0053f, 0.0034f, -0.0016f, -0.0092f,
    -0.0175f, -0.0233f, -0.0227f, -0.0123f, 0.0097f,
    0.0421f, 0.0807f, 0.1193f, 0.1504f, 0.1678f,
    0.1678f, 0.1504f, 0.1193f, 0.0807f, 0.0421f,
    0.0097f, -0.0123f, -0.0227f, -0.0233f, -0.0175f,
    -0.0092f, -0.0016f, 0.0034f, 0.0053f, 0.0048f,
    0.0032f, 0.0015f, 0.0001f, -0.0007f, -0.0011f
};

static float filter(gp100_bass_distortion_filter_t *state,
                    bass_distortion_coefficients_t c, float input)
{
    const float old_x[4] = {state->x1, state->x2, state->x3, state->x4};
    const float old_y[4] = {state->y1, state->y2, state->y3, state->y4};
    float output = c.b[0] * input;
    size_t i;
    for (i = 0u; i < c.order; ++i)
        output += c.b[i + 1u] * old_x[i] - c.a[i + 1u] * old_y[i];
    output /= c.a[0];
    state->x4 = state->x3; state->x3 = state->x2;
    state->x2 = state->x1; state->x1 = input;
    state->y4 = state->y3; state->y3 = state->y2;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static bass_distortion_coefficients_t input_coefficients(unsigned path,
                                                          float gain)
{
    float square = gain * gain;
    if (path == 0u) {
        float shared = square * -1.999667f + gain * 2.001179f + 0.374141f;
        return (bass_distortion_coefficients_t){
            {square * 17.67056f - gain * 13.13541f - 8.70982f,
             shared,
             square * -15.67089f + gain * 11.13725f + 8.332086f},
            {square + gain * 3.535147f - 5.575755f,
             shared,
             square * 0.9996666f - gain * 5.533302f + 5.198021f},
            2u};
    }
    return (bass_distortion_coefficients_t){
        {gain * 23.88749f + 0.3476386f,
         gain * -1.998489f - 0.01737911f,
         gain * -21.889f - 0.3298443f},
        {gain + 0.1462287f,
         gain * -1.998489f - 0.01737911f,
         gain * 0.9984894f - 0.1284344f},
        2u};
}

static bass_distortion_coefficients_t tone_coefficients(float bass,
                                                         float treble)
{
    float t2 = treble * treble;
    float b2 = bass * bass;
    float tb = treble * bass;
    float t2b = t2 * bass;
    float tb2 = treble * b2;
    float t2b2 = t2 * b2;
    return (bass_distortion_coefficients_t){
        {
            t2b2*.3811404f-t2b*.5348262f-t2*.7126435f-tb2*.5348262f+tb*.688512f+treble-b2*.02253194f+bass*.03161741f+.04212946f,
            t2b2*-.7740626f+t2b*1.060496f+t2*1.53975f+tb2*1.060496f-tb*1.346929f-treble*2.112584f+b2*.03423686f-bass*.0465234f-.06947934f,
            t2b2*.1305198f-t2b*.1095815f-t2*.3520827f-tb2*.1095815f+tb*.08864308f+treble*.3502678f+b2*.004163772f-bass*.009415117f-.004192862f,
            t2b2*.5365865f-t2b*.8230198f-t2*1.064797f-tb2*.8230198f+tb*1.109453f+treble*1.637632f-b2*.02378791f+bass*.03607445f+.04858579f,
            t2b2*-.2741842f+t2b*.4069316f+t2*.589774f+tb2*.4069316f-tb*.539679f-treble*.8753156f+b2*.007919218f-bass*.01175334f-.01703435f
        },
        {
            t2b2*.3811404f-t2b*.3049123f-t2*.7888715f-tb2*.3049123f+tb*.2286842f+treble*.6463428f-b2*.09876002f+bass*.09425363f+.1891645f,
            t2b2*-.7740626f+t2b*.6192501f+t2*1.694562f+tb2*.6192501f-tb*.4644376f-treble*1.386612f+b2*.1890494f-bass*.182202f-.3842766f,
            t2b2*.1305198f-t2b*.1044159f-t2*.3781867f-tb2*.1044159f+tb*.0783119f+treble*.3077701f-b2*.0219402f+bass*.02277295f+.06539092f,
            t2b2*.5365865f-t2b*.4292692f-t2*1.172115f-tb2*.4292692f+tb*.3219519f+treble*.9591553f-b2*.1311052f+bass*.1263476f+.2663029f,
            t2b2*-.2741842f+t2b*.2193473f+t2*.6446108f+tb2*.2193473f-tb*.1645105f-treble*.526656f+b2*.06275605f-bass*.06117221f-.136573f
        },
        4u};
}

static float reconstruct(float history[40], const float samples[4])
{
    float output = 0.0f;
    size_t i;
    memmove(&history[4], history, 36u * sizeof(float));
    for (i = 0u; i < 4u; ++i) history[i] = samples[3u - i];
    for (i = 0u; i < 40u; ++i) output += history[i] * reconstruction_fir[i];
    return output;
}

static void interpolate(float previous, float input, float samples[4])
{
    float step = (input - previous) / 3.0f;
    size_t i;
    for (i = 0u; i < 4u; ++i) samples[i] = previous + step * (float)i;
}

static float hard_clip_stage(gp100_bass_distortion_t *effect,
                             float input)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[0], input, samples);
    for (i = 0u; i < 4u; ++i)
        samples[i] = fmaxf(-4.0f, fminf(4.0f, samples[i]));
    effect->previous_stage_input[0] = input;
    return reconstruct(effect->oversample_history[0], samples);
}

static float rational_shape(float input)
{
    float magnitude = fabsf(input);
    float square = magnitude * magnitude;
    float cube = square * magnitude;
    float numerator = cube * 2.314f - square * 3.254f + magnitude * 4.637f;
    float denominator = cube - square * 0.2719f - magnitude * 1.218f + 4.167f;
    return copysignf(numerator / denominator, input);
}

static float rational_stage(gp100_bass_distortion_t *effect,
                            float input)
{
    float samples[4];
    size_t i;
    interpolate(effect->previous_stage_input[1], input, samples);
    for (i = 0u; i < 4u; ++i) samples[i] = rational_shape(samples[i]);
    effect->previous_stage_input[1] = input;
    return reconstruct(effect->oversample_history[1], samples);
}

static float glide(float current, float target, float step)
{
    if (current < target) return fminf(current + step, target);
    if (current > target) return fmaxf(current - step, target);
    return target;
}

bool gp100_bass_distortion_reset(
    gp100_bass_distortion_t *effect, uint8_t gain, uint8_t blend,
    uint8_t volume, uint8_t bass, uint8_t treble)
{
    if (effect == NULL || gain > 99u || blend > 99u || volume > 99u ||
        bass > 99u || treble > 99u) return false;
    memset(effect, 0, sizeof(*effect));
    effect->gain = factory_gain_volume[gain];
    effect->blend = effect->blend_target = factory_mix_tone[blend];
    effect->volume = effect->volume_target = factory_gain_volume[volume];
    effect->bass = factory_mix_tone[bass];
    effect->treble = effect->treble_target = factory_mix_tone[treble];
    effect->configured = true;
    return true;
}

bool gp100_bass_distortion_process(
    gp100_bass_distortion_t *effect, uint8_t gain, uint8_t blend,
    uint8_t volume, uint8_t bass, uint8_t treble, float *audio,
    size_t frames, bool enabled)
{
    static const bass_distortion_coefficients_t post_clip = {
        {14.3645897f, -1.37005794f, -12.9339199f},
        {1.0f, -1.37005794f, 0.430668414f}, 2u};
    static const bass_distortion_coefficients_t reconstruction = {
        {0.129373103f, -0.123409197f, -0.128966600f, 0.123815700f},
        {1.0f, -0.885922074f, -0.998733580f, 0.887188494f}, 3u};
    float gain_target, bass_target;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || blend > 99u ||
        volume > 99u || bass > 99u || treble > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u) return false;
    gain_target = factory_gain_volume[gain];
    bass_target = factory_mix_tone[bass];
    effect->blend_target = factory_mix_tone[blend];
    effect->volume_target = factory_gain_volume[volume];
    effect->treble_target = factory_mix_tone[treble];
    effect->blend = glide(effect->blend, effect->blend_target, 0.005f);
    effect->volume = glide(effect->volume, effect->volume_target, 0.005f);
    if (!enabled || frames == 0u) return true;

    /* Gain and Bass latch at the start of the block. Only the smoothed
     * Treble control can change the tone filter within a block. */
    const bass_distortion_coefficients_t dry_c = input_coefficients(0u, gain_target);
    const bass_distortion_coefficients_t wet_c = input_coefficients(1u, gain_target);
    bass_distortion_coefficients_t tone_c =
        tone_coefficients(bass_target, effect->treble);
    effect->gain = gain_target;
    effect->bass = bass_target;
    for (n = 0u; n < frames; ++n) {
        float dry, wet, output;
        const float next_treble = glide(effect->treble, effect->treble_target, 0.0001f);
        if (next_treble != effect->treble) {
            effect->treble = next_treble;
            tone_c = tone_coefficients(bass_target, next_treble);
        }
        dry = filter(&effect->input_filter[0], dry_c, audio[n * 2u]);
        wet = filter(&effect->input_filter[1], wet_c, audio[n * 2u]);
        wet = hard_clip_stage(effect, wet);
        wet = filter(&effect->post_clip_filter, post_clip, wet);
        wet = rational_stage(effect, wet);
        wet = filter(&effect->reconstruction_filter, reconstruction, wet);
        output = dry * (1.0f - effect->blend) + wet * effect->blend;
        output = filter(&effect->tone_filter, tone_c, output);
        output *= 1.5f * effect->volume;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
