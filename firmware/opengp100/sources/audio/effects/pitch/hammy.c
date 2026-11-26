#include "hammy.h"

#include <math.h>
#include <string.h>

#define HAMMY_SAMPLE_RATE 44100.0f
#define HAMMY_TWO_PI 6.2831855f
#define HAMMY_PERIOD_SEARCH_INTERVAL 64u
#define HAMMY_PERIOD_MINIMUM 30.0f
#define HAMMY_PERIOD_CONTINUITY_LIMIT 0.02f
#define HAMMY_PERIOD_STABILITY_LIMIT 0.03f
#define HAMMY_INITIAL_PERIOD 100.0f
#define HAMMY_FACTORY_LOOKAHEAD 220
#define HAMMY_HISTORY_COMPACTION_SAMPLES 256
#define HAMMY_HISTORY_MARGIN 20.0f
#define HAMMY_SPLICE_SPAN_SCALE 256.0f
#define HAMMY_LOW_PASS_LIMIT 22000.0f

/* Exact Range endpoint maps: -2 Oct, -1 Oct, +1 Oct, +2 Oct, +/-1, +/-2. */
static const int8_t range_low_semitones[6] = {0, 0, 0, 0, -12, -24};
static const int8_t range_high_semitones[6] = {-24, -12, 12, 24, 12, 24};

static bool parameters_valid(uint8_t range, uint8_t volume, uint8_t position)
{
    return range <= 5u && volume <= 99u && position <= 99u;
}

static float move_toward(float current, float target, float step)
{
    if (current < target) {
        current += step;
        return current > target ? target : current;
    }
    if (current > target) {
        current -= step;
        return current < target ? target : current;
    }
    return current;
}

static void configure_filter(
    gp100_hammy_filter_t *filter, float cutoff, bool high_pass)
{
    float omega = cutoff * (1.0f / HAMMY_SAMPLE_RATE) * HAMMY_TWO_PI;
    float sine = sinf(omega);
    float cosine = cosf(omega);
    float scale = 1.0f / (sine / 1.414f + 1.0f);
    float gain = high_pass ? 1.0f + cosine : 1.0f - cosine;

    filter->b0 = gain * 0.5f * scale;
    filter->b1 = (high_pass ? -gain : gain) * scale;
    filter->b2 = filter->b0;
    filter->a1 = -2.0f * cosine * scale;
    filter->a2 = (1.0f - sine / 1.414f) * scale;
}

static float filter_sample(gp100_hammy_filter_t *filter, float input)
{
    float output = input * filter->b0 + filter->x1 * filter->b1 +
                   filter->x2 * filter->b2 - filter->y1 * filter->a1 -
                   filter->y2 * filter->a2;

    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    return output;
}

static void set_targets(gp100_hammy_t *hammy,
                        uint8_t range,
                        bool harmony,
                        uint8_t volume,
                        uint8_t position)
{
    hammy->range = range;
    hammy->harmony = harmony;
    hammy->volume_target = (float)volume * 0.01f;
    hammy->position_target = (float)position * 0.01f;
}

static void update_controls(gp100_hammy_t *hammy, size_t frame_count)
{
    float frames = (float)frame_count;
    float harmony_target = hammy->harmony ? 0.5f : 1.0f;
    float low;
    float high;

    hammy->volume = move_toward(
        hammy->volume, hammy->volume_target, frames * 0.001f);
    hammy->harmony_mix = move_toward(
        hammy->harmony_mix, harmony_target, frames * 0.001f);
    hammy->position = move_toward(
        hammy->position, hammy->position_target, frames * 0.01f);
    low = (float)range_low_semitones[hammy->range];
    high = (float)range_high_semitones[hammy->range];
    hammy->semitones = low + hammy->position * (high - low);
    hammy->ratio = powf(2.0f, hammy->semitones / 12.0f);
}

/* Keep the factory's linear coordinates for the detector while audio stays
 * in a circular history. During detection, write_index holds the newest sample. */
