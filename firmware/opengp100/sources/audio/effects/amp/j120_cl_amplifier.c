#include "j120_cl_amplifier.h"

#include <math.h>
#include <string.h>

typedef struct j120_coefficients
{
    double b[4];
    double a[4];
} j120_coefficients_t;

static const float volume_bass_curve[100] = {
    0.0000f, 0.0000f, 0.0059f, 0.0099f, 0.0159f, 0.0199f, 0.0258f, 0.0298f,
    0.0358f, 0.0398f, 0.0458f, 0.0517f, 0.0557f, 0.0617f, 0.0657f, 0.0717f,
    0.0756f, 0.0816f, 0.0856f, 0.0916f, 0.0976f, 0.1016f, 0.1075f, 0.1115f,
    0.1175f, 0.1215f, 0.1275f, 0.1315f, 0.1375f, 0.1415f, 0.1475f, 0.1535f,
    0.1575f, 0.1635f, 0.1675f, 0.1736f, 0.1776f, 0.1837f, 0.1877f, 0.1938f,
    0.2000f, 0.2041f, 0.2102f, 0.2144f, 0.2206f, 0.2248f, 0.2312f, 0.2354f,
    0.2418f, 0.2462f, 0.2528f, 0.2594f, 0.2639f, 0.2708f, 0.2754f, 0.2824f,
    0.2872f, 0.2946f, 0.2995f, 0.3072f, 0.3150f, 0.3204f, 0.3286f, 0.3343f,
    0.3430f, 0.3490f, 0.3583f, 0.3647f, 0.3746f, 0.3815f, 0.3922f, 0.4034f,
    0.4112f, 0.4234f, 0.4318f, 0.4451f, 0.4543f, 0.4688f, 0.4789f, 0.4948f,
    0.5117f, 0.5235f, 0.5420f, 0.5550f, 0.5756f, 0.5900f, 0.6127f, 0.6287f,
    0.6539f, 0.6716f, 0.6996f, 0.7294f, 0.7503f, 0.7834f, 0.8066f, 0.8434f,
    0.8692f, 0.9101f, 0.9388f, 0.9842f};

static float linear_curve(uint8_t value)
{
    float index = floorf((float)value * 2.55f) - 2.f;
    if (index < 0.f)
        index = 0.f;
    return floorf(index / 251.f * 10000.f) * .0001f;
}

static float reversed_linear_curve(uint8_t value)
{
    unsigned index = 255u - (unsigned)floorf((float)value * 2.55f);
    float shifted = (float)index - 2.f;
    if (shifted < 0.f)
        shifted = 0.f;
    if (shifted > 251.f)
        shifted = 251.f;
    return floorf(shifted / 251.f * 10000.f) * .0001f;
}

static float glide(float current, float target)
{
    if (current < target)
        return fminf(current + .0001f, target);
    if (current > target)
        return fmaxf(current - .0001f, target);
    return target;
}

static j120_coefficients_t tone_coefficients(double bass, double middle,
                                             double treble)
{
    double bm = bass * middle;
    double bt = bass * treble;
    return (j120_coefficients_t){
        {middle * .0009766042931005359 + bass - treble * .00549272308126092 +
             bm * .17699097096920013 - bt * .9951800107955933 +
             .005512002855539322,
         bt * 2.9855401515960693 - bm * .45564138889312744 -
             bass * 2.9885427951812744 + treble * .013451507315039635 -
             middle * .0019769142381846905 - .01346351858228445,
         bass * 2.980720043182373 + middle * .001252598944120109 -
             treble * .01043015718460083 + bm * .3803098499774933 -
             bt * 2.9855401515960693 + .010410877875983715,
         bt * .9951800107955933 + treble * .0024713727179914713 -
             middle * .0002522891154512763 - bass * .9921773076057434 -
             bm * .10165943205356598 - .0024593619164079428},
        {bass * 1.3761073350906372 + middle * .0009766042931005359 -
             treble * 6.323562834288304e-22 + bm * .17699097096920013 +
             .007587861735373735,
         treble * 6.323562834288304e-22 - middle * .0019769142381846905 -
             bass * 3.364650249481201 - bm * .45564138889312744 -
             .014395514503121376,
         bass * 2.6046128273010254 + middle * .001252598944120109 +
             treble * 6.323562834288304e-22 + bm * .3803098499774933 +
             .00833702553063631,
         bass * -.6160699129104614 - middle * .0002522891154512763 -
             treble * 6.323562834288304e-22 - bm * .10165943205356598 -
             .0015253585297614336}};
}

