#include "muff.h"

#include "audio/dsp/decimation_internal.h"

#include <math.h>

/* White Can Ram's Head 74#3. The transistor model uses emitter-degenerated
 * small-signal current around the 9 V DC bias, with collector rail limits.
 * The linear capacitors and both diode feedback loops run at 176.4 kHz. */
#define STEP_RATE 176400.0f
#define DIODE_CAP_CONDUCTANCE (0.12e-6f * STEP_RATE)

enum { DIODE_POINTS = 8 };
typedef struct diode_segment {
    float voltage;
    float current;
    float conductance;
} diode_segment_t;

typedef struct stage_circuit {
    float input_conductance;
    float input_cap_step;
    float feedback_conductance;
    float feedback_cap_conductance;
    float base_inverse;
    float base_from_input;
    float base_from_feedback;
    float collector_from_input;
    float collector_from_feedback;
    float low_rail;
    float high_rail;
} stage_circuit_t;

typedef struct tone_circuit {
    float high_arm;
    float low_arm;
    float inverse_total;
} tone_circuit_t;

static float control(float raw)
{
    return fminf(99.0f, fmaxf(0.0f, raw)) * (1.0f / 99.0f);
}

static float rail_limit(float voltage, float minimum, float maximum)
{
    if (voltage < minimum) {
        return minimum;
    }
    return voltage > maximum ? maximum : voltage;
}

static float parallel_resistance(float r1, float r2)
{
    return r1 * r2 / (r1 + r2);
}

static float dc_block(float value, float *charge, float step)
{
    const float ac = value - *charge;
    *charge += step * ac;
    return ac;
}

static stage_circuit_t stage_circuit(float input_resistance, float input_capacitance,
                                    float emitter_resistance, float feedback_capacitance,
                                    float collector_load)
{
    const float collector_resistance = 12000.0f;
    const float feedback_resistance = 470000.0f;
    const float bias_resistance = 100000.0f;
    const float beta = 250.0f;
    const float base_emitter = 0.65f;
    const float bias_factor = 1.0f + feedback_resistance / bias_resistance;
    const float bias_current = (9.0f - base_emitter * bias_factor) /
        (collector_resistance + bias_factor * emitter_resistance + feedback_resistance / beta);
    const float collector_bias = 9.0f - collector_resistance * bias_current;
    const float transconductance = 1.0f / (emitter_resistance + 0.02585f / bias_current);
    const float collector_g = 1.0f / collector_resistance + 1.0f / collector_load;
    const float base_g = 1.0f / bias_resistance + transconductance / beta;

    /* Backward-Euler capacitor companion models. A series input capacitor
     * is folded into the input resistance, and the collector-base capacitor
     * contributes to the feedback conductance and its history current. */
    const float cap_resistance = 1.0f / (STEP_RATE * input_capacitance);
    const float input_g = 1.0f / (input_resistance + cap_resistance);
    const float feedback_cap_g = STEP_RATE * feedback_capacitance;
    const float feedback_g = 1.0f / feedback_resistance + feedback_cap_g;
    const float a = input_g + base_g + feedback_g;
    const float b = collector_g + feedback_g;
    const float c = transconductance - feedback_g;
    const float inverse_det = 1.0f / (a * b + feedback_g * c);
    return (stage_circuit_t){
        .input_conductance = input_g, .input_cap_step = cap_resistance * input_g,
        .feedback_conductance = feedback_g, .feedback_cap_conductance = feedback_cap_g,
        .base_inverse = 1.0f / a,
        .base_from_input = b * inverse_det,
        .base_from_feedback = collector_g * inverse_det,
        .collector_from_input = -c * inverse_det,
        .collector_from_feedback = -(transconductance + input_g + base_g) * inverse_det,
        .low_rail = 0.2f + emitter_resistance * bias_current - collector_bias,
        .high_rail = 9.0f - collector_bias,
    };
}

