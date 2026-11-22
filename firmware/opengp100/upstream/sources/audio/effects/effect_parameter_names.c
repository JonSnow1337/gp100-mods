#include "effect_catalog.h"

/* Exact V2.1 display strings, including abbreviations and product spelling. */
static const char *const parameter_names[] = {
    "Sustain", "Output", "Attack", "Clip", "Gain", "Bright", "Body",
    "Top", "VOL", "Mode", "Sense", "Range", "Q", "Mix", "Depth",
    "Rate", "Low", "High", "Sync", "Low Oct", "High Oct", "Dry",
    "H-Pitch", "L-Pitch", "H-Vol", "L-Vol", "Wet", "Harmony",
    "Position", "H-Cut", "Step 1", "Step 2", "Step 3", "Step 4",
    "Freq", "Fine", "Tone", "Fat", "Air", "Bass", "Treble", "Voice",
    "Middle", "Fuzz", "Filter", "Blend", "PRES", "Master", "Cut",
    "VOL 1", "VOL 2", "Char", "Tone 1", "Tone 2", "Edge", "MRange",
    "Balance", "EQ Freq", "EQ Q", "EQ Gain", "", "Thre", "Rel",
    "125Hz", "400Hz", "800Hz", "1.6kHz", "4kHz", "50Hz", "120Hz",
    "2kHz", "80Hz", "240Hz", "750Hz", "2.2kHz", "6.6kHz", "Level",
    "Detune", "Pre Delay", "FdBk", "Bias", "Time", "Trail", "Mod",
    " SwpDepth", "SwpRate", "SwpSync", "TimeSync", "Mix A", "FdBk A",
    "Time A", "Mix B", "FdBk B", "Time B", "Sync A", "Sync B",
    "Decay", "H-Damp", "Lo End", "Hi End"
};

static const uint8_t pre_fx_parameter_name_codes[] = {
    0, 1, /* algorithm 0 */
    0, 2, 1, 3, /* algorithm 1 */
    4, 5, /* algorithm 2 */
    4, /* algorithm 3 */
    6, 7, 8, 9, /* algorithm 4 */
    10, 11, 12, 13, 9, /* algorithm 5 */
    14, 15, 8, 16, 12, 17, 18, /* algorithm 6 */
    11, 12, 8, /* algorithm 7 */
    11, 12, 8, /* algorithm 8 */
    19, 20, 21, /* algorithm 9 */
    22, 23, 21, 24, 25, /* algorithm 10 */
    22, 23, 26, 21, 11, /* algorithm 11 */
    11, 27, 8, 28, /* algorithm 12 */
    4, 13, 1, 29, /* algorithm 13 */
    30, 31, 32, 33, 15, 18, /* algorithm 14 */
    13, 34, 35, 36 /* algorithm 15 */
};

static const uint8_t distortion_parameter_name_codes[] = {
    4, 36, 8, /* algorithm 0 */
    4, 36, 8, 37, 38, /* algorithm 1 */
    4, 8, 39, 40, /* algorithm 2 */
    4, 36, 8, 41, /* algorithm 3 */
    4, 8, /* algorithm 4 */
    4, 8, 39, 42, 40, /* algorithm 5 */
    4, 36, 8, /* algorithm 6 */
    4, 8, /* algorithm 7 */
    4, 36, 8, /* algorithm 8 */
    4, 36, 8, /* algorithm 9 */
    0, 36, 8, /* algorithm 10 */
    43, 8, /* algorithm 11 */
    4, 44, 8, /* algorithm 12 */
    4, 36, 8, 9, 45, /* algorithm 13 */
    4, 36, 8, /* algorithm 14 */
    4, 36, 8, /* algorithm 15 */
    4, 45, 8, 39, 40, /* algorithm 16 */
    4, 5, /* algorithm 17 */
    4 /* algorithm 18 */
};

