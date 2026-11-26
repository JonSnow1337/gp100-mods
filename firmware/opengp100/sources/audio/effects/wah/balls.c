#include "balls.h"

#include <math.h>
#include <string.h>

/* Clean BassBalls signal path. The rectifier drives the control input
 * of both parallel resonant filters. Run the circuit at 2x. */
#define CIRCUIT_RATE 88200.0f
#define VIRTUAL_RESISTANCE 470000.0f
#define FILTER_INPUT_RESISTANCE (VIRTUAL_RESISTANCE * 0.5f)
#define SIGNAL_VOLTS 2.0f

typedef struct detector_circuit {
    float wiper;
    float input_g;
    float inverse_sum;
    float cap_step;
} detector_circuit_t;

static float response_control(float value)
{
    return fminf(99.0f, fmaxf(0.0f, value)) * (1.0f / 99.0f);
}

static float dc_block(float input, float *bias, float coefficient)
{
    const float output = input - *bias;
    *bias += coefficient * output;
    return output;
}

/* Q1 drives the 10k trimmers, set to 60% from ground for this model.
 * Small-signal collector conductance of Q2/Q3 in parallel with 47k,
 * derived at the DC operating point using the 2N5088 datasheet SPICE
 * model. Entries are in siemens, from 1.0 V in 0.25 V envelope steps.
 * Below 1.0 V the transistors add negligible conductance. */
static float filter_conductance(float envelope)
{
    static const float conductance[] = {
        0.000021280929f, 0.000022310758f, 0.000095296771f, 0.00044161711f,
        0.00089247864f, 0.0013839202f, 0.0019545184f, 0.0024164220f,
        0.0028718302f, 0.0033671734f, 0.0038698193f, 0.0043720625f,
        0.0048744821f, 0.0053769222f, 0.0058790676f, 0.0063803994f,
        0.0068806550f, 0.0073803461f, 0.0078789789f, 0.0083762617f,
        0.0088723272f, 0.0093672428f, 0.0098609605f, 0.0103535750f,
        0.0108442230f,
    };
    if (!(envelope >= 1.0f)) {
        return 1.0f / 47000.0f;
    }
    /* The 7.5 V rectifier ceiling and 0.55 V diode drop bound this index. */
    const float position = 4.0f * (envelope - 1.0f);
    const unsigned index = (unsigned)position;
    return conductance[index] + (conductance[index + 1u] - conductance[index]) *
           (position - (float)index);
}

static float envelope_step(gp100_balls_t *effect, float rectifier)
{
    /* D1 charges C4 through R6. R7 and the load at Q1's base discharge
     * it between peaks. Limit the charging current to model the op amp. */
    const float cap_step = 1.0f / (CIRCUIT_RATE * 4.7e-6f);
    const float charge = (rectifier - 0.55f - effect->envelope) / 100.0f;
    const float charging_current = charge < 0.0f ? 0.0f
                                  : charge > 0.015f ? 0.015f : charge;
    effect->envelope += cap_step * (charging_current - effect->envelope / 306000.0f);
    return filter_conductance(effect->envelope);
}

static float rectify(gp100_balls_t *effect, const detector_circuit_t *detector, float input)
{
    /* IC1B uses C3/R3 and the loaded P1 wiper as its input. The
     * inverting node leaves virtual ground when the output saturates. */
    const float source = detector->wiper * input - effect->detector_cap;
    const float current = detector->input_g * source;
    const float raw = -220000.0f * current;
    const float output = raw < 0.0f ? 0.0f : raw > 7.5f ? 7.5f : raw;
    const float inverting_node = raw == output ? 0.0f
        : (current + output / 220000.0f) * detector->inverse_sum;
    effect->detector_cap += detector->cap_step * (source - inverting_node);
    return output;
}

