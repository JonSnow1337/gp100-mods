#include "ac_pre_amplifier.h"
#include <math.h>
#include <string.h>
typedef struct coefficients
{
    float b[3], a[3];
} coefficients_t;
static float glide(float c, float t)
{
    if (c < t)
        return fminf(c + .0001f, t);
    if (c > t)
        return fmaxf(c - .0001f, t);
    return t;
}
/* The factory curves store four decimal places. */
static const uint16_t eq_width_curve[256] =
{
       0,    0,    0, 3311, 3804, 4125, 4370, 4569, 4739, 4887, 5019, 5139,
    5248, 5349, 5443, 5531, 5614, 5692, 5766, 5836, 5903, 5967, 6029, 6088,
    6145, 6200, 6253, 6304, 6354, 6402, 6449, 6494, 6538, 6581, 6623, 6664,
    6704, 6743, 6781, 6818, 6855, 6890, 6925, 6960, 6993, 7026, 7059, 7091,
    7122, 7152, 7183, 7212, 7242, 7270, 7299, 7326, 7354, 7381, 7408, 7434,
    7460, 7485, 7511, 7535, 7560, 7584, 7608, 7632, 7655, 7678, 7701, 7723,
    7746, 7768, 7789, 7811, 7832, 7853, 7874, 7895, 7915, 7935, 7955, 7975,
    7995, 8014, 8033, 8052, 8071, 8090, 8108, 8127, 8145, 8163, 8181, 8199,
    8216, 8234, 8251, 8268, 8285, 8302, 8318, 8335, 8351, 8368, 8384, 8400,
    8416, 8432, 8447, 8463, 8479, 8494, 8509, 8524, 8539, 8554, 8569, 8584,
    8598, 8613, 8627, 8642, 8656, 8670, 8684, 8698, 8712, 8726, 8739, 8753,
    8767, 8780, 8793, 8807, 8820, 8833, 8846, 8859, 8872, 8885, 8898, 8910,
    8923, 8935, 8948, 8960, 8973, 8985, 8997, 9009, 9021, 9033, 9045, 9057,
    9069, 9081, 9092, 9104, 9115, 9127, 9138, 9150, 9161, 9172, 9184, 9195,
    9206, 9217, 9228, 9239, 9250, 9261, 9271, 9282, 9293, 9304, 9314, 9325,
    9335, 9346, 9356, 9367, 9377, 9387, 9397, 9408, 9418, 9428, 9438, 9448,
    9458, 9468, 9478, 9488, 9497, 9507, 9517, 9527, 9536, 9546, 9555, 9565,
    9574, 9584, 9593, 9603, 9612, 9621, 9631, 9640, 9649, 9658, 9667, 9677,
    9686, 9695, 9704, 9713, 9722, 9730, 9739, 9748, 9757, 9766, 9775, 9783,
    9792, 9801, 9809, 9818, 9826, 9835, 9843, 9852, 9860, 9869, 9877, 9885,
    9894, 9902, 9910, 9919, 9927, 9935, 9943, 9951, 9959, 9967, 9976, 9984,
    9992, 10000, 10000, 10000,
};

