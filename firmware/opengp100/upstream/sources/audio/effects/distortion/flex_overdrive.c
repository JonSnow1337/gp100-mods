#include "flex_overdrive.h"

#include <math.h>
#include <string.h>

typedef struct flex_coefficients { float b0, b1, b2, a0, a1, a2; }
    flex_coefficients_t;

/* UI-reachable entries from the factory tables. Gain and Volume share the
 * first table; Tone uses the second. */
static const float factory_gain_volume[100] = {
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

static const float factory_tone[100] = {
    0.000000f, 0.000000f, 0.011900f, 0.019900f, 0.031800f,
    0.039800f, 0.051700f, 0.059700f, 0.071700f, 0.079600f,
    0.091600f, 0.103500f, 0.111500f, 0.123500f, 0.131400f,
    0.143400f, 0.151300f, 0.163300f, 0.171300f, 0.183200f,
    0.195200f, 0.203100f, 0.215100f, 0.223100f, 0.235000f,
    0.243000f, 0.254900f, 0.262900f, 0.274900f, 0.282800f,
    0.294800f, 0.306700f, 0.314700f, 0.326600f, 0.334600f,
    0.346600f, 0.354500f, 0.366500f, 0.374500f, 0.386400f,
    0.398400f, 0.406300f, 0.418300f, 0.426200f, 0.438200f,
    0.446200f, 0.458100f, 0.466100f, 0.478000f, 0.486000f,
    0.498000f, 0.509900f, 0.517900f, 0.529800f, 0.537800f,
    0.549800f, 0.557700f, 0.569700f, 0.577600f, 0.589600f,
    0.601500f, 0.609500f, 0.621500f, 0.629400f, 0.641400f,
    0.649400f, 0.661300f, 0.669300f, 0.681200f, 0.689200f,
    0.701100f, 0.713100f, 0.721100f, 0.733000f, 0.741000f,
    0.752900f, 0.760900f, 0.772900f, 0.780800f, 0.792800f,
    0.804700f, 0.812700f, 0.824700f, 0.832600f, 0.844600f,
    0.852500f, 0.864500f, 0.872500f, 0.884400f, 0.892400f,
    0.904300f, 0.916300f, 0.924300f, 0.936200f, 0.944200f,
    0.956100f, 0.964100f, 0.976000f, 0.984000f, 0.996000f
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

static float filter(gp100_flex_overdrive_filter_t *state,
                    flex_coefficients_t c, float input)
{
    float output = (input * c.b0 + state->x1 * c.b1 +
                    state->x2 * c.b2 - state->y1 * c.a1 -
                    state->y2 * c.a2) / c.a0;
    state->x2 = state->x1; state->x1 = input;
    state->y2 = state->y1; state->y1 = output;
    return output;
}

static float model_blend(float gain)
{
    float reciprocal = 1.0f / (gain * 10.0f + 1.0f);
    return 0.28669f - gain * 0.50126f + gain * gain * 0.15087f +
           reciprocal * 0.71538f - reciprocal * reciprocal * 0.002069f;
}

static void fitted_models(float input, float *model_a, float *model_b)
{
    float magnitude = fabsf(input);
    float t, r;
    if (input < -10.0f) {
        t = magnitude * 0.05f;
        r = magnitude / (magnitude + 20.0f);
        *model_a = -0.93159f - t * 0.019057f + t * t * 0.00069736f -
                   r * 0.44222f + r * r * 0.13554f;
        *model_b = -1.2265f - t * 0.023848f + t * t * 0.0007149f -
                   r * 0.46049f + r * r * 0.14868f;
    } else if (input < -0.8f) {
        t = magnitude * 0.1f;
        r = magnitude / (magnitude * 3.0f + 1.0f);
        *model_a = 1.1116f - t * 0.20523f + t * t * 0.068381f -
                   r * 10.9477f + r * r * 14.2768f;
        *model_b = 5.3947f - t * 0.41938f + t * t * 0.16498f -
                   r * 42.1935f + r * r * 68.223f;
    } else if (input < 0.0f) {
        *model_a = -0.5812f * input * input * input -
                   0.3721f * input * input + 0.9304f * input;
        *model_b = -0.0869f * input * input * input -
                   0.0746f * input * input + 0.9825f * input;
    } else if (input < 0.4f) {
        *model_a = -2.3592f * input * input * input +
                   0.5737f * input * input + 0.9577f * input;
        *model_b = -0.6352f * input * input * input +
                   0.2686f * input * input + 0.9699f * input;
    } else if (input < 8.0f) {
        t = magnitude * 0.1f;
        r = magnitude / (magnitude + 0.1f);
        *model_a = -0.71018f + t * 0.14449f - t * t * 0.067693f +
                   r * 1.7467f - r * r * 0.57407f;
        *model_b = -3.5658f + t * 0.16876f - t * t * 0.08061f +
                   r * 8.0192f - r * r * 3.8539f;
    } else {
        t = magnitude * 0.05f;
        r = magnitude / (magnitude + 2.0f);
        *model_a = 1.0441f + t * 0.012023f - t * t * 0.00051962f -
                   r * 1.5038f + r * r * 1.0668f;
        *model_b = 1.1889f + t * 0.014235f - t * t * 0.00051829f -
                   r * 1.4948f + r * r * 1.0644f;
    }
}

static float nonlinear_stage(gp100_flex_overdrive_t *effect,
                             float input, float blend)
{
    float step = (input - effect->previous_clip_input) / 3.0f;
    float shaped[4];
    float output = 0.0f;
    size_t i;
    for (i = 0u; i < 4u; ++i) {
        float model_a, model_b;
        fitted_models(effect->previous_clip_input + step * (float)i,
                      &model_a, &model_b);
        shaped[i] = model_a * (1.0f - blend) + model_b * blend;
    }
    memmove(&effect->oversample_history[4], effect->oversample_history,
            36u * sizeof(float));
    for (i = 0u; i < 4u; ++i) effect->oversample_history[i] = shaped[3u - i];
    for (i = 0u; i < 40u; ++i)
        output += reconstruction_fir[i] * effect->oversample_history[i];
    effect->previous_clip_input = input;
    return output;
}

static float glide(float current, float target)
{
    if (current < target) return fminf(current + 0.005f, target);
    if (current > target) return fmaxf(current - 0.005f, target);
    return target;
}

bool gp100_flex_overdrive_reset(gp100_flex_overdrive_t *effect,
                                     uint8_t gain, uint8_t tone,
                                     uint8_t volume, uint8_t mode,
                                     uint8_t blend)
{
    if (effect == NULL || gain > 99u || tone > 99u || volume > 99u ||
        mode > 2u || blend > 99u) return false;
    memset(effect, 0, sizeof(*effect));
    effect->volume = effect->volume_target = factory_gain_volume[volume];
    effect->blend = effect->blend_target = (float)blend * 0.01f;
    effect->configured = true;
    return true;
}

bool gp100_flex_overdrive_process(gp100_flex_overdrive_t *effect,
                                       uint8_t gain, uint8_t tone,
                                       uint8_t volume, uint8_t mode,
                                       uint8_t blend, float *audio,
                                       size_t frames, bool enabled)
{
    static const flex_coefficients_t input_c = {
        0.999886572f, -0.999886572f, 0.0f,
        1.0f, -0.999773324f, 0.0f};
    static const flex_coefficients_t mode_one = {
        0.975000024f, -1.89569998f, 0.930100024f,
        1.0f, -1.89569998f, 0.905200005f};
    static const flex_coefficients_t mode_two_a = {
        0.997600019f, -1.98230004f, 0.986199975f,
        1.0f, -1.98230004f, 0.983900011f};
    static const flex_coefficients_t mode_two_b = {
        1.14769995f, -1.52540004f, 0.517899990f,
        1.0f, -1.52540004f, 0.665600002f};
    float gain_value, tone_value, transfer_blend;
    flex_coefficients_t gain_c, tone_c;
    size_t n;
    if (effect == NULL || !effect->configured || gain > 99u || tone > 99u ||
        volume > 99u || mode > 2u || blend > 99u ||
        (frames != 0u && audio == NULL) || frames > SIZE_MAX / 2u)
        return false;
    gain_value = factory_gain_volume[gain];
    tone_value = factory_tone[tone];
    transfer_blend = model_blend(gain_value);
    gain_c = (flex_coefficients_t){
        gain_value * 207.7543f + 1.976445f,
        gain_value * -207.7543f - 1.929335f, 0.0f,
        1.0f, -0.9528902f, 0.0f};
    tone_c = (flex_coefficients_t){
        tone_value * tone_value * 0.10183299f -
            tone_value * 0.10183299f - 0.004531166f,
        -0.004276182f,
        tone_value * tone_value * -0.10183299f +
            tone_value * 0.10183299f + 0.00025498445f,
        tone_value * tone_value - tone_value * 0.9490835f - 0.09541255f,
        tone_value * tone_value * -1.796334f +
            tone_value * 1.796334f + 0.037937667f,
        tone_value * tone_value * 0.796334f -
            tone_value * 0.8472505f + 0.04892252f};
    effect->volume_target = factory_gain_volume[volume];
    effect->blend_target = (float)blend * 0.01f;
    effect->volume = glide(effect->volume, effect->volume_target);
    effect->blend = glide(effect->blend, effect->blend_target);
    for (n = 0u; n < frames; ++n) {
        float dry, wet, output;
        if (!enabled) continue;
        dry = filter(&effect->input_filter, input_c, audio[n * 2u]);
        wet = filter(&effect->gain_filter, gain_c, dry) * 1.3f;
        wet = nonlinear_stage(effect, wet, transfer_blend);
        wet = filter(&effect->tone_filter, tone_c, wet);
        if (mode == 1u)
            wet = filter(&effect->mode_filter[0], mode_one, wet);
        else if (mode == 2u) {
            wet = filter(&effect->mode_filter[1], mode_two_a, wet);
            wet = filter(&effect->mode_filter[2], mode_two_b, wet) * 0.8f;
        }
        output = (dry * 4.0f * (1.0f - effect->blend) +
                  wet * effect->blend) * 0.8f * effect->volume;
        audio[n * 2u] = output;
        audio[n * 2u + 1u] = output;
    }
    return true;
}