static float history_sample(const gp100_hammy_t *hammy, int sample)
{
    int slot = (int)hammy->write_index - (hammy->history_position - sample);

    if (slot < 0) {
        slot += (int)GP100_HAMMY_DELAY_SAMPLES;
    } else if (slot >= (int)GP100_HAMMY_DELAY_SAMPLES) {
        slot -= (int)GP100_HAMMY_DELAY_SAMPLES;
    }
    return hammy->delay[slot];
}

static void discard_old_crossings(gp100_hammy_t *hammy, size_t count)
{
    hammy->crossing_count -= count;
    memmove(hammy->crossings, hammy->crossings + count,
            hammy->crossing_count * sizeof(hammy->crossings[0]));
    hammy->crossing_anchor -= (int)count;
    hammy->pending_crossing_anchor -= (int)count;
    if (hammy->crossing_anchor < 10 && hammy->crossing_count > 10u) {
        hammy->crossing_anchor = (int)hammy->crossing_count - 1;
    }
}

static void advance_history_position(gp100_hammy_t *hammy)
{
    size_t stale = 0u;
    size_t index;

    ++hammy->history_position;
    if (hammy->history_position < (int)GP100_HAMMY_DELAY_SAMPLES) {
        return;
    }
    /* Rebase only the metadata. The audio ring already retained these samples. */
    hammy->history_position -= HAMMY_HISTORY_COMPACTION_SAMPLES;
    if (hammy->read_position >= HAMMY_HISTORY_COMPACTION_SAMPLES) {
        hammy->read_position -= HAMMY_HISTORY_COMPACTION_SAMPLES;
    }
    if (hammy->splice_old_position >= HAMMY_HISTORY_COMPACTION_SAMPLES) {
        hammy->splice_old_position -= HAMMY_HISTORY_COMPACTION_SAMPLES;
    }
    if (hammy->hard_transient_marker > 0) {
        hammy->hard_transient_marker -= HAMMY_HISTORY_COMPACTION_SAMPLES;
    }
    for (index = 0u; index < hammy->crossing_count; ++index) {
        hammy->crossings[index] -= HAMMY_HISTORY_COMPACTION_SAMPLES;
        if (hammy->crossings[index] < HAMMY_FACTORY_LOOKAHEAD) {
            ++stale;
        }
    }
    discard_old_crossings(hammy, stale);
}

static void update_transient_envelope(
    gp100_hammy_t *hammy,
    float shifted)
{
    float previous =
        hammy->transient_envelope[hammy->transient_envelope_position];
    float magnitude = fabsf(shifted);
    float coefficient = magnitude < previous ? 0.0024880767f : 0.99314183f;
    float envelope = previous + (magnitude - previous) * coefficient;
    float delayed;
    float ratio;
    bool hard_transient;
    bool soft_transient;

    hammy->transient_envelope_position =
        (hammy->transient_envelope_position + 1u) % 100u;
    delayed = hammy->transient_envelope[hammy->transient_envelope_position];
    ratio = delayed > 0.0f
        ? envelope / delayed
        : (envelope > 0.0f ? 1000000.0f : 0.0f);
    hard_transient = ratio >= 3.2f && envelope >= 0.01f;
    if (hard_transient && !hammy->hard_transient) {
        hammy->hard_transient_marker = hammy->crossing_count == 0u
            ? 0 : (int)hammy->crossings[hammy->crossing_count - 1u];
    }
    hammy->hard_transient = hard_transient;
    soft_transient = ratio >= 1.6f && envelope >= 0.001f;
    if (soft_transient && !hammy->soft_transient) {
        hammy->transient_hold = 4410u;
    } else if (hammy->transient_hold != 0u) {
        --hammy->transient_hold;
    }
    hammy->soft_transient = soft_transient;
    hammy->transient_envelope[hammy->transient_envelope_position] = envelope;
}

