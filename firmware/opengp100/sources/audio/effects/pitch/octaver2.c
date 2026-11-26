#include "octaver2.h"

/* Bilinear transform at 44.1 kHz of U5D. R38/R39=220k, R40=1M,
 * C14=10n, R35/R37=10k, C5=470n, C26=2.2n. SW1B selects C16+C17
 * (68n+33n), C17 alone, or R8 (330k) in parallel with R38.
 * Treat the bias voltage as signal ground. */
typedef struct filter_section {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} filter_section_t;

static const filter_section_t tracking_filters[][2] = {
    {
        {-0.01390509852f, 0.001606840615f, 0.01551193914f, -1.315007418f, 0.3182811322f},
        {1.0f, -1.991141333f, 0.9911567297f, -1.995685570f, 0.9957006842f}
    },
    {
        {-0.01386546845f, 0.001789291823f, 0.01565476027f, -1.315007418f, 0.3182811322f},
        {1.0f, -1.979997476f, 0.9800397046f, -1.991475697f, 0.9915218580f}
    },
    {
        {0.005806933808f, 0.005898164669f, 0.00009123086138f, -0.3198204326f, 0.0f},
        {1.0f, -0.9975934098f, 0.0f, -1.986515205f, 0.9865569422f}
    }
};

/* Sampled capacitor-voltage equations at 44.1 kHz. Each row maps the
 * shunt capacitor, feedback capacitor, output capacitor and input voltage
 * to the next voltage of one capacitor. This keeps the stored voltages
 * continuous when a switch changes a resistance or capacitance. */
typedef struct voice_filter {
    float step[3][4];
} voice_filter_t;

/* U1C. R10=R16=68k, C18=33n and C21=3.3n. SW1A selects C19+C20 or
 * C19 alone (47n each). R18=68k is paralleled by R19=47k in High. */
static const voice_filter_t oct1_filters[] = {
    {{
        {0.9800109895f, 0.01047348893f, 0.009985324816f, 0.01000368565f},
        {0.003499251054f, 0.9927700026f, -0.003517012262f, 1.776120817e-05f},
        {0.0001776120817f, 0.1006874867f, 0.9998217883f, 5.996279169e-07f},
    }},
    {{
        {0.9800291415f, 0.01043639114f, 0.009967111933f, 0.01000374661f},
        {0.006973303283f, 0.9855915080f, -0.007008740843f, 3.543756016e-05f},
        {0.0003543756016f, 0.1003261773f, 0.9996444273f, 1.197112496e-06f},
    }},
    {{
        {0.9800307338f, 0.01110973691f, 0.009965515536f, 0.01000375064f},
        {0.006936396245f, 0.9750164665f, -0.006971709892f, 3.531364738e-05f},
        {0.0008640573295f, 0.2441841537f, 0.9991330212f, 2.921460767e-06f},
    }},
};

/* U1B. R14=R7=330k, C13=47n, C15=10n and C12=1n. C13 sees a source
 * resistance of R13 + (R11 || R12) with Q3 open and R13 with Q3 closed. */
static const voice_filter_t oct2_filters[] = {
    {{
        {0.9896579803f, 0.001494179665f, 0.001449336161f, 0.008892683527f},
        {0.006788461680f, 0.9861222828f, -0.006818926325f, 3.046464446e-05f},
        {0.0002341827456f, 0.06823908936f, 0.9997651181f, 6.992019121e-07f},
    }},
    {{
        {0.9768843899f, 0.001484617773f, 0.001439967718f, 0.0f},
        {0.006744531023f, 0.9861222610f, -0.006818947782f, 0.0f},
        {0.0002331725110f, 0.06823908899f, 0.9997651177f, 0.0f},
    }},
};

enum {
    MIN_EDGE_SPACING = 28,      /* Upper tracking limit of about 1.58 kHz. */
    TRACKING_TIMEOUT = 2205,   /* 50 ms, longer than the period of low B. */
};

static float run_section(gp100_biquad_channel_state_t *state,
                         const filter_section_t *coefficients, float input)
{
    const float output = coefficients->b0 * input +
        coefficients->b1 * state->x1 + coefficients->b2 * state->x2 -
        coefficients->a1 * state->y1 - coefficients->a2 * state->y2;
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;
    return output;
}

static float run_filter(gp100_biquad_channel_state_t state[2],
                        const filter_section_t coefficients[2], float input)
{
    return run_section(&state[1], &coefficients[1],
                       run_section(&state[0], &coefficients[0], input));
}

static float run_voice(float voltage[3], const voice_filter_t *filter, float input)
{
    float next[3];
    for (size_t i = 0; i < 3; ++i) {
        const float *step = filter->step[i];
        next[i] = step[0] * voltage[0] + step[1] * voltage[1] +
                  step[2] * voltage[2] + step[3] * input;
    }
    for (size_t i = 0; i < 3; ++i) {
        voltage[i] = next[i];
    }
    return voltage[2];
}

