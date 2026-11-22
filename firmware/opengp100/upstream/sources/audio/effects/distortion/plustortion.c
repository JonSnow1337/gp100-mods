#include "plustortion.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct plus_gain_coefficients {
    float b0, b1, a1;
} plus_gain_coefficients_t;

typedef struct plus_biquad_coefficients {
    float b0, b1, b2, a1, a2;
} plus_biquad_coefficients_t;

static const plus_gain_coefficients_t gain_coefficients[64] = {
    {7.8520732f, -7.8487668f, -0.9966940f},
    {7.8520732f, -7.8487668f, -0.9966940f},
    {8.0916519f, -8.0882311f, -0.9965790f},
    {8.3327942f, -8.3292561f, -0.9964620f},
    {8.5754099f, -8.5717554f, -0.9963450f},
    {8.8194704f, -8.8156967f, -0.9962270f},
    {9.0649948f, -9.0611029f, -0.9961090f},
    {9.3120651f, -9.3080549f, -0.9959900f},
    {9.5608215f, -9.5566912f, -0.9958700f},
    {9.8114634f, -9.8072119f, -0.9957490f},
    {10.0642481f, -10.0598745f, -0.9956270f},
    {10.3194962f, -10.3149996f, -0.9955040f},
    {10.5775862f, -10.5729647f, -0.9953790f},
    {10.8389559f, -10.8342094f, -0.9952530f},
    {11.1041040f, -11.0992289f, -0.9951250f},
    {11.3735876f, -11.3685827f, -0.9949950f},
    {11.6480236f, -11.6428862f, -0.9948630f},
    {11.9280901f, -11.9228172f, -0.9947280f},
    {12.2145262f, -12.2091160f, -0.9945890f},
    {12.5081377f, -12.5025864f, -0.9944480f},
    {12.8097992f, -12.8041010f, -0.9943020f},
    {13.1204529f, -13.1146059f, -0.9941520f},
    {13.4411240f, -13.4351215f, -0.9939980f},
    {13.7729177f, -13.7667561f, -0.9938380f},
    {14.1170349f, -14.1107073f, -0.9936720f},
    {14.4747772f, -14.4682760f, -0.9934990f},
    {14.8475599f, -14.8408794f, -0.9933190f},
    {15.2369299f, -15.2300606f, -0.9931310f},
    {15.6445780f, -15.6375132f, -0.9929350f},
    {16.0723629f, -16.0650902f, -0.9927280f},
    {16.5223312f, -16.5148411f, -0.9925110f},
    {16.9967518f, -16.9890327f, -0.9922820f},
    {17.4981422f, -17.4901829f, -0.9920400f},
    {18.0293217f, -18.0211067f, -0.9917840f},
    {18.5934448f, -18.5849552f, -0.9915120f},
    {19.1940708f, -19.1852932f, -0.9912220f},
    {19.8352337f, -19.8261452f, -0.9909130f},
    {20.5215244f, -20.5121059f, -0.9905820f},
    {21.2582054f, -21.2484322f, -0.9902260f},
    {22.0513325f, -22.0411758f, -0.9898440f},
    {22.9079266f, -22.8973560f, -0.9894300f},
    {23.8361702f, -23.8251534f, -0.9889820f},
    {24.8456764f, -24.8341713f, -0.9884950f},
    {25.9478092f, -25.9357738f, -0.9879640f},
    {27.1561203f, -27.1434994f, -0.9873810f},
    {28.4868908f, -28.4736309f, -0.9867390f},
    {29.9598846f, -29.9459114f, -0.9860280f},
    {31.5993156f, -31.5845528f, -0.9852370f},
    {33.4351997f, -33.4195518f, -0.9843510f},
    {35.5051994f, -35.4885521f, -0.9833530f},
    {37.8572311f, -37.8394508f, -0.9822180f},
    {40.5532227f, -40.5341415f, -0.9809170f},
    {43.6746330f, -43.6540451f, -0.9794110f},
    {47.3308372f, -47.3084831f, -0.9776470f},
    {51.6721840f, -51.6477394f, -0.9755530f},
    {56.9112091f, -56.8842354f, -0.9730250f},
    {63.3584633f, -63.3283768f, -0.9699140f},
    {71.4864044f, -71.4523926f, -0.9659930f},
    {82.0507202f, -82.0116196f, -0.9608960f},
    {96.3400421f, -96.2940445f, -0.9540020f},
    {116.7462082f, -116.6903610f, -0.9441570f},
    {148.2664795f, -148.1954346f, -0.9289490f},
    {203.3787079f, -203.2810669f, -0.9023600f},
    {203.3787079f, -203.2810669f, -0.9023600f}
};