static float period_score(
    const gp100_hammy_t *hammy,
    float anchor,
    float candidate)
{
    int anchor_start = (int)anchor - HAMMY_FACTORY_LOOKAHEAD;
    int candidate_start = (int)candidate - HAMMY_FACTORY_LOOKAHEAD;
    int offset;
    float score = 0.0f;

    /* Compare every fourth sample over two lookahead windows centered
     * on the integer positions of the two crossings. */
    for (offset = 0; offset < 2 * HAMMY_FACTORY_LOOKAHEAD; offset += 4) {
        score += fabsf(
            history_sample(hammy, anchor_start + offset) -
            history_sample(hammy, candidate_start + offset));
    }
    if (fabsf((anchor - candidate) / hammy->raw_period - 1.0f) <=
        HAMMY_PERIOD_CONTINUITY_LIMIT) {
        score = (float)((double)score * 0.8);
    }
    return score;
}

static void stabilize_period(gp100_hammy_t *hammy)
{
    float period = hammy->raw_period;
    float last = hammy->last_raw_period;
    float previous = hammy->previous_raw_period;

    if (period == last) {
        return;
    }
    hammy->last_raw_period = period;
    hammy->previous_raw_period = last;
    hammy->detected_period = period;
    if (fabsf(period - last) / period <= HAMMY_PERIOD_STABILITY_LIMIT &&
        fabsf(period - previous) / period <= HAMMY_PERIOD_STABILITY_LIMIT) {
        hammy->detected_period = (period + last + previous) / 3.0f;
    }
}

static void score_period_candidates(gp100_hammy_t *hammy)
{
    bool attack_marked = hammy->hard_transient_marker > 0;
    bool short_search = attack_marked && hammy->ratio > 1.0f;
    int headroom = short_search ? 4 : 2;
    int lookback = short_search ? 20 : 40;
    int oldest = (int)hammy->crossing_count - headroom - lookback - 1;
    int candidate_index;
    float anchor;
    float previous_period = hammy->raw_period;
    float best_period = 0.0f;
    float best_score = 10000.0f;

    if (hammy->crossing_anchor < headroom) {
        return;
    }
    if (oldest < 0) {
        oldest = 0;
    }
    anchor = hammy->crossings[hammy->crossing_anchor];
    for (candidate_index = hammy->crossing_anchor - headroom;
         candidate_index >= oldest; candidate_index -= 2) {
        float crossing = hammy->crossings[candidate_index];
        float period = anchor - crossing;
        float score;

        if (period < HAMMY_PERIOD_MINIMUM) {
            continue;
        }
        if (crossing < HAMMY_FACTORY_LOOKAHEAD ||
            (!attack_marked && previous_period > HAMMY_PERIOD_MINIMUM &&
             (double)period > (double)previous_period * 4.5)) {
            break;
        }
        if (!attack_marked && previous_period > 500.0f &&
            (double)period < (double)previous_period / 2.5) {
            continue;
        }
        score = period_score(hammy, anchor, crossing);
        if (score < best_score) {
            best_score = score;
            best_period = period;
        }
    }
    if (best_period != 0.0f) {
        float ratio = best_period / previous_period;
        bool octave_error = fabsf(ratio - 2.0f) <= HAMMY_PERIOD_STABILITY_LIMIT ||
                            fabsf(ratio - 3.0f) <= HAMMY_PERIOD_STABILITY_LIMIT ||
                            fabsf(ratio - 4.0f) <= HAMMY_PERIOD_STABILITY_LIMIT;

        if (hammy->transient_hold != 0u || !octave_error) {
            hammy->raw_period = best_period;
            ++hammy->period_search_count;
        }
    }
}

static float scan_peak(const gp100_hammy_t *hammy, int start)
{
    float peak = 0.0f;
    int offset;

    for (offset = 0; offset < HAMMY_FACTORY_LOOKAHEAD; offset += 3) {
        float sample = fabsf(history_sample(hammy, start + offset));

        if (sample > peak) {
            peak = sample;
        }
    }
    return peak;
}

