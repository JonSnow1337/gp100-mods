#ifndef GP100_CHIEF_NONLINEARITY_H
#define GP100_CHIEF_NONLINEARITY_H

/* Recovered odd-symmetric transfer curve used by the Chief distortion.
 *
 * The factory implementation selects the magnitude without branching and
 * evaluates three rational-polynomial regions.  Keeping the regions explicit
 * makes their two hand-off points visible without retaining compiler-shaped
 * integer comparisons against floating-point bit patterns. */
static inline float chief_nonlinearity(float input)
{
    float magnitude = input < 0.0f ? -input : input;
    float output;

    if (magnitude < 0.5f) {
        float ratio = magnitude / (magnitude * 2.0f + 1.0f);
        output = magnitude * 2.184f - magnitude * 0.75263f * magnitude -
                 ratio * 1.2455f - ratio * 2.301f * ratio;
    } else if (magnitude < 6.0f) {
        float scaled = magnitude * 0.0625f;
        float ratio = (magnitude + 1.0f) / (magnitude * 10.0f);
        output = scaled * 0.37569f + 0.60633f -
                 scaled * 0.3562f * scaled + ratio * 0.91023f -
                 ratio * 4.9234f * ratio;
    } else {
        float scaled = magnitude / 100.0f;
        float ratio = magnitude / (magnitude + 20.0f);
        output = scaled * 0.051805f + 0.68293f -
                 scaled * 0.0034161f * scaled + ratio * 0.23037f -
                 ratio * 0.054325f * ratio;
    }

    return input < 0.0f ? -output : output;
}

#endif