static const uint16_t balance_curve[256] =
{
       0,    0,    0,    0,    0,    1,    2,    4,    5,    7,   10,   12,
      15,   19,   22,   26,   31,   35,   40,   45,   51,   57,   63,   70,
      76,   84,   91,   99,  107,  115,  124,  133,  142,  152,  162,  172,
     183,  194,  205,  217,  229,  241,  254,  266,  280,  293,  307,  321,
     335,  350,  365,  381,  396,  412,  429,  445,  462,  480,  497,  515,
     534,  552,  571,  590,  610,  630,  650,  670,  691,  712,  734,  755,
     777,  800,  822,  845,  869,  892,  916,  941,  965,  990, 1015, 1041,
    1067, 1093, 1120, 1146, 1173, 1201, 1229, 1257, 1285, 1314, 1343, 1372,
    1402, 1432, 1462, 1493, 1524, 1555, 1587, 1619, 1651, 1683, 1716, 1750,
    1783, 1817, 1851, 1885, 1920, 1955, 1991, 2026, 2062, 2099, 2135, 2172,
    2210, 2247, 2285, 2323, 2362, 2401, 2440, 2480, 2520, 2560, 2600, 2641,
    2682, 2723, 2765, 2807, 2850, 2892, 2935, 2979, 3022, 3066, 3111, 3155,
    3200, 3245, 3291, 3337, 3383, 3429, 3476, 3523, 3571, 3619, 3667, 3715,
    3764, 3813, 3862, 3912, 3962, 4012, 4063, 4114, 4165, 4217, 4269, 4321,
    4373, 4426, 4479, 4533, 4587, 4641, 4695, 4750, 4805, 4861, 4916, 4972,
    5029, 5085, 5142, 5200, 5257, 5315, 5373, 5432, 5491, 5550, 5610, 5669,
    5730, 5790, 5851, 5912, 5973, 6035, 6097, 6160, 6222, 6285, 6349, 6412,
    6476, 6541, 6605, 6670, 6735, 6801, 6867, 6933, 6999, 7066, 7133, 7201,
    7269, 7337, 7405, 7474, 7543, 7612, 7682, 7752, 7822, 7893, 7964, 8035,
    8107, 8179, 8251, 8323, 8396, 8469, 8543, 8617, 8691, 8765, 8840, 8915,
    8991, 9066, 9142, 9219, 9295, 9372, 9450, 9527, 9605, 9683, 9762, 9841,
    9920, 10000, 10000, 10000,
};

static const uint16_t frequency_curve[256] =
{
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    1,    1,    1,    1,    1,    2,    2,    2,    3,
       3,    3,    4,    4,    5,    5,    6,    7,    7,    8,    9,   10,
      11,   12,   13,   14,   15,   17,   18,   19,   21,   23,   24,   26,
      28,   30,   32,   34,   37,   39,   42,   45,   47,   50,   53,   57,
      60,   64,   67,   71,   75,   79,   84,   88,   93,   98,  103,  108,
     113,  119,  125,  131,  137,  144,  151,  158,  165,  172,  180,  188,
     196,  205,  214,  223,  232,  242,  251,  262,  272,  283,  294,  306,
     318,  330,  342,  355,  368,  382,  396,  410,  425,  440,  456,  472,
     488,  505,  522,  540,  558,  576,  595,  615,  635,  655,  676,  697,
     719,  742,  764,  788,  812,  836,  861,  887,  913,  940,  967,  995,
    1024, 1053, 1083, 1113, 1144, 1176, 1208, 1241, 1275, 1309, 1344, 1380,
    1417, 1454, 1492, 1530, 1570, 1610, 1651, 1692, 1735, 1778, 1822, 1867,
    1913, 1959, 2007, 2055, 2104, 2154, 2205, 2256, 2309, 2363, 2417, 2472,
    2529, 2586, 2644, 2704, 2764, 2825, 2887, 2951, 3015, 3080, 3147, 3214,
    3283, 3353, 3423, 3495, 3568, 3642, 3718, 3794, 3872, 3951, 4031, 4112,
    4194, 4278, 4363, 4449, 4537, 4625, 4715, 4807, 4899, 4993, 5089, 5185,
    5284, 5383, 5484, 5586, 5690, 5795, 5902, 6010, 6119, 6230, 6343, 6457,
    6572, 6689, 6808, 6928, 7050, 7173, 7298, 7425, 7553, 7683, 7815, 7948,
    8083, 8220, 8358, 8499, 8641, 8784, 8930, 9077, 9226, 9377, 9530, 9685,
    9841, 10000, 10000, 10000,
};

static float curve_value(const uint16_t curve[256], unsigned index)
{
    return (float)curve[index] / 10000.f;
}

static float control_curve(const uint16_t curve[256], uint8_t value)
{
    return curve_value(curve, (unsigned)((float)value * 2.55f));
}