static void update_extremum_gate(
    gp100_hammy_t *hammy,
    int center_index,
    float center)
{
    bool positive = center > 0.0f;
    bool negative = center < 0.0f;
    float threshold;
    int offset;

    if (hammy->extremum_armed) {
        return;
    }
    for (offset = 1; offset <= 3; ++offset) {
        float before = history_sample(hammy, center_index - offset);
        float after = history_sample(hammy, center_index + offset);

        positive = positive && center > before && center > after;
        negative = negative && center < before && center < after;
    }
    if (!positive && !negative) {
        return;
    }
    if (hammy->peak_scan_pending) {
        hammy->recent_peak = scan_peak(hammy, center_index + 2);
        hammy->earlier_peak = scan_peak(
            hammy, center_index + 2 - HAMMY_FACTORY_LOOKAHEAD);
        hammy->peak_scan_pending = false;
    }
    threshold = hammy->hard_transient_marker > 0 ? 0.05f : 0.3f;
    hammy->extremum_armed = fabsf(center) > threshold * hammy->recent_peak ||
                            fabsf(center) > threshold * hammy->earlier_peak;
    if (hammy->extremum_armed && hammy->pending_crossing_anchor > 0) {
        hammy->crossing_anchor = hammy->pending_crossing_anchor;
    }
    hammy->pending_crossing_anchor = -1;
}

static void update_period_detector(gp100_hammy_t *hammy)
{
    int center_index = hammy->history_position - HAMMY_FACTORY_LOOKAHEAD - 1;
    float center = history_sample(hammy, center_index);
    float next = history_sample(hammy, center_index + 1);

    update_extremum_gate(hammy, center_index, center);
    if (center * next < 0.0f) {
        float fraction = center / (center - next);

        if (isfinite(fraction)) {
            float crossing = fraction + (float)hammy->history_position;

            hammy->crossings[hammy->crossing_count] =
                (crossing - HAMMY_FACTORY_LOOKAHEAD) - 1.0f;
            if (hammy->extremum_armed) {
                hammy->pending_crossing_anchor = (int)hammy->crossing_count;
                hammy->extremum_armed = false;
            }
            ++hammy->crossing_count;
            if (hammy->crossing_count == GP100_HAMMY_CROSSING_CAPACITY) {
                discard_old_crossings(hammy, 1u);
            }
        }
    }
    ++hammy->detector_sample_count;
    if ((hammy->detector_sample_count % HAMMY_PERIOD_SEARCH_INTERVAL) == 0u) {
        score_period_candidates(hammy);
    }
    stabilize_period(hammy);
}

static float interpolate_history(const gp100_hammy_t *hammy, float position)
{
    int index = (int)position;
    float fraction = position - (float)index;

    return history_sample(hammy, index) * (1.0f - fraction) +
           history_sample(hammy, index + 1) * fraction;
}

static void begin_splice(
    gp100_hammy_t *hammy,
    float span)
{
    hammy->splice_old_position = hammy->read_position;
    hammy->splice_phase = 0.0f;
    hammy->splice_span = fmaxf(1.0f, span);
    hammy->splice_active = true;
    ++hammy->splice_count;
}

static void schedule_splice(gp100_hammy_t *hammy)
{
    float period = hammy->detected_period;
    float base_span = hammy->ratio * HAMMY_SPLICE_SPAN_SCALE;

    if (hammy->ratio > 1.0f) {
        float distance = (float)hammy->history_position - hammy->read_position;

        if (distance < HAMMY_FACTORY_LOOKAHEAD) {
            begin_splice(hammy, fminf(base_span, fminf(distance, period)));
            hammy->read_position -= period;
            if (hammy->read_position >= (float)hammy->hard_transient_marker) {
                hammy->hard_transient_marker = -1;
            }
        }
    } else if (!hammy->splice_active) {
        float limit = period + HAMMY_HISTORY_MARGIN;
        int distance = hammy->history_position - (int)hammy->read_position;

        if ((float)distance <= limit) {
            return;
        }
        begin_splice(hammy, base_span);
        do {
            hammy->read_position += period;
            distance = hammy->history_position - (int)hammy->read_position;
        } while ((float)distance > limit);
    }
}

static float render_splice(gp100_hammy_t *hammy)
{
    float new_sample = interpolate_history(hammy, hammy->read_position);

    if (hammy->splice_active) {
        float old_sample = interpolate_history(
            hammy, hammy->splice_old_position + hammy->splice_phase);
        float weight = 0.5f - 0.5f * cosf(
            HAMMY_TWO_PI * hammy->splice_phase / hammy->splice_span);
        float output = old_sample * (1.0f - weight) + new_sample * weight;

        hammy->splice_phase += hammy->ratio;
        if (hammy->splice_phase >= hammy->splice_span * 0.5f) {
            hammy->splice_active = false;
            hammy->splice_phase = 0.0f;
        }
        return output;
    }
    return new_sample;
}

