#include "z38_od_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct z38_od_coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
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
static const float interpolation[8][5] = {
    {-.0000417f, -.0058887f, .0758057f, .0617957f, -.0066209f},
    {-.0005088f, -.0032559f, .0877509f, .0470175f, -.0060670f},
    {-.0011711f, .0017899f, .0964619f, .0327264f, -.0048301f},
    {-.0021369f, .0095427f, .1010551f, .0199836f, -.0034084f},
    {-.0034084f, .0199836f, .1010551f, .0095427f, -.0021369f},
    {-.0048301f, .0327264f, .0964619f, .0017899f, -.0011711f},
    {-.0060670f, .0470175f, .0877509f, -.0032559f, -.0005088f},
    {-.0066209f, .0617957f, .0758057f, -.0058887f, -.0000417f}};
static const float reconstruction[20] = {
    -.0000417f, -.0005088f, -.0011711f, -.0021369f, -.0034084f,
    -.0048301f, -.0060670f, -.0066209f, -.0058887f, -.0032559f,
    .0017899f,  .0095427f,  .0199836f,  .0327264f,  .0470175f,
    .0617957f,  .0758057f,  .0877509f,  .0964619f,  .1010551f};
static float main_curve(uint8_t raw)
{
    const unsigned i = (unsigned)((float)raw * 2.55f);
    if (i <= 2)
        return 0;
    if (i >= 253)
        return 1;
    const unsigned steps = (i - 2u) * 10000u / 251u;
    return (float)steps / 10000.f;
}
static float bass_control(uint8_t raw)
{
    return (float)bass_curve[raw] / 10000.f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_z38_od_filter_t *s, coefficients_t c, float x)
{
    float y = c.b[0] * x;
    size_t i;
    for (i = 0; i < c.order; i++)
        y += c.b[i + 1] * s->x[i];
    for (i = 0; i < c.order; i++)
        y -= c.a[i + 1] * s->y[i];
    y /= c.a[0];
    for (i = c.order; i > 1; i--)
    {
        s->x[i - 1] = s->x[i - 2];
        s->y[i - 1] = s->y[i - 2];
    }
    s->x[0] = x;
    s->y[0] = y;
    return y;
}
static coefficients_t tone(float b, float m, float t)
{
    float bm = b * m, bt = b * t;
    return (coefficients_t){
        {b * .006509536f + m * .0003560573f + t * .001420393f + bm * .2510463f +
             bt,
         b * -.004942745f - m * .0002640034f - t * .001416486f -
             bm * .6881892f - bt * 3,
         bm * .6232394f - m * .0003560573f - t * .001420393f - b * .006509536f +
             bt * 3,
         b * .004942745f + m * .0002640034f + t * .001416486f - bm * .1860965f -
             bt},
        {b * 1.187915f + m * .0003560573f + bm * .2510463f + .001678061f,
         b * -3.186349f - m * .0002640034f - bm * .6881892f - .00115811f,
         b * 2.812085f - m * .0003560573f + bm * .6232394f - .001676643f,
         m * .0002640034f - b * .8136514f - bm * .1860965f + .001159527f},
        3};
}
static float shape(float x)
{
    float magnitude = fabsf(x), power;
    unsigned i;
    if (magnitude >= 1)
        return copysignf(1, x);
    power = 1 - magnitude * 25.f / 1024.f;
    for (i = 0; i < 10; i++)
        power *= power;
    return copysignf(1 - power, x);
}
static float nonlinear_stage(gp100_z38_od_amplifier_t *a, float x)
{
    float sample[5] = {x, a->interpolation_history[0],
                       a->interpolation_history[1], a->interpolation_history[2],
                       a->interpolation_history[3]},
          phase[8], output = 0;
    size_t i, j;
    for (i = 0; i < 8; i++)
    {
        phase[i] = 0;
        for (j = 0; j < 5; j++)
            phase[i] += sample[j] * interpolation[i][j];
        phase[i] = shape(phase[i] * 8);
    }
    for (i = 3; i > 0; i--)
        a->interpolation_history[i] = a->interpolation_history[i - 1];
    a->interpolation_history[0] = x;
    memmove(&a->reconstruction_history[8], a->reconstruction_history,
            32 * sizeof(float));
    for (i = 0; i < 8; i++)
        a->reconstruction_history[i] = phase[7 - i];
    for (i = 0; i < 20; i++)
        output +=
            (a->reconstruction_history[i] + a->reconstruction_history[39 - i]) *
            reconstruction[i];
    return output;
}
static bool valid(uint8_t g, uint8_t c, uint8_t p, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && c <= 99 && p <= 99 && b <= 99 && m <= 99 && t <= 99;
}
static void targets(gp100_z38_od_amplifier_t *a, uint8_t g, uint8_t c,
                    uint8_t p, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = main_curve(g);
    a->cut_target = main_curve(c);
    a->presence_target = main_curve(p);
    a->bass_target = bass_control(b);
    a->middle_target = main_curve(m);
    a->treble_target = main_curve(t);
}
bool gp100_z38_od_amplifier_reset(gp100_z38_od_amplifier_t *a,
                                       uint8_t g, uint8_t c, uint8_t p,
                                       uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || !valid(g, c, p, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, c, p, b, m, t);
    a->gain = a->gain_target;
    a->cut_value = a->cut_target;
    a->presence = a->presence_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->configured = true;
    return true;
}
bool gp100_z38_od_amplifier_process(gp100_z38_od_amplifier_t *a,
                                         uint8_t g, uint8_t c, uint8_t p,
                                         uint8_t b, uint8_t m, uint8_t t,
                                         float *audio, size_t frames,
                                         bool enabled)
{
    static const coefficients_t input[3] = {
        {{.461609602f, -.8038f, .342718989f, 0},
         {1, -1.77761865f, .780146301f, 0},
         2},
        {{.8038f, -.316558301f, .042179901f, 0},
         {1, -.782526195f, .177506402f, 0},
         2},
        {{.8038f, .367579311f, .016855801f, 0},
         {1, -.157289401f, .0128562f, 0},
         2}};
    static const coefficients_t presence = {
        {-2.08249402f, 3.75538611f, -1.67289197f, 0},
        {1, -1.58751702f, .58803308f, 0},
        2};
    size_t n, i;
    if (!a || !a->configured || !valid(g, c, p, b, m, t) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, c, p, b, m, t);
    for (n = 0; n < frames; n++)
    {
        float x;
        a->gain = glide(a->gain, a->gain_target);
        a->cut_value = glide(a->cut_value, a->cut_target);
        a->presence = glide(a->presence, a->presence_target);
        a->bass = glide(a->bass, a->bass_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        x = audio[n * 2] * 29.4f * a->gain;
        for (i = 0; i < 3; i++)
            x = filter(&a->input[i], input[i], x);
        x = filter(&a->tone, tone(a->bass, a->middle, a->treble), x);
        x = nonlinear_stage(a, x);
        x = filter(&a->cut,
                   (coefficients_t){{-.052431874f, -.052431874f, 0, 0},
                                    {a->cut_value + .09016772f,
                                     .014696025f - a->cut_value, 0, 0},
                                    1},
                   x);
        x = filter(&a->presence_filter, presence, x) * a->presence * .4f * .257f * 10.04f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
