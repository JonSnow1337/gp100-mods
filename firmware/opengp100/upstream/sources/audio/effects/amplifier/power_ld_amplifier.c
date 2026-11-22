#include "power_ld_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[4], a[4];
    size_t order;
} coefficients_t;
static const uint16_t middle_curve[100] = {
    0,    0,    59,   99,   159,  199,  258,  298,  358,  398,  458,  517,
    557,  617,  657,  717,  756,  816,  856,  916,  976,  1016, 1075, 1115,
    1175, 1215, 1275, 1315, 1375, 1415, 1475, 1535, 1575, 1635, 1675, 1736,
    1776, 1837, 1877, 1938, 2000, 2041, 2102, 2144, 2206, 2248, 2312, 2354,
    2418, 2462, 2528, 2594, 2639, 2708, 2754, 2824, 2872, 2946, 2995, 3072,
    3150, 3204, 3286, 3343, 3430, 3490, 3583, 3647, 3746, 3815, 3922, 4034,
    4112, 4234, 4318, 4451, 4543, 4688, 4789, 4948, 5117, 5235, 5420, 5550,
    5756, 5900, 6127, 6287, 6539, 6716, 6996, 7294, 7503, 7834, 8066, 8434,
    8692, 9101, 9388, 9842};
static const uint16_t linear_curve[100] = {
    0,    0,    119,  199,  318,  398,  517,  597,  717,  796,  916,  1035,
    1115, 1235, 1314, 1434, 1513, 1633, 1713, 1832, 1952, 2031, 2151, 2231,
    2350, 2430, 2549, 2629, 2749, 2828, 2948, 3067, 3147, 3266, 3346, 3466,
    3545, 3665, 3745, 3864, 3984, 4063, 4183, 4262, 4382, 4462, 4581, 4661,
    4780, 4860, 4980, 5099, 5179, 5298, 5378, 5498, 5577, 5697, 5776, 5896,
    6015, 6095, 6215, 6294, 6414, 6494, 6613, 6693, 6812, 6892, 7011, 7131,
    7211, 7330, 7410, 7529, 7609, 7729, 7808, 7928, 8047, 8127, 8247, 8326,
    8446, 8525, 8645, 8725, 8844, 8924, 9043, 9163, 9243, 9362, 9442, 9561,
    9641, 9760, 9840, 9960};
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
static const uint16_t presence_curve[100] = {
    0,    0,    738,  1202, 1857, 2268, 2849, 3213, 3725, 4046, 4498, 4917,
    5179, 5546, 5776, 6098, 6299, 6580, 6756, 7001, 7227, 7367, 7563, 7685,
    7855, 7961, 8108, 8199, 8326, 8404, 8514, 8614, 8676, 8762, 8815, 8889,
    8934, 8998, 9037, 9091, 9141, 9172, 9215, 9242, 9279, 9302, 9334, 9354,
    9383, 9400, 9425, 9448, 9463, 9484, 9497, 9516, 9528, 9545, 9556, 9572,
    9587, 9597, 9611, 9620, 9634, 9643, 9656, 9665, 9678, 9686, 9699, 9711,
    9719, 9732, 9740, 9752, 9760, 9772, 9780, 9792, 9804, 9812, 9824, 9832,
    9844, 9852, 9864, 9872, 9884, 9892, 9904, 9916, 9924, 9936, 9944, 9956,
    9964, 9976, 9984, 9996};
static const float half_fir[20] = {
    -.0013103f, -.0014933f, -.0017981f, -.0021462f, -.0023752f,
    -.0022494f, -.0014837f, .0002208f,  .0031343f,  .0074528f,
    .0132593f,  .0204929f,  .0289355f,  .0382153f,  .0478301f,
    .0571879f,  .0656602f,  .0726431f,  .0776181f,  .0802058f};
