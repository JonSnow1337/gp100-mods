#include "effect_catalog.h"

#include <stdio.h>
#include <string.h>

static int copy_result(const char *text, char *output, size_t output_size)
{
    size_t length;

    if (text == NULL || (output == NULL && output_size != 0u)) {
        return -1;
    }
    length = strlen(text);
    if (output_size != 0u) {
        size_t copied = length < output_size - 1u ? length : output_size - 1u;
        memcpy(output, text, copied);
        output[copied] = '\0';
    }
    return (int)length;
}

static long round_half_away_from_zero(float value)
{
    return (long)(value < 0.0f ? value - 0.5f : value + 0.5f);
}

static const char *indexed_label(const char *const *labels,
                                 size_t label_count,
                                 float value)
{
    long index = (long)value;
    return index < 0 || (size_t)index >= label_count ? "Error" : labels[index];
}

static bool uses_tempo_divisions(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY])
{
    gp100_parameter_rule_t baseline;
    gp100_parameter_rule_t selected;

    return stored_parameters != NULL &&
           gp100_effect_parameter_rule(family, algorithm_index,
                                            parameter_index, NULL,
                                            &baseline) &&
           gp100_effect_parameter_rule(family, algorithm_index,
                                            parameter_index,
                                            stored_parameters, &selected) &&
           selected.minimum == 0.0f && selected.maximum == 10.0f &&
           selected.default_value == 4.0f &&
           (baseline.minimum != selected.minimum ||
            baseline.maximum != selected.maximum ||
            baseline.default_value != selected.default_value);
}

int gp100_effect_format_parameter_value(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    char *output,
    size_t output_size)
{
    static const char *const tempo_divisions[] = {
        "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D",
        "1/4T", "1/8", "1/8D", "1/8T", "1/16"
    };
    static const char *const acoustic_modes[] = {
        "STD", "Jumbo", "ENH", "Piezo"
    };
    static const char *const instrument_modes[] = {"Guitar", "Bass"};
    static const char *const pitch_ranges[] = {
        "-2 Oct", "-1 Oct", "+1 Oct", "+2 Oct", "+/-1 Oct", "+/-2 Oct"
    };
    static const char *const distortion_modes[] = {
        "Normal", "Scoop", "Edge"
    };
    static const char *const temperature_modes[] = {"Cool", "Hot"};
    static const char *const mid_ranges[] = {
        "220Hz", "450Hz", "800Hz", "1.6kHz", "3kHz"
    };
    gp100_parameter_rule_t rule;
    const char *name;
    const char *label = NULL;
    char numeric[32];
    long rounded;

    if ((output == NULL && output_size != 0u) ||
        !gp100_effect_parameter_rule(family, algorithm_index,
                                         parameter_index,
                                         stored_parameters, &rule)) {
        return -1;
    }

    if (uses_tempo_divisions(family, algorithm_index, parameter_index,
                             stored_parameters)) {
        label = indexed_label(tempo_divisions,
                              sizeof(tempo_divisions) /
                                  sizeof(tempo_divisions[0]),
                              value);
    } else if (family == GP100_EFFECT_PRE_FX && algorithm_index == 4u &&
               parameter_index == 3u) {
        label = indexed_label(acoustic_modes,
                              sizeof(acoustic_modes) /
                                  sizeof(acoustic_modes[0]),
                              value);
    } else if (family == GP100_EFFECT_PRE_FX && algorithm_index == 5u &&
               parameter_index == 4u) {
        label = indexed_label(instrument_modes,
                              sizeof(instrument_modes) /
                                  sizeof(instrument_modes[0]),
                              value);
    } else if (family == GP100_EFFECT_PRE_FX &&
               algorithm_index == 12u && parameter_index == 0u) {
        label = indexed_label(pitch_ranges,
                              sizeof(pitch_ranges) / sizeof(pitch_ranges[0]),
                              value);
    } else if (family == GP100_EFFECT_DISTORTION &&
               algorithm_index == 13u && parameter_index == 3u) {
        label = indexed_label(distortion_modes,
                              sizeof(distortion_modes) /
                                  sizeof(distortion_modes[0]),
                              value);
    } else if (family == GP100_EFFECT_AMPLIFIER &&
               algorithm_index == 18u && parameter_index == 5u) {
        label = indexed_label(temperature_modes,
                              sizeof(temperature_modes) /
                                  sizeof(temperature_modes[0]),
                              value);
    } else if (family == GP100_EFFECT_AMPLIFIER &&
               algorithm_index == 40u && parameter_index == 3u) {
        label = indexed_label(mid_ranges,
                              sizeof(mid_ranges) / sizeof(mid_ranges[0]),
                              value);
    }
    if (label != NULL) {
        return copy_result(label, output, output_size);
    }

    if (rule.minimum == 0.0f && rule.maximum == 1.0f) {
        static const char *const switches[] = {"Off", "On"};
        return copy_result(indexed_label(switches, 2u, value), output,
                           output_size);
    }

    name = gp100_effect_parameter_name(family, algorithm_index,
                                            parameter_index);
    if (name == NULL) {
        return -1;
    }
    if (strcmp(name, "Rate") == 0) {
        (void)snprintf(numeric, sizeof(numeric), "%.2fHz", (double)value);
    } else if ((family == GP100_EFFECT_DELAY &&
                (strcmp(name, "Time") == 0 ||
                 strcmp(name, "Time A") == 0 ||
                 strcmp(name, "Time B") == 0)) ||
               (family == GP100_EFFECT_REVERB &&
                strcmp(name, "Pre Delay") == 0)) {
        rounded = round_half_away_from_zero(value);
        (void)snprintf(numeric, sizeof(numeric), "%ldms", rounded);
    } else {
        rounded = round_half_away_from_zero(value);
        if (rounded == 0 && value < 0.0f) {
            (void)snprintf(numeric, sizeof(numeric), "-0");
        } else {
            (void)snprintf(numeric, sizeof(numeric), "%ld", rounded);
        }
    }
    return copy_result(numeric, output, output_size);
}