static float shift_sample(gp100_hammy_t *hammy, float input)
{
    float filtered;

    filtered = filter_sample(&hammy->input_high_pass, input);
    filtered = filter_sample(&hammy->anti_alias_low_pass, filtered);
    hammy->delay[hammy->write_index] = filtered;
    update_period_detector(hammy);
    hammy->read_position += hammy->ratio;
    if (hammy->ratio == 1.0f &&
        (float)hammy->history_position - hammy->read_position > 100.0f) {
        hammy->read_position += 0.0001f;
    }
    schedule_splice(hammy);
    return render_splice(hammy);
}

bool gp100_hammy_reset(gp100_hammy_t *hammy,
                            uint8_t range,
                            bool harmony,
                            uint8_t volume,
                            uint8_t position)
{
    if (hammy == NULL || !parameters_valid(range, volume, position)) {
        return false;
    }
    memset(hammy, 0, sizeof(*hammy));
    set_targets(hammy, range, harmony, volume, position);
    hammy->history_position = (int)GP100_HAMMY_DELAY_SAMPLES -
                              HAMMY_HISTORY_COMPACTION_SAMPLES;
    hammy->read_position = (float)(hammy->history_position - HAMMY_FACTORY_LOOKAHEAD);
    hammy->raw_period = HAMMY_INITIAL_PERIOD;
    hammy->detected_period = HAMMY_INITIAL_PERIOD;
    configure_filter(&hammy->input_high_pass, 20.0f, true);
    configure_filter(&hammy->anti_alias_low_pass, HAMMY_LOW_PASS_LIMIT, false);
    hammy->anti_alias_ratio = 1.0f;
    hammy->volume = 1.0f;
    hammy->harmony_mix = 1.0f;
    hammy->position = 0.5f;
    update_controls(hammy, 0u);
    hammy->configured = true;
    return true;
}

bool gp100_hammy_process(gp100_hammy_t *hammy,
                              uint8_t range,
                              bool harmony,
                              uint8_t volume,
                              uint8_t position,
                              float *interleaved_stereo,
                              size_t frame_count,
                              bool enabled)
{
    size_t frame;

    if (hammy == NULL || !hammy->configured ||
        !parameters_valid(range, volume, position) ||
        (frame_count != 0u && interleaved_stereo == NULL) ||
        frame_count > SIZE_MAX / 2u) {
        return false;
    }
    set_targets(hammy, range, harmony, volume, position);
    update_controls(hammy, frame_count);
    hammy->peak_scan_pending = true;
    if (hammy->ratio != hammy->anti_alias_ratio) {
        float cutoff = (HAMMY_SAMPLE_RATE / 2.01f) / hammy->ratio;

        configure_filter(&hammy->anti_alias_low_pass,
                         fminf(cutoff, HAMMY_LOW_PASS_LIMIT), false);
        hammy->anti_alias_ratio = hammy->ratio;
    }
    for (frame = 0u; frame < frame_count; ++frame) {
        float input;
        float shifted;
        float output;

        if (!enabled) {
            continue;
        }
        input = interleaved_stereo[frame * 2u];
        shifted = shift_sample(hammy, input);
        update_transient_envelope(hammy, shifted);
        hammy->write_index =
            (hammy->write_index + 1u) % GP100_HAMMY_DELAY_SAMPLES;
        advance_history_position(hammy);
        if (hammy->harmony_mix < 0.5f) {
            output = input + shifted * hammy->volume * hammy->harmony_mix;
        } else {
            output = input * 2.0f * (1.0f - hammy->harmony_mix) +
                     shifted * hammy->volume * 0.5f;
        }
        interleaved_stereo[frame * 2u] = output;
        interleaved_stereo[frame * 2u + 1u] = output;
    }
    return true;
}
