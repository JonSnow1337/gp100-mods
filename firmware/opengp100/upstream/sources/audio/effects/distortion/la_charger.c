#include "la_charger.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>
#include <string.h>

typedef struct charger_coefficients { float b0, b1, b2, a0, a1, a2; }
    charger_coefficients_t;

static const float factory_volume[100] = {
    0.000000f, 0.000000f, 0.005900f, 0.009900f, 0.015900f,
    0.019900f, 0.025800f, 0.029800f, 0.035800f, 0.039800f,
    0.045800f, 0.051700f, 0.055700f, 0.061700f, 0.065700f,
    0.071700f, 0.075600f, 0.081600f, 0.085600f, 0.091600f,
    0.097600f, 0.101600f, 0.107500f, 0.111500f, 0.117500f,
    0.121500f, 0.127500f, 0.131500f, 0.137500f, 0.141500f,
    0.147500f, 0.153500f, 0.157500f, 0.163500f, 0.167500f,
    0.173600f, 0.177600f, 0.183700f, 0.187700f, 0.193800f,
    0.200000f, 0.204100f, 0.210200f, 0.214400f, 0.220600f,
    0.224800f, 0.231200f, 0.235400f, 0.241800f, 0.246200f,
    0.252800f, 0.259400f, 0.263900f, 0.270800f, 0.275400f,
    0.282400f, 0.287200f, 0.294600f, 0.299500f, 0.307200f,
    0.315000f, 0.320400f, 0.328600f, 0.334300f, 0.343000f,
    0.349000f, 0.358300f, 0.364700f, 0.374600f, 0.381500f,
    0.392200f, 0.403400f, 0.411200f, 0.423400f, 0.431800f,
    0.445100f, 0.454300f, 0.468800f, 0.478900f, 0.494800f,
    0.511700f, 0.523500f, 0.542000f, 0.555000f, 0.575600f,
    0.590000f, 0.612700f, 0.628700f, 0.653900f, 0.671600f,
    0.699600f, 0.729400f, 0.750300f, 0.783400f, 0.806600f,
    0.843400f, 0.869200f, 0.910100f, 0.938800f, 0.984200f
};

static float filter(gp100_la_charger_filter_t *state,
                    charger_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 +
                    state->x2 * c.b2 - state->y1 * c.a1 -
                    state->y2 * c.a2) / c.a0;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static float rational_shape(float input)
{
    float magnitude = fabsf(input);
    float square = magnitude * magnitude;
    float cube = square * magnitude;
    float output = (cube * 2.084f + square * 5.705f + magnitude * 5.959f) /
                   (cube + square * 4.923f - magnitude * 2.031f + 9.76f);

    return copysignf(output, input);
}

static float nonlinear_stage(gp100_la_charger_t *effect, float input)
{
    float step = (input - effect->previous_clip_input) / 3.0f;
    float shaped[4];
    size_t i;
    for (i = 0u; i < 4u; ++i)
        shaped[i] = rational_shape(effect->previous_clip_input +
                                   step * (float)i);
    effect->previous_clip_input = input;
    return gp100_decimate_four(effect->oversample_history, shaped);
}

bool gp100_la_charger_reset(gp100_la_charger_t *effect,
                                 uint8_t gain, uint8_t tone, uint8_t volume)
{
    if (effect == NULL || gain > 99u || tone > 99u || volume > 99u)
        return false;
    memset(effect, 0, sizeof(*effect));
    effect->configured = true;
    return true;
}

bool gp100_la_charger_process(gp100_la_charger_t *effect,
                                   uint8_t gain, uint8_t tone,
                                   uint8_t volume, float *audio,
                                   size_t frames, bool enabled)
{
    static const charger_coefficients_t post[3] = {
        {0.984099984f, -1.91050005f, 0.929600000f,
         1.0f, -1.91050005f, 0.913600028f},
        {0.986299992f, -1.85430002f, 0.880100012f,
         1.0f, -1.85430002f, 0.866400003f},
        {1.42770004f, -1.68710005f, 0.614300013f,
         1.0f, -0.999000013f, 0.353899986f}
    };
    float gain_value, tone_value, volume_value;
    charger_coefficients_t gain_c[2], tone_c;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || tone > 99u ||
        volume > 99u || (frames != 0u && audio == NULL) ||
        frames > SIZE_MAX / 2u) return false;
    gain_value = (float)gain * 0.008f;
    tone_value = 1.0f - (float)tone * 0.01f;
    volume_value = factory_volume[volume];
    gain_c[0] = (charger_coefficients_t){
        gain_value * 108.822f + 1.1337868f,
        0.111133784f - gain_value * 1.90198f,
        gain_value * -106.92001f - 1.0226531f,
        gain_value + 1.1337868f,
        0.111133784f - gain_value * 1.90198f,
        gain_value * 0.90198f - 1.0226531f};
    gain_c[1] = (charger_coefficients_t){
        1.01833f, 0.0f, -1.01833f,
        gain_value - 1.1011338f,
        1.9757365f - gain_value * 1.796334f,
        gain_value * 0.796334f - 0.87506455f};
    tone_c = (charger_coefficients_t){
        0.009777979f, 0.0019324298f, -0.007845549f,
        tone_value + 0.038526412f,
        tone_value * -1.8960314f - 0.017027885f,
        tone_value * 0.89603144f - 0.01763367f};
    for (n = 0u; n < frames; ++n) {
        float output;
        if (!enabled) continue;
        output = filter(&effect->gain_filter[0], gain_c[0], audio[n * 2u]);
        output = filter(&effect->gain_filter[1], gain_c[1], output);
        output = nonlinear_stage(effect, output);
        output = filter(&effect->post_filter[0], post[0], output);
        output = filter(&effect->post_filter[1], post[1], output);
        output = filter(&effect->post_filter[2], post[2], output);
        output = filter(&effect->tone_filter, tone_c, output);
        output *= volume_value;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