static float tone_frequency(float tone)
{
    return curve_value(frequency_curve, (unsigned)(tone * 255.f));
}
static float filter(gp100_ac_pre_filter_t *s, coefficients_t c, float x)
{
    float y = (c.b[0] * x + c.b[1] * s->x[0] + c.b[2] * s->x[1] -
               c.a[1] * s->y[0] - c.a[2] * s->y[1]) /
              c.a[0];
    s->x[1] = s->x[0];
    s->y[1] = s->y[0];
    s->x[0] = x;
    s->y[0] = y;
    return y;
}
static coefficients_t peaking(float angle, float width, float gain)
{
    float k = sinf(angle) / width, c = -2.f * cosf(angle);
    return (coefficients_t){{1.f + k * gain, c, 1.f - k * gain},
                            {1.f + k / gain, c, 1.f - k / gain}};
}
static coefficients_t tone_low(float tone)
{
    float mapped = tone_frequency(tone);
    float gain = powf(10.f, ((1.f - mapped) * 25.f - 5.f) / 40.f);
    return peaking(.002422089688f, .626f, gain);
}
static coefficients_t tone_high(float tone)
{
    float db = fminf(tone * 22.f, 12.f);
    float angle = ((12000.f - tone * 2000.f) / 44100.f) * 6.2831855f;
    return peaking(angle, .68f, powf(10.f, db / 40.f));
}
static coefficients_t equalizer(float frequency, float q, float gain)
{
    float angle = ((frequency * 12172.f + 844.f) / 44100.f) * 6.2831855f;
    float width = ((1.f - q) * 2.f + .6f) * 2.f;
    return peaking(angle, width, powf(10.f, (gain * 30.f - 15.f) / 40.f));
}
static bool valid(uint8_t v, uint8_t t, uint8_t b, uint8_t f, uint8_t q,
                  uint8_t g)
{
    return v <= 99 && t <= 99 && b <= 99 && f <= 99 && q <= 99 && g <= 99;
}
static void targets(gp100_ac_pre_amplifier_t *a, uint8_t v, uint8_t t,
                    uint8_t b, uint8_t f, uint8_t q, uint8_t g)
{
    a->volume_target = v * .01f;
    a->tone_target = t * .01f;
    a->balance_target = control_curve(balance_curve, b);
    a->frequency_target = control_curve(frequency_curve, f);
    a->q_target = control_curve(eq_width_curve, q);
    a->gain_target = g * .01f;
}
bool gp100_ac_pre_amplifier_reset(gp100_ac_pre_amplifier_t *a,
                                       uint8_t v, uint8_t t, uint8_t b,
                                       uint8_t f, uint8_t q, uint8_t g)
{
    if (!a || !valid(v, t, b, f, q, g))
        return false;
    memset(a, 0, sizeof(*a));
    targets(a, v, t, b, f, q, g);
    a->volume = a->volume_target;
    a->tone_value = a->tone_target;
    a->balance = a->balance_target;
    a->frequency = a->frequency_target;
    a->q = a->q_target;
    a->gain = a->gain_target;
    a->configured = true;
    return true;
}
bool gp100_ac_pre_amplifier_process(gp100_ac_pre_amplifier_t *a,
                                         uint8_t v, uint8_t t, uint8_t b,
                                         uint8_t f, uint8_t q, uint8_t g,
                                         float *x, size_t frames, bool enabled)
{
    size_t n;
    if (!a || !a->configured || !valid(v, t, b, f, q, g) || (frames && !x) ||
        frames > SIZE_MAX / 2)
        return false;
    targets(a, v, t, b, f, q, g);
    for (n = 0; n < frames; n++)
    {
        float dry, y;
        a->volume = glide(a->volume, a->volume_target);
        a->tone_value = glide(a->tone_value, a->tone_target);
        a->balance = glide(a->balance, a->balance_target);
        a->gain = glide(a->gain, a->gain_target);
        a->q = glide(a->q, a->q_target);
        a->frequency = glide(a->frequency, a->frequency_target);
        if (!enabled)
            continue;
        dry = x[n * 2];
        y = filter(&a->tone[0], tone_low(a->tone_value), dry);
        y = filter(&a->tone[1], tone_high(a->tone_value), y);
        y = y * a->balance + dry * (1.f - a->balance);
        y = filter(&a->equalizer, equalizer(a->frequency, a->q, a->gain), y);
        y *= a->volume * .871f * 1.3f;
        x[n * 2] = y;
        x[n * 2 + 1] = y;
    }
    return true;
}