static void prepare_diodes(const stage_circuit_t *circuit, diode_segment_t segments[DIODE_POINTS])
{
    /* Piecewise linear 1N914 Shockley curve (Is≈2.5 nA, n≈1.75 at 25 °C).
     * Each voltage includes the driving resistance seen by the antiparallel
     * pair. Interpolating the resulting current solves the feedback loop. */
    static const float current[DIODE_POINTS] =
        {0.0f, 0.2e-6f, 1.0e-6f, 5.0e-6f, 20.0e-6f, 100.0e-6f, 500.0e-6f, 2000.0e-6f};
    static const float voltage[DIODE_POINTS] =
        {0.0f, 0.1990f, 0.2711f, 0.3439f, 0.4066f, 0.4795f, 0.5526f, 0.6156f};
    const float series = circuit->base_from_feedback - circuit->collector_from_feedback +
                         1.0f / DIODE_CAP_CONDUCTANCE;
    for (unsigned i = 0; i < DIODE_POINTS; ++i) {
        segments[i].current = current[i];
        segments[i].voltage = voltage[i] + current[i] * series;
    }
    for (unsigned i = 0; i + 1 < DIODE_POINTS; ++i) {
        segments[i].conductance = (current[i + 1] - current[i]) /
                                  (segments[i + 1].voltage - segments[i].voltage);
    }
    segments[DIODE_POINTS - 1].conductance = segments[DIODE_POINTS - 2].conductance;
}

static float diode_current(const diode_segment_t segments[DIODE_POINTS], float voltage)
{
    const float magnitude = fabsf(voltage);
    unsigned i = 0u;
    while (i + 1u < DIODE_POINTS && magnitude > segments[i + 1u].voltage) {
        ++i;
    }
    const float current = segments[i].current +
        (magnitude - segments[i].voltage) * segments[i].conductance;
    return copysignf(current, voltage);
}

static float transistor(gp100_muff_stage_t *state, const stage_circuit_t *circuit,
                        const diode_segment_t *diodes, float input)
{
    const float source = circuit->input_conductance * (input - state->input_cap);
    float feedback = circuit->feedback_cap_conductance * state->feedback_cap;
    if (diodes != NULL) {
        const float open_voltage = source * (circuit->base_from_input -
                                              circuit->collector_from_input) +
            feedback * (circuit->base_from_feedback - circuit->collector_from_feedback);
        const float current = diode_current(diodes, open_voltage - state->diode_cap);
        state->diode_cap += current / DIODE_CAP_CONDUCTANCE;
        feedback -= current;
    }
    float base = circuit->base_from_input * source + circuit->base_from_feedback * feedback;
    const float collector = circuit->collector_from_input * source +
                            circuit->collector_from_feedback * feedback;
    const float output = rail_limit(collector, circuit->low_rail, circuit->high_rail);
    if (output != collector) {
        base = circuit->base_inverse *
            (source + feedback + circuit->feedback_conductance * output);
    }
    state->input_cap += circuit->input_cap_step * (input - base - state->input_cap);
    state->feedback_cap = base - output;
    return output;
}

/* The 100k tone pot loads both the 4 nF treble branch and the 6.8 nF bass
 * branch. C3 sees the input impedance of Q1 and its 390k/100k bias network. */
#define TONE_BRANCH_RESISTANCE 33000.0f
#define TONE_HIGH_G (4.0e-9f * STEP_RATE)
#define TONE_LOW_G (6.8e-9f * STEP_RATE)
#define TONE_HIGH_R (1.0f / (1.0f / TONE_BRANCH_RESISTANCE + TONE_HIGH_G))
#define TONE_LOW_R (1.0f / (1.0f / TONE_BRANCH_RESISTANCE + TONE_LOW_G))
#define RECOVERY_R 73000.0f
#define RECOVERY_CAP_R (1.0f / (0.12e-6f * STEP_RATE))
#define RECOVERY_G (1.0f / (RECOVERY_R + RECOVERY_CAP_R))

static tone_circuit_t tone_circuit(float tone)
{
    const float high_arm = 1.0f / (TONE_HIGH_R + (1.0f - tone) * 100000.0f);
    const float low_arm = 1.0f / (TONE_LOW_R + tone * 100000.0f);
    return (tone_circuit_t){high_arm, low_arm, 1.0f / (high_arm + low_arm + RECOVERY_G)};
}