static double tone_filter(gp100_j120_tone_filter_t *state,
                          j120_coefficients_t c, double input)
{
    double output = c.b[0] * input + c.b[1] * state->x[0] +
                    c.b[2] * state->x[1] + c.b[3] * state->x[2] -
                    c.a[1] * state->y[0] - c.a[2] * state->y[1] -
                    c.a[3] * state->y[2];
    output /= c.a[0];
    state->x[2] = state->x[1];
    state->x[1] = state->x[0];
    state->x[0] = input;
    state->y[2] = state->y[1];
    state->y[1] = state->y[0];
    state->y[0] = output;
    return output;
}

static float bright_filter(gp100_j120_cl_amplifier_t *amplifier,
                           float input)
{
    float square = amplifier->volume * amplifier->volume;
    float b0 = square - amplifier->volume * 1.0515357f;
    float b1 = amplifier->volume * .9484642f - square;
    float a0 = square - amplifier->volume - .051535767f;
    float a1 = amplifier->volume - square - .051535767f;
    float output =
        (input * b0 + amplifier->bright_x1 * b1 - amplifier->bright_y1 * a1) /
        a0;
    amplifier->bright_x1 = input;
    amplifier->bright_y1 = output;
    return output;
}

bool gp100_j120_cl_amplifier_reset(
    gp100_j120_cl_amplifier_t *amplifier, uint8_t volume, uint8_t bass,
    uint8_t middle, uint8_t treble, bool bright)
{
    if (amplifier == NULL || volume > 99u || bass > 99u || middle > 99u ||
        treble > 99u)
        return false;
    memset(amplifier, 0, sizeof(*amplifier));
    amplifier->volume = amplifier->volume_target = volume_bass_curve[volume];
    amplifier->bass = amplifier->bass_target = volume_bass_curve[bass];
    amplifier->middle = amplifier->middle_target = linear_curve(middle);
    amplifier->treble = amplifier->treble_target =
        reversed_linear_curve(treble);
    amplifier->bright = bright;
    amplifier->configured = true;
    return true;
}

bool gp100_j120_cl_amplifier_process(
    gp100_j120_cl_amplifier_t *amplifier, uint8_t volume, uint8_t bass,
    uint8_t middle, uint8_t treble, bool bright, float *audio, size_t frames,
    bool enabled)
{
    size_t n;
    if (amplifier == NULL || !amplifier->configured || volume > 99u ||
        bass > 99u || middle > 99u || treble > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    amplifier->volume_target = volume_bass_curve[volume];
    amplifier->bass_target = volume_bass_curve[bass];
    amplifier->middle_target = linear_curve(middle);
    amplifier->treble_target = reversed_linear_curve(treble);
    amplifier->bright = bright;
    for (n = 0u; n < frames; ++n)
    {
        double signal;
        amplifier->volume = glide(amplifier->volume, amplifier->volume_target);
        amplifier->bass = glide(amplifier->bass, amplifier->bass_target);
        amplifier->middle = glide(amplifier->middle, amplifier->middle_target);
        amplifier->treble = glide(amplifier->treble, amplifier->treble_target);
        if (!enabled)
            continue;
        signal =
            tone_filter(&amplifier->tone_filter,
                        tone_coefficients(amplifier->bass, amplifier->middle,
                                          amplifier->treble),
                        audio[n * 2u]);
        if (amplifier->bright)
            signal = bright_filter(amplifier, (float)(signal * 9.60000038));
        else
            signal *= amplifier->volume * 6.0;
        signal *= 4.54;
        audio[n * 2u] = (float)signal;
        audio[n * 2u + 1u] = (float)signal;
    }
    return true;
}