static float dc_block(float input, float *bias, float coefficient)
{
    const float centered = input - *bias;
    *bias += coefficient * centered;
    return centered;
}

static void clock_dividers(gp100_octaver2_t *state, float detector)
{
    /* U2C/U2D feed positive and negative peaks to a Schmitt trigger (U3A).
     * Capture peaks immediately and give them a 16 ms release. */
    const float release = 0.998584f;
    const float floor = 0.0005f;
    state->positive_peak *= release;
    state->negative_peak *= release;
    /* R33 || R34 and C24 set the rectifiers' reference time constant. */
    state->detector_reference += 0.003772f * (detector - state->detector_reference);
    if (state->detector_reference > state->positive_peak) {
        state->positive_peak = state->detector_reference;
    }
    if (-state->detector_reference > state->negative_peak) {
        state->negative_peak = -state->detector_reference;
    }
    if (detector < -(floor + 0.22f * state->negative_peak)) {
        state->armed = true;
    }
    if (state->samples_since_edge < TRACKING_TIMEOUT) {
        ++state->samples_since_edge;
    }
    if (state->armed && detector > floor + 0.22f * state->positive_peak) {
        state->armed = false;
        if (state->samples_since_edge >= MIN_EDGE_SPACING) {
            /* U3B and U4B divide the detector clock by two and four. */
            state->divider = state->samples_since_edge == TRACKING_TIMEOUT
                ? 0u : (state->divider + 1u) & 3u;
            state->samples_since_edge = 0;
        }
    }
}

static float mix_gain(float control)
{
    if (!(control > 0.0f)) {
        return 0.0f;
    }
    return control >= 99.0f ? 1.0f : control / 99.0f;
}

void gp100_octaver2_reset(gp100_octaver2_t *state, const float levels[3])
{
    *state = (gp100_octaver2_t){
        .levels = {mix_gain(levels[0]), mix_gain(levels[1]), mix_gain(levels[2])},
        .samples_since_edge = TRACKING_TIMEOUT,
    };
}

void gp100_octaver2_process(gp100_octaver2_t *state, const float controls[4],
                         float *stereo, size_t frames, bool enabled)
{
    if (!enabled) {
        if (state->active) {
            gp100_octaver2_reset(state, controls);
        }
        return;
    }
    state->active = true;
    const unsigned range = (controls[3] >= 0.5f) + (controls[3] >= 1.5f);
    const float levels[3] = {
        mix_gain(controls[0]), mix_gain(controls[1]), mix_gain(controls[2])
    };
    const filter_section_t *tracking_filter = tracking_filters[range];
    for (size_t i = 0; i < frames; ++i, stereo += 2) {
        /* The hardware input and divided voices are mono. Keep the dry
         * paths separate so this pedal can also follow a stereo effect. */
        const float left = dc_block(stereo[0], &state->input_bias[0], 0.001461f);
        const float right = dc_block(stereo[1], &state->input_bias[1], 0.001461f);
        const float input = 0.5f * (left + right);
        const float detector = run_filter(state->tracker, tracking_filter, input);
        clock_dividers(state, dc_block(detector, &state->tracker_bias, 0.001f));

        /* Q2 shunts U1D's noninverting input. The feedback term comes from
         * R3/R2 and R3*C11, and the output is scaled by 1/(R3/R2). Remove
         * its inversion here along with the inversion of the final mixer. */
        const float plus = (state->divider & 1u) ? input * (100.0f / 147.0f) : 0.0f;
        state->switch_feedback += 0.497f * ((input - plus) - state->switch_feedback);
        const float switched = state->switch_feedback - plus * (47.0f / 330.0f);
        const float oct1 = run_voice(state->oct1, &oct1_filters[range], switched);
        const unsigned q3_closed = (state->divider >> 1) & 1u;
        const float oct2 = run_voice(state->oct2, &oct2_filters[q3_closed],
                                     switched * (100.0f / 147.0f));

        const bool locked = state->samples_since_edge < TRACKING_TIMEOUT;
        state->gate += 0.008f * ((locked ? 1.0f : 0.0f) - state->gate);
        for (size_t control = 0; control < 3; ++control) {
            state->levels[control] += 0.008f * (levels[control] - state->levels[control]);
        }
        /* C4 couples Oct 1 into the mixer through R17. Preserve the
         * relative wet mix weights of R17 and R20 and normalize to GP100
         * signal levels. C2 and C7 couple the mixed output. */
        const float wet1 = dc_block(oct1, &state->oct1_bias, 0.0000768f);
        const float wet = state->gate * 2.0f * (state->levels[1] * wet1 +
                                   state->levels[2] * (47.0f / 33.0f) * oct2);
        stereo[0] = dc_block(state->levels[0] * left + wet, &state->output_bias[0], 0.000227f);
        stereo[1] = dc_block(state->levels[0] * right + wet, &state->output_bias[1], 0.000227f);
    }
}
