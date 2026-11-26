#include "z38_cl_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct z38_coefficients
{
    float b[4], a[4];
    size_t order;
} z38_coefficients_t;

static const uint16_t bass_curve[100] = {
    0,    0,    9,    15,   25,   31,   41,   47,   57,   63,   73,   82,
    89,   98,   105,  114,  121,  130,  137,  146,  156,  162,  172,  178,
    188,  194,  204,  211,  221,  227,  237,  247,  254,  265,  272,  282,
    290,  301,  309,  321,  333,  341,  355,  364,  379,  389,  405,  416,
    434,  447,  468,  490,  506,  531,  550,  579,  600,  635,  659,  699,
    743,  775,  826,  863,  924,  967,  1038, 1089, 1171, 1231, 1327, 1433,
    1509, 1633, 1721, 1865, 1968, 2134, 2253, 2445, 2655, 2805, 3046, 3219,
    3496, 3694, 4012, 4239, 4602, 4861, 5276, 5723, 6042, 6550, 6911, 7487,
    7896, 8547, 9008, 9743};

static const float reconstruction_fir[40] = {
    -.0011f, -.0007f, .0001f,  .0015f,  .0032f,  .0048f,  .0053f,  .0034f,
    -.0016f, -.0092f, -.0175f, -.0233f, -.0227f, -.0123f, .0097f,  .0421f,
    .0807f,  .1193f,  .1504f,  .1678f,  .1678f,  .1504f,  .1193f,  .0807f,
    .0421f,  .0097f,  -.0123f, -.0227f, -.0233f, -.0175f, -.0092f, -.0016f,
    .0034f,  .0053f,  .0048f,  .0032f,  .0015f,  .0001f,  -.0007f, -.0011f};