static const uint8_t amplifier_parameter_name_codes[] = {
    8, 36, 1, /* algorithm 0 */
    8, 46, 1, 39, 42, 40, /* algorithm 1 */
    8, 1, 39, 42, 40, 5, /* algorithm 2 */
    4, 46, 47, 39, 42, 40, /* algorithm 3 */
    8, 48, 47, 5, /* algorithm 4 */
    4, 46, 47, 39, 40, 5, /* algorithm 5 */
    8, 39, 42, 40, 5, /* algorithm 6 */
    4, 46, 47, 39, 42, 40, /* algorithm 7 */
    4, 48, 47, 39, 42, 40, /* algorithm 8 */
    4, 8, 39, 42, 40, /* algorithm 9 */
    4, 46, 47, /* algorithm 10 */
    8, 46, 1, 39, 42, 40, /* algorithm 11 */
    49, 46, 1, 39, 42, 40, 50, /* algorithm 12 */
    4, 46, 47, 39, 42, 40, /* algorithm 13 */
    4, 46, 47, 39, 42, 40, /* algorithm 14 */
    4, 48, 47, 39, 42, 40, /* algorithm 15 */
    4, 46, 47, 39, 42, 40, /* algorithm 16 */
    8, 46, 1, 39, 42, 40, /* algorithm 17 */
    8, 48, 47, 39, 40, 51, /* algorithm 18 */
    49, 52, 50, 53, 1, /* algorithm 19 */
    4, 46, 47, 39, 42, 40, /* algorithm 20 */
    4, 46, 47, 39, 42, 40, /* algorithm 21 */
    4, 46, 47, 39, 42, 40, /* algorithm 22 */
    4, 46, 47, 39, 54, 40, /* algorithm 23 */
    4, 46, 47, 39, 42, 40, /* algorithm 24 */
    4, 46, 47, 39, 42, 40, /* algorithm 25 */
    4, 39, 42, 40, 8, /* algorithm 26 */
    4, 46, 47, 39, 42, 40, /* algorithm 27 */
    4, 46, 47, 39, 42, 40, /* algorithm 28 */
    4, 47, 39, 42, 40, 46, /* algorithm 29 */
    4, 46, 47, 39, 42, 40, /* algorithm 30 */
    4, 46, 47, 39, 42, 40, /* algorithm 31 */
    4, 46, 47, 39, 42, 40, /* algorithm 32 */
    4, 46, 47, 39, 42, 40, /* algorithm 33 */
    4, 46, 47, 39, 42, 40, /* algorithm 34 */
    4, 47, 39, 42, 40, /* algorithm 35 */
    4, 46, 47, 39, 42, 40, /* algorithm 36 */
    4, 46, 47, 39, 42, 40, /* algorithm 37 */
    4, 46, 47, 39, 42, 40, /* algorithm 38 */
    4, 46, 47, 39, 42, 40, /* algorithm 39 */
    4, 39, 42, 55, 40, 47, /* algorithm 40 */
    8, 5, 39, 42, 40, /* algorithm 41 */
    8, 39, 40, /* algorithm 42 */
    8, 39, 40, /* algorithm 43 */
    8, 47, 39, 42, 40, /* algorithm 44 */
    8, 36, 56, 57, 58, 59, /* algorithm 45 */
    60, 60, 60, 60, 60, /* algorithm 46 */
    60, 60, 60, 60, 60, /* algorithm 47 */
    60, 60, 60, 60, 60, /* algorithm 48 */
    60, 60, 60, 60, 60, /* algorithm 49 */
    60, 60, 60, 60, 60 /* algorithm 50 */
};

static const uint8_t noise_reduction_parameter_name_codes[] = {
    61, /* algorithm 0 */
    61, 2, 62 /* algorithm 1 */
};

static const uint8_t cabinet_parameter_name_codes[] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

static const uint8_t equalizer_parameter_name_codes[] = {
    63, 64, 65, 66, 67, 8, /* algorithm 0 */
    68, 69, 64, 65, 70, 8, /* algorithm 1 */
    71, 72, 73, 74, 75 /* algorithm 2 */
};

static const uint8_t modulation_parameter_name_codes[] = {
    14, 15, 36, 18, /* algorithm 0 */
    14, 15, 8, 18, /* algorithm 1 */
    14, 15, 76, 18, /* algorithm 2 */
    77, 26, 21, /* algorithm 3 */
    14, 15, 78, 79, 18, /* algorithm 4 */
    14, 15, 18, /* algorithm 5 */
    15, 18, /* algorithm 6 */
    14, 15, 18, /* algorithm 7 */
    14, 15, 18, /* algorithm 8 */
    14, 15, 8, 18, /* algorithm 9 */
    14, 15, 8, 18, /* algorithm 10 */
    14, 15, 8, 18, 80 /* algorithm 11 */
};