static float resonant_filter(gp100_balls_filter_t *filter, float input,
                             float capacitance, float conductance)
{
    /* Each 470k/470k op amp stage shares a variable shunt at the join
     * between its two equal capacitors. With s = d/dt its gain is
     * -(2*C*s+G)/(R*C*C*s*s+2*C*s+G). Integrate the two capacitor
     * voltages with the trapezoidal rule as G changes under the envelope. */
    const float k = 1.0f / (2.0f * CIRCUIT_RATE * VIRTUAL_RESISTANCE * capacitance);
    const float shunt = 2.0f + conductance / (2.0f * CIRCUIT_RATE * capacitance);
    const float v1 = filter->input_cap_history;
    const float v2 = filter->feedback_cap_history;
    const float raw = ((shunt - 1.0f) * v1 - v2 - shunt * k * input) /
                      (1.0f + shunt * k);
    const float output = raw < -3.4f ? -3.4f : raw > 3.4f ? 3.4f : raw;
    /* Keep the capacitor voltages consistent when the op amp reaches a rail. */
    const float cap1 = output == raw ? v1 - k * (input + output)
                                     : (output + v1 + v2) / shunt;
    filter->input_cap_history = 2.0f * cap1 - v1;
    filter->feedback_cap_history = 2.0f * (cap1 - output) - v2;
    return output;
}

static float circuit_step(gp100_balls_t *effect, const detector_circuit_t *detector,
                          float input)
{
    /* C1/R1 and the buffered C2/P1 coupling. R2 is behind the high
     * impedance op amp input so it does not set the input time constant. */
    const float buffered = dc_block(input, &effect->input_cap,
        1.0f / (1.0f + CIRCUIT_RATE * 220000.0f * 47.0e-9f));
    const float dry = dc_block(buffered, &effect->buffer_cap,
        1.0f / (1.0f + CIRCUIT_RATE * 10000.0f * 10.0e-6f));
    const float conductance = envelope_step(effect, rectify(effect, detector, dry));
    const float signal = dc_block(dry, &effect->clean_cap,
        1.0f / (1.0f + CIRCUIT_RATE * FILTER_INPUT_RESISTANCE * 47.0e-9f));
    const float low = resonant_filter(&effect->filter[0], signal, 22.0e-9f, conductance);
    const float high = resonant_filter(&effect->filter[1], signal, 4.7e-9f, conductance);

    /* R11/R16 mix the two outputs into C8 and R20. Flip the fixed
     * inversion of the analog mono circuit for use inside the patch. */
    const float sum = dc_block(0.5f * (low + high), &effect->output_cap,
        1.0f / (1.0f + CIRCUIT_RATE * (1350.0f + 47000.0f) * 1.0e-6f));
    return -0.27f * sum;
}

static float decimate(float history[15], float first, float second)
{
    /* Symmetric 15-tap half-band low-pass, followed by 2:1 decimation. */
    memmove(history + 2, history, 13u * sizeof(*history));
    history[0] = second;
    history[1] = first;
    return 0.5f * history[7]
        + 0.31289f * (history[6] + history[8])
        - 0.08443f * (history[4] + history[10])
        + 0.03204f * (history[2] + history[12])
        - 0.01050f * (history[0] + history[14]);
}

void gp100_balls_reset(gp100_balls_t *effect, float response)
{
    *effect = (gp100_balls_t){.response = response_control(response)};
}

void gp100_balls_process(gp100_balls_t *effect, float response,
                            float *audio, size_t frames, bool enabled)
{
    if (!enabled) {
        if (effect->enabled) {
            gp100_balls_reset(effect, response);
        }
        return;
    }
    effect->enabled = true;
    effect->response += 0.25f * (response_control(response) - effect->response);

    /* C3's series impedance includes R3 and the P1 wiper resistance. */
    const float r3_and_wiper = 4700.0f + 10000.0f * effect->response * (1.0f - effect->response);
    const float cap_r = 1.0f / (CIRCUIT_RATE * 0.47e-6f);
    const float input_g = 1.0f / (r3_and_wiper + cap_r);
    const detector_circuit_t detector = {
        .wiper = effect->response, .input_g = input_g,
        .inverse_sum = 1.0f / (input_g + 1.0f / 220000.0f + 1.0f / 2200000.0f),
        .cap_step = cap_r * input_g,
    };
    for (size_t i = 0; i < frames; ++i, audio += 2) {
        const float input = (0.5f * SIGNAL_VOLTS) * (audio[0] + audio[1]);
        const float midpoint = 0.5f * (effect->previous_sample + input);
        const float first = circuit_step(effect, &detector, midpoint);
        const float second = circuit_step(effect, &detector, input);
        const float output = decimate(effect->decimator, first, second);
        effect->previous_sample = input;
        audio[0] = output;
        audio[1] = output;
    }
}