static const plus_biquad_coefficients_t dc_block = {
    1.0f, -2.0f, 1.0f, -1.9919404f, 0.99197274f
};

static const plus_biquad_coefficients_t interstage = {
    0.47732532f, 0.0f, -0.47732532f, -0.8417716f, -0.15776782f
};

static const plus_biquad_coefficients_t output_coefficients[4] = {
    {0.815381f, -1.584f, 0.7693f, -1.963386f, 0.964192f},
    {0.805648f, -1.584f, 0.787911f, -1.958638f, 0.970684f},
    {0.915305f, -1.584f, 0.7394f, -1.68132f, 0.759619f},
    {1.584f, 0.03806f, 0.03058f, -0.004208f, 0.00038f}
};

static float biquad(gp100_plustortion_filter_t *state,
                    plus_biquad_coefficients_t c, float input)
{
    float output = input * c.b0 + state->x1 * c.b1 + state->x2 * c.b2 -
                   state->y1 * c.a1 - state->y2 * c.a2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float hard_clip_stage(gp100_plustortion_t *effect, float input)
{
    float samples[4];
    float step = (input - effect->previous_clip_input[0]) / 3.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index) {
        float value = effect->previous_clip_input[0] + step * (float)index;
        samples[index] = fmaxf(-2.0f, fminf(2.0f, value));
    }
    effect->previous_clip_input[0] = input;
    return gp100_decimate_four(effect->clip_history[0], samples);
}

static float germanium_shape(float input)
{
    float magnitude = fabsf(input);
    float output;
    if (magnitude < 0.5786f)
        output = 0.8163f * magnitude;
    else if (magnitude < 1.515f)
        output = magnitude * 2.0524f - 0.2944f -
                 magnitude * magnitude * 1.4693f +
                 magnitude * magnitude * magnitude * 0.3671f;
    else if (magnitude <= 3.0f)
        output = magnitude * 0.1169f + 0.57965f -
                 magnitude * magnitude * 0.0164f;
    else
        output = 0.78275f;
    return copysignf(output * 0.5f, input);
}

static float soft_clip_stage(gp100_plustortion_t *effect, float input)
{
    float samples[4];
    float step = (input - effect->previous_clip_input[1]) / 3.0f;
    size_t index;
    for (index = 0u; index < 4u; ++index)
        samples[index] = germanium_shape(effect->previous_clip_input[1] +
                                         step * (float)index);
    effect->previous_clip_input[1] = input;
    return gp100_decimate_four(effect->clip_history[1], samples);
}

static unsigned gain_index(uint8_t gain)
{
    return (unsigned)((float)gain * 2.55f) >> 2;
}

bool gp100_plustortion_reset(gp100_plustortion_t *effect,
                                  uint8_t gain, uint8_t volume)
{
    if (effect == NULL || gain > 99u || volume > 99u) return false;
    memset(effect, 0, sizeof(*effect));
    effect->volume_target = (float)volume * 0.01f;
    effect->configured = true;
    return true;
}

bool gp100_plustortion_process(gp100_plustortion_t *effect,
                                    uint8_t gain, uint8_t volume,
                                    float *audio, size_t frames, bool enabled)
{
    const plus_gain_coefficients_t *drive;
    size_t frame;
    if (effect == NULL || !effect->configured || gain > 99u || volume > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    drive = &gain_coefficients[gain_index(gain)];
    effect->volume_target = (float)volume * 0.01f;
    for (frame = 0u; frame < frames; ++frame) {
        float input;
        float output;
        size_t stage;
        if (!enabled) continue;
        if (effect->volume < effect->volume_target)
            effect->volume = fminf(effect->volume + 0.0001f,
                                   effect->volume_target);
        else if (effect->volume > effect->volume_target)
            effect->volume = fmaxf(effect->volume - 0.0001f,
                                   effect->volume_target);
        input = audio[frame * 2u];
        output = input * drive->b0 + effect->gain_filter.x1 * drive->b1 -
                 effect->gain_filter.y1 * drive->a1;
        effect->gain_filter.x1 = input;
        effect->gain_filter.y1 = output;
        output = biquad(&effect->pre_clip_filter[0], dc_block, output);
        output = hard_clip_stage(effect, output);
        output = biquad(&effect->interstage_filter, interstage, output);
        output = biquad(&effect->pre_clip_filter[1], dc_block, output);
        output = soft_clip_stage(effect, output);
        for (stage = 0u; stage < 4u; ++stage)
            output = biquad(&effect->output_filter[stage],
                            output_coefficients[stage], output);
        output *= effect->volume * 0.6f;
        audio[frame * 2u] = output;
        audio[frame * 2u + 1u] = output;
    }
    return true;
}