static float tone_and_recovery(gp100_muff_t *effect, const tone_circuit_t *tone, float input)
{
    const float high = TONE_HIGH_G * TONE_HIGH_R * (input - effect->tone_high_cap);
    const float low = TONE_LOW_R * (input / TONE_BRANCH_RESISTANCE +
                                    TONE_LOW_G * effect->tone_low_cap);
    const float wiper = (high * tone->high_arm + low * tone->low_arm +
                         effect->recovery_cap * RECOVERY_G) * tone->inverse_total;
    const float high_node = high - (high - wiper) * tone->high_arm * TONE_HIGH_R;
    const float low_node = low - (low - wiper) * tone->low_arm * TONE_LOW_R;
    const float current = (wiper - effect->recovery_cap) * RECOVERY_G;
    effect->tone_high_cap = input - high_node;
    effect->tone_low_cap = low_node;
    effect->recovery_cap += current * RECOVERY_CAP_R;

    /* Q1 has Rc=12k, Re=3.3k, and the 100k volume pot as collector load
     * through C2. The modeled gain is (12k || 100k)/(3.3k + re). */
    const float collector = rail_limit(-3.17f * current * RECOVERY_R, -3.8f, 3.9f);
    return dc_block(collector, &effect->output_cap,
                     1.0f / (1.0f + STEP_RATE * 112000.0f * 10.0e-6f));
}

void gp100_muff_reset(gp100_muff_t *effect, const float controls[3])
{
    *effect = (gp100_muff_t){
        .sustain = control(controls[0]), .tone = control(controls[1]), .volume = control(controls[2]),
    };
}

void gp100_muff_process(gp100_muff_t *effect, const float controls[3],
                             float *audio, size_t frames, bool enabled)
{
    if (!enabled) {
        if (effect->enabled) {
            gp100_muff_reset(effect, controls);
        }
        return;
    }
    effect->enabled = true;
    effect->sustain += 0.24f * (control(controls[0]) - effect->sustain);
    effect->tone += 0.24f * (control(controls[1]) - effect->tone);
    const float final_volume = control(controls[2]);

    /* Use volts internally, with 1 V peak at digital full scale. Each
     * collector uses a lumped load for the following circuit. C4, the
     * linear Sustain pot and its 820 ohm floor precede C5/R19 and Q3. */
    const float pot_bottom = effect->sustain * 100000.0f + 820.0f;
    const float pot_top = (1.0f - effect->sustain) * 100000.0f;
    const float sustain_gain = pot_bottom / 100820.0f;
    const stage_circuit_t input = stage_circuit(33000.0f, 10.0e-6f, 100.0f, 220.0e-12f, 100820.0f);
    const stage_circuit_t clip1 = stage_circuit(10000.0f + parallel_resistance(pot_bottom, pot_top),
                                                0.12e-6f, 150.0f, 680.0e-12f, 47000.0f);
    const stage_circuit_t clip2 = stage_circuit(10000.0f, 10.0e-6f, 150.0f, 680.0e-12f, 47000.0f);
    const tone_circuit_t tone = tone_circuit(effect->tone);
    diode_segment_t diodes1[DIODE_POINTS], diodes2[DIODE_POINTS];
    prepare_diodes(&clip1, diodes1);
    prepare_diodes(&clip2, diodes2);

    for (size_t i = 0; i < frames; ++i, audio += 2) {
        const float sample = 0.5f * (audio[0] + audio[1]);
        const float step = 0.25f * (sample - effect->previous_input);
        float phase = effect->previous_input;
        float oversampled[4];
        for (unsigned j = 0u; j < 4u; ++j) {
            phase += step;
            const float stage1 = transistor(&effect->stage[0], &input, NULL, phase);
            const float drive = sustain_gain * dc_block(stage1, &effect->sustain_cap,
                1.0f / (1.0f + STEP_RATE * 112820.0f * 0.12e-6f));
            const float stage2 = transistor(&effect->stage[1], &clip1, diodes1, drive);
            const float stage3 = transistor(&effect->stage[2], &clip2, diodes2, stage2);
            oversampled[j] = tone_and_recovery(effect, &tone, stage3);
        }
        effect->previous_input = sample;
        effect->volume += 0.008f * (final_volume - effect->volume);
        /* Map the modeled pedal's output swing to the GP100's DSP range. */
        const float sample_out = 0.25f * effect->volume *
                                 gp100_decimate_four(effect->decimator, oversampled);
        audio[0] = sample_out;
        audio[1] = sample_out;
    }
}