static float linear_curve(uint8_t value)
{
    float index = floorf((float)value * 2.55f) - 2.f;
    if (index < 0.f)
        index = 0.f;
    return floorf(index / 251.f * 10000.f) * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static float filter(gp100_z38_cl_filter_t *state, z38_coefficients_t c,
                    float input)
{
    float output = c.b[0] * input;
    size_t i;
    for (i = 0u; i < c.order; ++i)
        output += c.b[i + 1u] * state->x[i] - c.a[i + 1u] * state->y[i];
    output /= c.a[0];
    for (i = c.order; i > 1u; --i)
    {
        state->x[i - 1u] = state->x[i - 2u];
        state->y[i - 1u] = state->y[i - 2u];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static z38_coefficients_t tone_coefficients(float bass, float middle,
                                            float treble)
{
    float bm = bass * middle;
    float bt = bass * treble;
    return (z38_coefficients_t){
        {bass * .006509536f + middle * .0003560573f + treble * .001420393f +
             bm * .2510463f + bt,
         bass * -.004942745f - middle * .0002640034f - treble * .001416486f -
             bm * .6881892f - bt * 3.f,
         bm * .6232394f - middle * .0003560573f - treble * .001420393f -
             bass * .006509536f + bt * 3.f,
         bass * .004942745f + middle * .0002640034f + treble * .001416486f -
             bm * .1860965f - bt},
        {bass * 1.187915f + middle * .0003560573f + bm * .2510463f +
             .001678061f,
         bass * -3.186349f - middle * .0002640034f - bm * .6881892f -
             .00115811f,
         bass * 2.812085f - middle * .0003560573f + bm * .6232394f -
             .001676643f,
         middle * .0002640034f - bass * .8136514f - bm * .1860965f +
             .001159527f},
        3u};
}

static float nonlinear(float input)
{
    float magnitude = fabsf(input);
    float power;
    unsigned i;
    if (magnitude >= 1.275f)
        return copysignf(1.275f, input);
    power = 1.f - magnitude * .7521f / 1024.f;
    for (i = 0u; i < 10u; ++i)
        power *= power;
    return copysignf((1.f - power) * 1.275f, input);
}

static float power_stage(gp100_z38_cl_amplifier_t *amplifier, float input)
{
    float points[4], output = 0.f;
    float step = (input - amplifier->previous_nonlinear_input) / 3.f;
    size_t i;
    points[0] = amplifier->previous_nonlinear_input;
    points[1] = points[0] + step;
    points[2] = points[0] + step * 2.f;
    points[3] = input;
    memmove(&amplifier->fir_history[4], amplifier->fir_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i)
        amplifier->fir_history[i] = nonlinear(points[3u - i]);
    for (i = 0u; i < 40u; ++i)
        output += amplifier->fir_history[i] * reconstruction_fir[i];
    amplifier->previous_nonlinear_input = input;
    return output;
}

bool gp100_z38_cl_amplifier_reset(gp100_z38_cl_amplifier_t *a,
                                       uint8_t gain, uint8_t cut,
                                       uint8_t master, uint8_t bass,
                                       uint8_t middle, uint8_t treble)
{
    if (a == NULL || gain > 99u || cut > 99u || master > 99u || bass > 99u ||
        middle > 99u || treble > 99u)
        return false;
    memset(a, 0, sizeof(*a));
    a->gain = a->gain_target = linear_curve(gain) * .9f;
    a->cut = a->cut_target = linear_curve(cut);
    a->master = a->master_target = linear_curve(master);
    a->bass = a->bass_target = (float)bass_curve[bass] * .0001f;
    a->middle = a->middle_target = linear_curve(middle);
    a->treble = a->treble_target = linear_curve(treble);
    a->configured = true;
    return true;
}

bool gp100_z38_cl_amplifier_process(gp100_z38_cl_amplifier_t *a,
                                         uint8_t gain, uint8_t cut,
                                         uint8_t master, uint8_t bass,
                                         uint8_t middle, uint8_t treble,
                                         float *audio, size_t frames,
                                         bool enabled)
{
    static const z38_coefficients_t voicing[3] = {
        {{.461609602f, -.803799987f, .342718989f, 0.f},
         {1.f, -1.77761865f, .780146301f, 0.f},
         2u},
        {{.803799987f, -.316558301f, .042179901f, 0.f},
         {1.f, -.782526195f, .177506402f, 0.f},
         2u},
        {{.803799987f, .367579311f, .016855801f, 0.f},
         {1.f, -.157289401f, .012856200f, 0.f},
         2u}};
    static const z38_coefficients_t output = {
        {-2.08249402f, 3.75538611f, -1.67289197f, 0.f},
        {1.f, -1.58751702f, .58803308f, 0.f},
        2u};
    size_t n, i;
    if (a == NULL || !a->configured || gain > 99u || cut > 99u ||
        master > 99u || bass > 99u || middle > 99u || treble > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    a->gain_target = linear_curve(gain) * .9f;
    a->cut_target = linear_curve(cut);
    a->master_target = linear_curve(master);
    a->bass_target = (float)bass_curve[bass] * .0001f;
    a->middle_target = linear_curve(middle);
    a->treble_target = linear_curve(treble);
    for (n = 0u; n < frames; ++n)
    {
        float signal;
        a->gain = glide(a->gain, a->gain_target);
        a->cut = glide(a->cut, a->cut_target);
        a->master = glide(a->master, a->master_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        signal = audio[n * 2u] * 29.4f * a->gain;
        for (i = 0u; i < 3u; ++i)
            signal = filter(&a->voicing[i], voicing[i], signal);
        signal = filter(
            &a->tone, tone_coefficients(a->bass, a->middle, a->treble), signal);
        signal = power_stage(a, signal);
        signal =
            filter(&a->cut_filter,
                   (z38_coefficients_t){
                       {-.052431874f, -.052431874f, 0.f, 0.f},
                       {a->cut + .09016772f, .014696025f - a->cut, 0.f, 0.f},
                       1u},
                   signal);
        signal =
            filter(&a->output_filter, output, signal) * a->master * 1.7625f;
        audio[n * 2u] = signal;
        audio[n * 2u + 1u] = signal;
    }
    return true;
}
