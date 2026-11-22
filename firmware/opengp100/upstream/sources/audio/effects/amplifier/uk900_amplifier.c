#include "uk900_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct coefficients
{
    float b[4];
    float a[4];
    size_t order;
} coefficients_t;

static const uint16_t drive_curve[100] = {
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

static const uint16_t presence_curve[100] = {
    10000, 10000, 9262, 8798, 8142, 7731, 7151, 6787, 6274, 5953, 5501, 5082,
    4821,  4453,  4224, 3902, 3701, 3419, 3244, 2998, 2773, 2632, 2436, 2314,
    2144,  2039,  1892, 1801, 1674, 1595, 1486, 1386, 1324, 1238, 1185, 1111,
    1065,  1002,  963,  908,  858,  827,  784,  758,  721,  698,  665,  645,
    617,   599,   574,  551,  537,  516,  503,  484,  472,  455,  444,  428,
    413,   403,   388,  379,  365,  356,  343,  335,  322,  313,  301,  288,
    280,   268,   260,  248,  239,  227,  219,  207,  195,  187,  175,  167,
    155,   147,   135,  127,  116,  108,  96,   84,   76,   64,   56,   44,
    36,    24,    16,   4};

static const float half_fir[20] = {
    -.0000417f, -.0005088f, -.0011711f, -.0021369f, -.0034084f,
    -.0048301f, -.006067f,  -.0066209f, -.0058887f, -.0032559f,
    .0017899f,  .0095427f,  .0199836f,  .0327264f,  .0470175f,
    .0617957f,  .0758057f,  .0877509f,  .0964619f,  .1010551f};

static const float interpolation_phases[8][5] = {
    {-.0000417f, -.0058887f, .0758057f, .0617957f, -.0066209f},
    {-.0005088f, -.0032559f, .0877509f, .0470175f, -.006067f},
    {-.0011711f, .0017899f, .0964619f, .0327264f, -.0048301f},
    {-.0021369f, .0095427f, .1010551f, .0199836f, -.0034084f},
    {-.0034084f, .0199836f, .1010551f, .0095427f, -.0021369f},
    {-.0048301f, .0327264f, .0964619f, .0017899f, -.0011711f},
    {-.006067f, .0470175f, .0877509f, -.0032559f, -.0005088f},
    {-.0066209f, .0617957f, .0758057f, -.0058887f, -.0000417f}};

static float curve(const uint16_t *table, uint8_t value)
{
    return (float)table[value] * .0001f;
}

static float glide_block(float current, float target, size_t frame_count)
{
    float step = (float)frame_count * .0001f;
    if (current < target)
        return fminf(current + step, target);
    if (current > target)
        return fmaxf(current - step, target);
    return target;
}

static float filter(gp100_uk900_filter_t *state,
                    coefficients_t coefficients, float input)
{
    float output = coefficients.b[0] * input;
    size_t i;

    for (i = 0; i < coefficients.order; ++i)
        output += coefficients.b[i + 1] * state->x[i] -
                  coefficients.a[i + 1] * state->y[i];
    output /= coefficients.a[0];
    for (i = coefficients.order; i > 1; --i)
    {
        state->x[i - 1] = state->x[i - 2];
        state->y[i - 1] = state->y[i - 2];
    }
    state->x[0] = input;
    state->y[0] = output;
    return output;
}

static coefficients_t tone(float bass, float middle, float treble)
{
    float bm = bass * middle;
    float bt = bass * treble;
    float mt = middle * treble;
    float m2 = middle * middle;
    float t2 = treble * treble;
    float bt2 = bass * t2;
    float mt2 = middle * t2;
    float m2t = m2 * treble;
    float m2t2 = m2 * t2;
    float bmt = bm * treble;
    float bmt2 = bm * t2;

    return (coefficients_t){
        {treble * .000557462128f - middle * .0215350569f - bass +
             bm * .451506872f + bt * .536659279f + mt * .0118065041f +
             m2 * .00993315118f - .00102240526f,
         bass * 2.89431890f + middle * .0632918065f - treble * .000548823750f -
             bm * 1.27438443f - bt * 1.60997784f - mt * .0354195124f -
             m2 * .0280364575f + .000932033127f,
         bm * 1.19424825f - middle * .0618975724f - treble * .000557462128f -
             bass * 2.79239224f + bt * 1.60997784f + mt * .0354195124f +
             m2 * .0262734615f + .00102240526f,
         bass * .898073339f + middle * .0201408228f + treble * .000548823750f -
             bm * .371370690f - bt * .536659279f - mt * .0118065041f -
             m2 * .00817015518f - .000932033127f},
        {bm * .924174183f - middle * .0467213378f - treble * .000412106483f -
             bass * 2.16695511f - bt * .402954370f - mt * .00868521718f +
             bt2 * .422593359f + mt2 * .00912444193f + m2t * .00384158915f +
             m2 * .0203318320f + t2 * .000433551700f - m2t2 * .00368429917f +
             bmt * .174617689f - bmt2 * .167468144f - .00221746300f,
         bass * 5.95438443f + middle * .130251871f + treble * .000503180388f -
             bm * 2.56984327f + bt * 1.29137747f + mt * .0281930758f -
             bt2 * 1.25993408f - mt2 * .0275459378f - m2t * .0123154772f -
             m2 * .0565365520f - t2 * .000429508205f + m2t2 * .0110528975f -
             bmt * .559794420f + bmt2 * .502404432f + .00168903166f,
         treble * .000412993226f - middle * .120143921f - bass * 5.41783341f +
             bm * 2.36733606f - bt * 1.37220796f - mt * .0303683540f +
             bt2 * 1.25208808f + mt2 * .0277185497f + m2t * .0131061870f +
             m2 * .0520813934f - t2 * .000433551700f - m2t2 * .0110528975f +
             bmt * .595735773f - bmt2 * .502404432f + .00221269877f,
         bass * 1.63039602f + middle * .0366133872f - treble * .000502293644f -
             bm * .721666972f + bt * .483784854f + mt * .0108604954f -
             bt2 * .414747360f - mt2 * .00929705391f - m2t * .00463229893f -
             m2 * .0158766734f + t2 * .000429508205f + m2t2 * .00368429917f -
             bmt * .210559042f + bmt2 * .167468144f - .00169379589f},
        3};
}

static coefficients_t presence(float value)
{
    return (coefficients_t){{1.035669467f - value * .9993705149f,
                             value * 1.998741030f - 2.063216547f,
                             1.027547079f - value * .9993705149f, 0.f},
                            {value - 1.014829978f,
                             2.020227753f - value * 1.998741030f,
                             value * .9987410298f - 1.005408007f, 0.f},
                            2};
}

static float saturate(float input)
{
    float magnitude = fabsf(input);
    return copysignf(magnitude * .9403f / (magnitude + .1677f), input);
}

static float nonlinear_stage(gp100_uk900_amplifier_t *amplifier,
                             float input)
{
    float phase[8];
    float output = 0.f;
    size_t i;
    size_t j;

    memmove(&amplifier->input_history[1], amplifier->input_history,
            4 * sizeof(float));
    amplifier->input_history[0] = input;
    for (i = 0; i < 8; ++i)
    {
        phase[i] = 0.f;
        for (j = 0; j < 5; ++j)
            phase[i] +=
                amplifier->input_history[j] * interpolation_phases[i][j];
        phase[i] *= 8.f;
    }
    memmove(&amplifier->nonlinear_history[8], amplifier->nonlinear_history,
            32 * sizeof(float));
    for (i = 0; i < 8; ++i)
        amplifier->nonlinear_history[i] = saturate(phase[7 - i]);
    for (i = 0; i < 20; ++i)
        output += (amplifier->nonlinear_history[i] +
                   amplifier->nonlinear_history[39 - i]) *
                  half_fir[i];
    return output;
}

static bool valid(uint8_t gain, uint8_t presence_value, uint8_t master,
                  uint8_t bass, uint8_t middle, uint8_t treble)
{
    return gain <= 99 && presence_value <= 99 && master <= 99 && bass <= 99 &&
           middle <= 99 && treble <= 99;
}

static void set_targets(gp100_uk900_amplifier_t *amplifier, uint8_t gain,
                        uint8_t presence_value, uint8_t master, uint8_t bass,
                        uint8_t middle, uint8_t treble)
{
    amplifier->gain_target = curve(drive_curve, gain);
    amplifier->presence_target = 1.f - curve(presence_curve, presence_value);
    amplifier->master_target = curve(drive_curve, master);
    amplifier->bass_target = curve(drive_curve, bass);
    amplifier->middle_target = 1.f - curve(linear_curve, middle);
    amplifier->treble_target = 1.f - curve(linear_curve, treble);
}

bool gp100_uk900_amplifier_reset(gp100_uk900_amplifier_t *amplifier,
                                      uint8_t gain, uint8_t presence_value,
                                      uint8_t master, uint8_t bass,
                                      uint8_t middle, uint8_t treble)
{
    if (!amplifier ||
        !valid(gain, presence_value, master, bass, middle, treble))
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    set_targets(amplifier, gain, presence_value, master, bass, middle, treble);
    amplifier->gain = amplifier->gain_target;
    amplifier->presence = amplifier->presence_target;
    amplifier->master = amplifier->master_target;
    amplifier->bass = amplifier->bass_target;
    amplifier->middle = amplifier->middle_target;
    amplifier->treble = amplifier->treble_target;
    amplifier->configured = true;
    return true;
}

bool gp100_uk900_amplifier_process(gp100_uk900_amplifier_t *amplifier,
                                        uint8_t gain, uint8_t presence_value,
                                        uint8_t master, uint8_t bass,
                                        uint8_t middle, uint8_t treble,
                                        float *audio, size_t frames,
                                        bool enabled)
{
    static const coefficients_t input = {
        {4.933197021f, -1.513663173f, -3.381581306f, 0.f},
        {1.f, -1.513663173f, .5516155362f, 0.f},
        2};
    static const coefficients_t reconstruction = {
        {0x1.6a3998p-1f, -0x1.01c5d6p+1f, 0x1.e85314p+0f, -0x1.33c866p-1f},
        {1.0f, -0x1.635030p+1f, 0x1.470158p+1f, -0x1.8ec40cp-1f},
        3};
    coefficients_t tone_coefficients;
    coefficients_t presence_coefficients;
    size_t frame;

    if (!amplifier || !amplifier->configured ||
        !valid(gain, presence_value, master, bass, middle, treble) ||
        (frames && !audio) || frames > SIZE_MAX / 2)
        return false;

    set_targets(amplifier, gain, presence_value, master, bass, middle, treble);
    amplifier->gain =
        glide_block(amplifier->gain, amplifier->gain_target, frames);
    amplifier->master =
        glide_block(amplifier->master, amplifier->master_target, frames);
    amplifier->bass =
        glide_block(amplifier->bass, amplifier->bass_target, frames);
    amplifier->middle =
        glide_block(amplifier->middle, amplifier->middle_target, frames);
    amplifier->treble =
        glide_block(amplifier->treble, amplifier->treble_target, frames);
    amplifier->presence = amplifier->presence_target;
    tone_coefficients =
        tone(amplifier->bass, amplifier->middle, amplifier->treble);
    presence_coefficients = presence(amplifier->presence);

    for (frame = 0; frame < frames; ++frame)
    {
        float sample;
        if (!enabled)
            continue;
        sample = audio[frame * 2] * 26.f * amplifier->gain * amplifier->gain;
        sample = filter(&amplifier->input_filter, input, sample);
        sample = nonlinear_stage(amplifier, sample);
        sample = filter(&amplifier->reconstruction_filter, reconstruction, sample);
        sample = filter(&amplifier->tone_filter, tone_coefficients, sample);
        sample =
            filter(&amplifier->presence_filter, presence_coefficients, sample);
        sample *= 8.f * amplifier->master;
        audio[frame * 2] = sample;
        audio[frame * 2 + 1] = sample;
    }
    return true;
}