static const uint8_t delay_parameter_name_codes[] = {
    13, 81, 79, 18, 82, /* algorithm 0 */
    13, 81, 79, 18, 82, /* algorithm 1 */
    13, 81, 79, 18, 82, /* algorithm 2 */
    13, 81, 79, 18, 82, /* algorithm 3 */
    13, 81, 79, 18, 82, /* algorithm 4 */
    13, 79, 81, 18, 82, /* algorithm 5 */
    13, 81, 79, 82, /* algorithm 6 */
    13, 79, 81, 83, 36, 18, 82, /* algorithm 7 */
    13, 79, 81, 84, 85, 86, 87, 82, /* algorithm 8 */
    13, 79, 81, 18, 82, /* algorithm 9 */
    13, 81, 79, 36, 18, 82, /* algorithm 10 */
    88, 89, 90, 91, 92, 93, 94, 95, 82 /* algorithm 11 */
};

static const uint8_t reverb_parameter_name_codes[] = {
    13, 78, 96, 82, /* algorithm 0 */
    13, 78, 96, 82, /* algorithm 1 */
    13, 78, 96, 82, /* algorithm 2 */
    13, 96, 97, 82, /* algorithm 3 */
    13, 36, 96, 82, /* algorithm 4 */
    13, 96, 82, /* algorithm 5 */
    13, 96, 82, /* algorithm 6 */
    13, 78, 96, 98, 99, 82, /* algorithm 7 */
    13, 96, 82, /* algorithm 8 */
    13, 78, 96, 98, 99, 82 /* algorithm 9 */
};

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

_Static_assert(ARRAY_COUNT(parameter_names) == 100u,
               "parameter name dictionary must be complete");
_Static_assert(ARRAY_COUNT(pre_fx_parameter_name_codes) == 62u,
               "Pre FX parameter names must be complete");
_Static_assert(ARRAY_COUNT(distortion_parameter_name_codes) == 61u,
               "distortion parameter names must be complete");
_Static_assert(ARRAY_COUNT(amplifier_parameter_name_codes) == 281u,
               "amplifier parameter names must be complete");
_Static_assert(ARRAY_COUNT(noise_reduction_parameter_name_codes) == 4u,
               "noise-reduction parameter names must be complete");
_Static_assert(ARRAY_COUNT(cabinet_parameter_name_codes) == 60u,
               "cabinet parameter names must be complete");
_Static_assert(ARRAY_COUNT(equalizer_parameter_name_codes) == 17u,
               "equalizer parameter names must be complete");
_Static_assert(ARRAY_COUNT(modulation_parameter_name_codes) == 44u,
               "modulation parameter names must be complete");
_Static_assert(ARRAY_COUNT(delay_parameter_name_codes) == 69u,
               "delay parameter names must be complete");
_Static_assert(ARRAY_COUNT(reverb_parameter_name_codes) == 41u,
               "reverb parameter names must be complete");

static const uint8_t *const family_parameter_name_codes[] = {
    pre_fx_parameter_name_codes,
    distortion_parameter_name_codes,
    amplifier_parameter_name_codes,
    noise_reduction_parameter_name_codes,
    cabinet_parameter_name_codes,
    equalizer_parameter_name_codes,
    modulation_parameter_name_codes,
    delay_parameter_name_codes,
    reverb_parameter_name_codes
};

const char *gp100_effect_parameter_name(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index)
{
    size_t algorithm_count = gp100_effect_algorithm_count(family);
    size_t parameter_count = gp100_effect_parameter_count(
        family, algorithm_index);
    size_t offset = parameter_index;
    size_t index;

    if (algorithm_index >= algorithm_count || parameter_index >= parameter_count) {
        return NULL;
    }
    for (index = 0u; index < algorithm_index; ++index) {
        offset += gp100_effect_parameter_count(family, index);
    }
    return parameter_names[family_parameter_name_codes[(size_t)family][offset]];
}