static const float phases[8][5] = {
    {-.0013103f, .0031343f, .0656602f, .0571879f, .0002208f},
    {-.0014933f, .0074528f, .0726431f, .0478301f, -.0014837f},
    {-.0017981f, .0132593f, .0776181f, .0382153f, -.0022494f},
    {-.0021462f, .0204929f, .0802058f, .0289355f, -.0023752f},
    {-.0023752f, .0289355f, .0802058f, .0204929f, -.0021462f},
    {-.0022494f, .0382153f, .0776181f, .0132593f, -.0017981f},
    {-.0014837f, .0478301f, .0726431f, .0074528f, -.0014933f},
    {.0002208f, .0571879f, .0656602f, .0031343f, -.0013103f}};
static float curve(const uint16_t *t, uint8_t v)
{
    return (float)t[v] * .0001f;
}
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
static float filter(gp100_power_ld_filter_t *s, coefficients_t c, float x)
{
    float y = c.b[0] * x;
    size_t i;
    for (i = 0; i < c.order; i++)
        y += c.b[i + 1] * s->x[i] - c.a[i + 1] * s->y[i];
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
    float bm = b * m, bt = b * t, t2 = t * t, mt = m * t, bt2 = b * t2,
          m2 = m * m, mt2 = m * t2, m2t = m2 * t, m2t2 = m2 * t2, bmt = bm * t,
          bmt2 = bm * t2;
    return (coefficients_t){
        {b * .01472269f + m * .01078763f + t * .02075923f + bm * .5339114f +
             bt - mt * .02f - m2 * .01067823f + .0002944538f,
         mt * .06f - m * .03099541f - t * .06075394f - bm * 1.544975f -
             bt * 3.f - b * .01326013f + m2 * .0308995f - .0002652027f,
         m * .02965493f - b * .01472269f + t * .05924077f + bm * 1.488216f +
             bt * 3.f - mt * .06f - m2 * .02976433f - .0002944538f,
         b * .01326013f - m * .009447145f - t * .01924606f - bm * .4771525f -
             bt + mt * .02f + m2 * .009543049f + .0002652027f},
        {b * 1.081657f - m * .009887389f + t * .008319645f + bm * .5658874f +
             bt * .4008864f - mt * .00512811f - bt2 * .3772258f +
             mt2 * .004248772f - m2t * .002784255f - m2 * .01131775f -
             t2 * .007827812f + m2t2 * .003175611f + bmt * .1392127f -
             bmt2 * .1587806f + .02244228f,
         m * .02848598f - b * 3.080037f - t * .02319508f - bm * 1.636633f -
             bt * 1.146625f + mt * .01490133f + bt2 * 1.123494f -
             mt2 * .01282292f + m2t * .007941448f + m2 * .03273265f +
             t2 * .02275121f - m2t2 * .009526834f - bmt * .3970724f +
             bmt2 * .4763417f - .06230444f,
         b * 2.918344f - m * .027282f + t * .02153136f + bm * 1.575643f +
             bt * 1.091651f - mt * .01440825f - bt2 * 1.115312f +
             mt2 * .01289953f - m2t * .007530131f - m2 * .03151287f -
             t2 * .02202293f + m2t2 * .009526834f + bmt * .3765065f -
             bmt2 * .4763417f + .05755844f,
         m * .008683408f - b * .9199618f - t * .006655412f - bm * .5048981f -
             bt * .3459122f + mt * .004635036f + bt2 * .3690429f -
             mt2 * .004325378f + m2t * .002372938f + m2 * .01009796f +
             t2 * .007099535f - m2t2 * .003175611f - bmt * .1186469f +
             bmt2 * .1587806f - .01769485f},
        3};
}
static coefficients_t presence(float p)
{
    return (coefficients_t){
        {1.035669f - p * .9993705f, p * 1.998741f - 2.063217f,
         1.027547f - p * .9993705f, 0},
        {p - 1.01483f, 2.020228f - p * 1.998741f, p * .998741f - 1.005408f, 0},
        2};
}
static float saturate(float x)
{
    float m = fabsf(x), y = m * .8417f / (m + .09062f);
    return copysignf(y, x);
}
static float nonlinear_stage(gp100_power_ld_amplifier_t *a, float x)
{
    float up[8], y = 0;
    size_t i, j;
    memmove(&a->input_history[1], a->input_history, 4 * sizeof(float));
    a->input_history[0] = x;
    for (i = 0; i < 8; i++)
    {
        up[i] = 0;
        for (j = 0; j < 5; j++)
            up[i] += a->input_history[j] * phases[i][j];
        up[i] *= 8.f;
    }
    memmove(&a->nonlinear_history[8], a->nonlinear_history, 32 * sizeof(float));
    for (i = 0; i < 8; i++)
        a->nonlinear_history[i] = saturate(up[7 - i]);
    for (i = 0; i < 40; i++)
        y += a->nonlinear_history[i] * half_fir[i < 20 ? i : 39 - i];
    return y;
}
static bool valid(uint8_t g, uint8_t p, uint8_t o, uint8_t b, uint8_t m,
                  uint8_t t)
{
    return g <= 99 && p <= 99 && o <= 99 && b <= 99 && m <= 99 && t <= 99;
}
static void targets(gp100_power_ld_amplifier_t *a, uint8_t g, uint8_t p,
                    uint8_t o, uint8_t b, uint8_t m, uint8_t t)
{
    a->gain_target = curve(linear_curve, g);
    a->presence_target = curve(presence_curve, p);
    a->master_target = curve(linear_curve, o);
    a->bass_target = curve(bass_curve, b);
    a->middle_target = curve(middle_curve, m);
    a->treble_target = curve(linear_curve, t);
}
bool gp100_power_ld_amplifier_reset(gp100_power_ld_amplifier_t *a,
                                         uint8_t g, uint8_t p, uint8_t o,
                                         uint8_t b, uint8_t m, uint8_t t)
{
    if (!a || !valid(g, p, o, b, m, t))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, g, p, o, b, m, t);
    a->gain = a->gain_target;
    a->presence = a->presence_target;
    a->master = a->master_target;
    a->bass = a->bass_target;
    a->middle = a->middle_target;
    a->treble = a->treble_target;
    a->configured = true;
    return true;
}
bool gp100_power_ld_amplifier_process(gp100_power_ld_amplifier_t *a,
                                           uint8_t g, uint8_t p, uint8_t o,
                                           uint8_t b, uint8_t m, uint8_t t,
                                           float *audio, size_t frames,
                                           bool enabled)
{
    static const coefficients_t in[3] = {
        {{.4827753901f, -.9642000198f, .4814284146f, 0},
         {1, -1.946848631f, .9474908113f, 0},
         2},
        {{.9089884162f, -.9642000198f, .134831205f, 0},
         {1, -1.393601418f, .4417017996f, 0},
         2},
        {{.9642000198f, .3036135137f, .004289200064f, 0},
         {1, -.08632659912f, .0006717999931f, 0},
         2}};
    size_t n, i;
    if (!a || !a->configured || !valid(g, p, o, b, m, t) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;
    targets(a, g, p, o, b, m, t);
    for (n = 0; n < frames; n++)
    {
        float x;
        a->gain = glide(a->gain, a->gain_target);
        a->master = glide(a->master, a->master_target);
        a->middle = glide(a->middle, a->middle_target);
        a->treble = glide(a->treble, a->treble_target);
        if (!enabled)
            continue;
        a->presence = a->presence_target;
        a->bass = a->bass_target;
        x = audio[n * 2] * 70.f * a->gain * 4.f;
        for (i = 0; i < 3; i++)
            x = filter(&a->input_filter[i], in[i], x);
        x = nonlinear_stage(a, x);
        x = filter(&a->tone_filter, tone(a->bass, a->middle, a->treble), x);
        x = filter(&a->presence_filter, presence(a->presence), x);
        x *= a->master * 2.5f * .1175f * 4.f * 1.6f;
        audio[n * 2] = x;
        audio[n * 2 + 1] = x;
    }
    return true;
}
