#ifndef GP100_LAZARO_NONLINEARITY_H
#define GP100_LAZARO_NONLINEARITY_H

typedef struct lazaro_diode_models {
    float model_a;
    float model_b;
} lazaro_diode_models_t;

/* The factory evaluates both diode fits together, then Gain crossfades them.
 * Their asymmetric region boundaries are part of the model, not clipping
 * limits: the negative side changes at -4 and -0.6, while the positive side
 * changes at 0.45 and 5. */
static inline lazaro_diode_models_t lazaro_diode_models(float input)
{
    float magnitude = input < 0.0f ? -input : input;
    float scaled;
    float ratio;
    lazaro_diode_models_t result;

    if (input < -4.0f) {
        scaled = magnitude * 0.01f;
        ratio = magnitude / (magnitude + 20.0f);
        result.model_b = ratio * 0.35034f * ratio -
            (scaled * 0.24912f + 0.92866f -
             scaled * 0.066952f * scaled + ratio * 0.5088f);
        result.model_a = ratio * 0.31759f * ratio -
            (scaled * 0.19427f + 0.6172f -
             scaled * 0.062215f * scaled + ratio * 0.4787f);
    } else if (input < -0.6f) {
        scaled = magnitude * 0.0625f;
        ratio = (magnitude + 1.0f) / (magnitude * 10.0f);
        result.model_b = ratio * 12.0137f * ratio -
            (scaled * 0.84431f + 0.72877f -
             scaled * 1.0797f * scaled + ratio * 2.5778f);
        result.model_a = ratio * 0.07924f * ratio -
            (scaled * 0.46524f + 0.7399f -
             scaled * 0.50744f * scaled - ratio * 1.0627f);
    } else if (input < 0.0f) {
        ratio = magnitude / (magnitude * 2.0f + 1.0f);
        result.model_b = ratio * 1.481f * ratio -
            (magnitude * 1.7583f - magnitude * 0.42555f * magnitude -
             ratio * 0.73212f);
        result.model_a = ratio * 0.96583f * ratio -
            (magnitude * 2.5806f - magnitude * 1.5563f * magnitude -
             ratio * 1.6452f);
    } else if (input < 0.45f) {
        ratio = magnitude / (magnitude * 2.0f + 1.0f);
        result.model_b = magnitude * 1.9724f -
            magnitude * 0.59242f * magnitude - ratio * 0.95371f -
            ratio * 1.6991f * ratio;
        result.model_a = magnitude * 7.2213f -
            magnitude * 4.4974f * magnitude - ratio * 6.196f -
            ratio * 8.8326f * ratio;
    } else if (input < 5.0f) {
        scaled = magnitude * 0.0625f;
        ratio = (magnitude + 1.0f) / (magnitude * 10.0f);
        result.model_b = scaled * 0.57267f + 0.56178f -
            scaled * 0.69257f * scaled + ratio * 1.3763f -
            ratio * 5.5328f * ratio;
        result.model_a = scaled * 0.28434f + 0.56035f -
            scaled * 0.27776f * scaled - ratio * 0.52409f -
            ratio * 0.22476f * ratio;
    } else {
        scaled = magnitude * 0.01f;
        ratio = magnitude / (magnitude + 20.0f);
        result.model_b = scaled * 0.14893f + 0.70332f -
            scaled * 0.032962f * scaled + ratio * 0.28615f -
            ratio * 0.17428f * ratio;
        result.model_a = scaled * 0.10363f + 0.50426f -
            scaled * 0.031526f * scaled + ratio * 0.2763f -
            ratio * 0.16398f * ratio;
    }
    return result;
}

static inline float lazaro_blended_nonlinearity(float input, float blend)
{
    lazaro_diode_models_t models = lazaro_diode_models(input);
    return models.model_a * (1.0f - blend) + models.model_b * blend;
}

#endif
