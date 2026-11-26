#include "effect_catalog.h"

#include "audio/effects/dynamics/studio_comp.h"
#include "gp100_effect_profile.h"

typedef struct family_descriptor {
    const char *name;
    const uint32_t *algorithm_ids;
    const char *const *algorithm_names;
    const uint8_t *parameter_counts;
    size_t algorithm_count;
} family_descriptor_t;

#if GP100_PROFILE_FULL
static const uint32_t pre_fx_algorithms[] = {
    UINT32_C(0x00000000), UINT32_C(0x00000001),
    UINT32_C(0x0000001a), UINT32_C(0x0000000e),
    UINT32_C(0x01000001), UINT32_C(0x0100000f),
    UINT32_C(0x01000015), UINT32_C(0x05000001),
    UINT32_C(0x05000008), UINT32_C(0x01000021),
    UINT32_C(0x01000023), UINT32_C(0x01000024),
    UINT32_C(0x01000049), UINT32_C(0x01000033),
    UINT32_C(0x01000019), UINT32_C(0x0100002f),
    [GP100_PRE_FX_LIMITTER] = UINT32_C(0x4f470001),
    [GP100_PRE_FX_OCTAVER2] = UINT32_C(0x4f470002),
    [GP100_PRE_FX_BALLS] = UINT32_C(0x4f470004),
    [GP100_PRE_FX_ENVELOPE] = UINT32_C(0x4f470005),
    [GP100_PRE_FX_STUDIO_COMP] = UINT32_C(0x4f470006)
};

static const uint32_t distortion_algorithms[] = {
    UINT32_C(0x03000014), UINT32_C(0x03000008),
    UINT32_C(0x0300000b), UINT32_C(0x03000010),
    UINT32_C(0x03000029), UINT32_C(0x0300002d),
    UINT32_C(0x03000000), UINT32_C(0x03000002),
    UINT32_C(0x03000006), UINT32_C(0x03000009),
    UINT32_C(0x03000022), UINT32_C(0x03000024),
    UINT32_C(0x0300002b), UINT32_C(0x0300003f),
    UINT32_C(0x0300002a), UINT32_C(0x03000030),
    UINT32_C(0x03000040), UINT32_C(0x0000001a),
    UINT32_C(0x0000000e), [GP100_DISTORTION_MUFF] = UINT32_C(0x4f470003)
};

static const uint32_t amplifier_algorithms[] = {
    UINT32_C(0x07000001), UINT32_C(0x07000003),
    UINT32_C(0x07000004), UINT32_C(0x07000019),
    UINT32_C(0x07000011), UINT32_C(0x0700001a),
    UINT32_C(0x07000014), UINT32_C(0x07000015),
    UINT32_C(0x0700001b), UINT32_C(0x0700001f),
    UINT32_C(0x07000022), UINT32_C(0x0700002a),
    UINT32_C(0x0700002f), UINT32_C(0x07000035),
    UINT32_C(0x07000040), UINT32_C(0x07000049),
    UINT32_C(0x0700003d), UINT32_C(0x07000024),
    UINT32_C(0x07000027), UINT32_C(0x07000028),
    UINT32_C(0x07000048), UINT32_C(0x07000047),
    UINT32_C(0x0700004a), UINT32_C(0x0700004b),
    UINT32_C(0x07000039), UINT32_C(0x0700003a),
    UINT32_C(0x0700007c), UINT32_C(0x07000065),
    UINT32_C(0x0700005f), UINT32_C(0x0700005a),
    UINT32_C(0x07000059), UINT32_C(0x07000055),
    UINT32_C(0x07000068), UINT32_C(0x07000063),
    UINT32_C(0x0700005d), UINT32_C(0x07000053),
    UINT32_C(0x07000069), UINT32_C(0x0700006d),
    UINT32_C(0x0700006e), UINT32_C(0x0700004e),
    UINT32_C(0x07000073), UINT32_C(0x08000076),
    UINT32_C(0x08000075), UINT32_C(0x07000075),
    UINT32_C(0x07000077), UINT32_C(0x0800007a),
    UINT32_C(0x0f000000), UINT32_C(0x0f000001),
    UINT32_C(0x0f000002), UINT32_C(0x0f000003),
    UINT32_C(0x0f000004)
};

static const uint32_t noise_reduction_algorithms[] = {
    UINT32_C(0x0000001b), UINT32_C(0x0000001d)
};

static const uint32_t cabinet_algorithms[] = {
    UINT32_C(0x0a00001b), UINT32_C(0x0a000012),
    UINT32_C(0x0a000019), UINT32_C(0x0a00001c),
    UINT32_C(0x0a000011), UINT32_C(0x0a000013),
    UINT32_C(0x0a00001a), UINT32_C(0x0a000030),
    UINT32_C(0x0a000022), UINT32_C(0x0a00001f),
    UINT32_C(0x0a00002b), UINT32_C(0x0a000021),
    UINT32_C(0x0a000027), UINT32_C(0x0a00002e),
    UINT32_C(0x0a000026), UINT32_C(0x0a000020),
    UINT32_C(0x0a000028), UINT32_C(0x0a000031),
    UINT32_C(0x0a000024), UINT32_C(0x0a00002c),
    UINT32_C(0x0a000029), UINT32_C(0x0a00002a),
    UINT32_C(0x0a000006), UINT32_C(0x0a000004),
    UINT32_C(0x0a00000d), UINT32_C(0x0a000007),
    UINT32_C(0x0a000008), UINT32_C(0x0a00000c),
    UINT32_C(0x0a000000), UINT32_C(0x0a000001),
    UINT32_C(0x0a000002), UINT32_C(0x0a00001e),
    UINT32_C(0x0a000035), UINT32_C(0x0a000039),
    UINT32_C(0x0a000038), UINT32_C(0x0a00003b),
    UINT32_C(0x0a00003c), UINT32_C(0x0a00003e),
    UINT32_C(0x0a00003f), UINT32_C(0x0a000041),
    UINT32_C(0x0a100000), UINT32_C(0x0a100001),
    UINT32_C(0x0a100002), UINT32_C(0x0a100003),
    UINT32_C(0x0a100004), UINT32_C(0x0a100005),
    UINT32_C(0x0a100006), UINT32_C(0x0a100007),
    UINT32_C(0x0a100008), UINT32_C(0x0a100009),
    UINT32_C(0x0a10000a), UINT32_C(0x0a10000b),
    UINT32_C(0x0a10000c), UINT32_C(0x0a10000d),
    UINT32_C(0x0a10000e), UINT32_C(0x0a10000f),
    UINT32_C(0x0a100010), UINT32_C(0x0a100011),
    UINT32_C(0x0a100012), UINT32_C(0x0a100013)
};

static const uint32_t equalizer_algorithms[] = {
    UINT32_C(0x01000035), UINT32_C(0x0100003a),
    UINT32_C(0x0100003c)
};

static const uint32_t modulation_algorithms[] = {
    UINT32_C(0x04000000), UINT32_C(0x04000001),
    UINT32_C(0x04000008), UINT32_C(0x01000029),
    UINT32_C(0x04000011), UINT32_C(0x04000015),
    UINT32_C(0x04000019), UINT32_C(0x0400001f),
    UINT32_C(0x04000021), UINT32_C(0x04000026),
    UINT32_C(0x04000027), UINT32_C(0x04000028)
};

static const uint32_t delay_algorithms[] = {
    UINT32_C(0x0b00000d), UINT32_C(0x0b000000),
    UINT32_C(0x0b000002), UINT32_C(0x0b00000b),
    UINT32_C(0x0b000012), UINT32_C(0x0b000013),
    UINT32_C(0x0b000005), UINT32_C(0x0b000014),
    UINT32_C(0x0b000006), UINT32_C(0x0b000004),
    UINT32_C(0x0b00000c), UINT32_C(0x0b000003)
};

static const uint32_t reverb_algorithms[] = {
    UINT32_C(0x0c000000), UINT32_C(0x0c000001),
    UINT32_C(0x0c000002), UINT32_C(0x0c000003),
    UINT32_C(0x0c000004), UINT32_C(0x0c000006),
    UINT32_C(0x0c000007), UINT32_C(0x0c000008),
    UINT32_C(0x0c000005), UINT32_C(0x0c000009)
};

static const char *const pre_fx_names[] = {
    "COMP", "COMP4", "Boost", "14 Boost", "AC Sim", "T-Wah", "A-WAH",
    "V-Wah", "C-Wah", "OCTA", "Pitch", "P-Bend", "Hammy", "Saturate",
    "Step Filter", "Ring Mod", [GP100_PRE_FX_LIMITTER] = "Limitter",
    [GP100_PRE_FX_OCTAVER2] = "Octaver2", [GP100_PRE_FX_BALLS] = "Balls",
    [GP100_PRE_FX_ENVELOPE] = "Envelope",
    [GP100_PRE_FX_STUDIO_COMP] = "Studio Comp"
};

static const char *const distortion_names[] = {
    "Penesas", "Scream OD", "Tube Clipper", "TaiChi OD", "Plustortion",
    "Chief", "Green OD", "Yellow OD", "Super OD", "Blues OD", "Lazaro",
    "Red Haze", "Darktale", "Flex OD", "SM Dist", "La Charger",
    "Bass Dist", "Boost", "14 Boost", [GP100_DISTORTION_MUFF] = "Muff"
};

static const char *const amplifier_names[] = {
    "Tweedy", "Bellman 59N", "Dark Twin", "L-Star CL", "Foxy 30N",
    "BogSV CL", "J-120 CL", "Match CL", "Z38 CL", "Knights CL",
    "Bad-KT CL", "UK 45", "UK 50JP", "UK 800", "Flagman", "Z38 OD",
    "BogSV OD", "Bellman 59B", "Foxy 30TB", "SUPDual OD", "Match OD",
    "Solo100 OD", "L-Star OD", "Bad-KT OD", "Mess2C+ 1", "Mess2C+ 2",
    "Knights OD", "Dizz VH", "Eagle 120", "EV 51", "Solo100 LD",
    "Mess4 LD", "Mess DualV", "Power LD", "Flagman+", "Juice R100",
    "Mess DualM", "Bog BlueV", "Bog RedM", "UK 900", "Classic Bass",
    "Bass Pre", "Mini Bass", "Foxy Bass", "Mess Bass", "AC Pre",
    NULL, NULL, NULL, NULL, NULL
};

static const char *const noise_reduction_names[] = {"Gate 1", "Gate 2"};

static const char *const cabinet_names[] = {
    "TWD 2x12", "DarkTW 2x12", "L-Star 2x12", "2Rick 2x12", "J-120 2x12",
    "UK-GN 2x12", "Free 2x12", "UK-75 4x12", "UK-GN 4x12", "UK-LD 4x12",
    "UK-DK 4x12", "UK-MD 4x12", "Pogner 4x12", "Dizz 4x12", "Eagle 4x12",
    "EV51 4x12", "Solo 4x12", "US 4x12", "Mess-D 4x12", "U-ban 4x12",
    "Juice 4x12", "H-Way 4x12", "BogSV 1x12", "Dark 1x12", "Regular 1x12",
    "Bad-KT 1x12", "Foxy 1x12", "Studio 1x12", "SUP 1x6", "TWD 1x8",
    "TWD-P 1x10", "Bellman 4x10", "MessBass 2x10", "Max 4x10", "Ameg 4x10",
    "Ameg 8x10", "D", "OM", "Jumbo", "GA",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const char *const equalizer_names[] = {"EQ 1", "EQ 2", "Mess EQ"};

static const char *const modulation_names[] = {
    "A-Chorus", "G-Chorus", "B-Chorus", "Detune", "Flanger", "Vibrato",
    "Phaser", "Vibe", "Opto Trem", "Sine Trem", "Triangle Trem", "Bias Trem"
};

static const char *const delay_names[] = {
    "Sweet", "P-Echo", "M-Echo", "T-Echo", "999 Echo", "Rev Echo",
    "Slapbk", "Vin-Rack", "Swp Echo", "Ping Pong", "M-Echo2", "Dual Echo"
};

static const char *const reverb_names[] = {
    "Room", "Hall", "Church", "Plate", "Spring", "N-Star", "Deep Sea",
    "Mod Verb", "Clear Sky", "Shimmer"
};
#else
#include "gp100_effect_catalog_profile.inc"
#endif

static const uint8_t pre_fx_parameter_counts[] = {
    2u, 4u, 2u, 1u, 4u, 5u, 7u, 3u,
    3u, 3u, 5u, 5u, 4u, 4u, 6u, 4u, [GP100_PRE_FX_LIMITTER] = 3u,
    [GP100_PRE_FX_OCTAVER2] = 4u, [GP100_PRE_FX_BALLS] = 1u,
    [GP100_PRE_FX_ENVELOPE] = 5u,
    [GP100_PRE_FX_STUDIO_COMP] = GP100_STUDIO_COMP_CONTROL_COUNT
};

static const uint8_t distortion_parameter_counts[] = {
    3u, 5u, 4u, 4u, 2u, 5u, 3u, 2u, 3u, 3u,
    3u, 2u, 3u, 5u, 3u, 3u, 5u, 2u, 1u, [GP100_DISTORTION_MUFF] = 3u
};

static const uint8_t amplifier_parameter_counts[] = {
    3u, 6u, 6u, 6u, 4u, 6u, 5u, 6u, 6u, 5u,
    3u, 6u, 7u, 6u, 6u, 6u, 6u, 6u, 6u, 5u,
    6u, 6u, 6u, 6u, 6u, 6u, 5u, 6u, 6u, 6u,
    6u, 6u, 6u, 6u, 6u, 5u, 6u, 6u, 6u, 6u,
    6u, 5u, 3u, 3u, 5u, 6u, 5u, 5u, 5u, 5u,
    5u
};

static const uint8_t noise_reduction_parameter_counts[] = {1u, 3u};

static const uint8_t cabinet_parameter_counts[] = {
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u,
    1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u
};

static const uint8_t equalizer_parameter_counts[] = {6u, 6u, 5u};

static const uint8_t modulation_parameter_counts[] = {
    4u, 4u, 4u, 3u, 5u, 3u, 2u, 3u, 3u, 4u, 4u, 5u
};

static const uint8_t delay_parameter_counts[] = {
    5u, 5u, 5u, 5u, 5u, 5u, 4u, 7u, 8u, 5u, 6u, 9u
};

static const uint8_t reverb_parameter_counts[] = {
    4u, 4u, 4u, 4u, 4u, 3u, 3u, 6u, 3u, 6u
};

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

_Static_assert(ARRAY_COUNT(pre_fx_algorithms) ==
               ARRAY_COUNT(pre_fx_parameter_counts),
               "Pre FX catalog arrays must match");
_Static_assert(ARRAY_COUNT(pre_fx_algorithms) == ARRAY_COUNT(pre_fx_names),
               "Pre FX name array must match");
_Static_assert(ARRAY_COUNT(distortion_algorithms) ==
               ARRAY_COUNT(distortion_parameter_counts),
               "distortion catalog arrays must match");
_Static_assert(ARRAY_COUNT(distortion_algorithms) ==
               ARRAY_COUNT(distortion_names),
               "distortion name array must match");
_Static_assert(ARRAY_COUNT(amplifier_algorithms) ==
               ARRAY_COUNT(amplifier_parameter_counts),
               "amplifier catalog arrays must match");
_Static_assert(ARRAY_COUNT(amplifier_algorithms) ==
               ARRAY_COUNT(amplifier_names),
               "amplifier name array must match");
_Static_assert(ARRAY_COUNT(noise_reduction_algorithms) ==
               ARRAY_COUNT(noise_reduction_parameter_counts),
               "noise-reduction catalog arrays must match");
_Static_assert(ARRAY_COUNT(noise_reduction_algorithms) ==
               ARRAY_COUNT(noise_reduction_names),
               "noise-reduction name array must match");
_Static_assert(ARRAY_COUNT(cabinet_algorithms) ==
               ARRAY_COUNT(cabinet_parameter_counts),
               "cabinet catalog arrays must match");
_Static_assert(ARRAY_COUNT(cabinet_algorithms) == ARRAY_COUNT(cabinet_names),
               "cabinet name array must match");
_Static_assert(ARRAY_COUNT(equalizer_algorithms) ==
               ARRAY_COUNT(equalizer_parameter_counts),
               "equalizer catalog arrays must match");
_Static_assert(ARRAY_COUNT(equalizer_algorithms) ==
               ARRAY_COUNT(equalizer_names),
               "equalizer name array must match");
_Static_assert(ARRAY_COUNT(modulation_algorithms) ==
               ARRAY_COUNT(modulation_parameter_counts),
               "modulation catalog arrays must match");
_Static_assert(ARRAY_COUNT(modulation_algorithms) ==
               ARRAY_COUNT(modulation_names),
               "modulation name array must match");
_Static_assert(ARRAY_COUNT(delay_algorithms) ==
               ARRAY_COUNT(delay_parameter_counts),
               "delay catalog arrays must match");
_Static_assert(ARRAY_COUNT(delay_algorithms) == ARRAY_COUNT(delay_names),
               "delay name array must match");
_Static_assert(ARRAY_COUNT(reverb_algorithms) ==
               ARRAY_COUNT(reverb_parameter_counts),
               "reverb catalog arrays must match");
_Static_assert(ARRAY_COUNT(reverb_algorithms) == ARRAY_COUNT(reverb_names),
               "reverb name array must match");

static const family_descriptor_t families[] = {
    {"Pre FX", pre_fx_algorithms, pre_fx_names, pre_fx_parameter_counts,
     ARRAY_COUNT(pre_fx_algorithms)},
    {"Distortion", distortion_algorithms, distortion_names,
     distortion_parameter_counts,
     ARRAY_COUNT(distortion_algorithms)},
    {"Amplifier", amplifier_algorithms, amplifier_names,
     amplifier_parameter_counts,
     ARRAY_COUNT(amplifier_algorithms)},
    {"Noise reduction", noise_reduction_algorithms,
     noise_reduction_names, noise_reduction_parameter_counts,
     ARRAY_COUNT(noise_reduction_algorithms)},
    {"Cabinet", cabinet_algorithms, cabinet_names, cabinet_parameter_counts,
     ARRAY_COUNT(cabinet_algorithms)},
    {"Equalizer", equalizer_algorithms, equalizer_names,
     equalizer_parameter_counts,
     ARRAY_COUNT(equalizer_algorithms)},
    {"Modulation", modulation_algorithms, modulation_names,
     modulation_parameter_counts,
     ARRAY_COUNT(modulation_algorithms)},
    {"Delay", delay_algorithms, delay_names, delay_parameter_counts,
     ARRAY_COUNT(delay_algorithms)},
    {"Reverb", reverb_algorithms, reverb_names, reverb_parameter_counts,
     ARRAY_COUNT(reverb_algorithms)}
};

static const family_descriptor_t *family_descriptor(
    gp100_effect_family_t family)
{
    if ((unsigned)family >= ARRAY_COUNT(families)) {
        return NULL;
    }
    return &families[(unsigned)family];
}

const char *gp100_effect_family_name(
    gp100_effect_family_t family)
{
    const family_descriptor_t *descriptor = family_descriptor(family);
    return descriptor == NULL ? NULL : descriptor->name;
}

size_t gp100_effect_algorithm_count(
    gp100_effect_family_t family)
{
    const family_descriptor_t *descriptor = family_descriptor(family);
    return descriptor == NULL ? 0u : descriptor->algorithm_count;
}

bool gp100_effect_available(gp100_effect_family_t family, size_t algorithm_index)
{
    static const uint64_t masks[GP100_EFFECT_FAMILY_COUNT] = {
        [GP100_EFFECT_PRE_FX] = GP100_PROFILE_PRE_FX,
        [GP100_EFFECT_DISTORTION] = GP100_PROFILE_DISTORTION,
        [GP100_EFFECT_AMPLIFIER] = GP100_PROFILE_AMPLIFIER,
        [GP100_EFFECT_NOISE_REDUCTION] = GP100_PROFILE_NOISE_REDUCTION,
        [GP100_EFFECT_CABINET] = GP100_PROFILE_CABINET,
        [GP100_EFFECT_EQUALIZER] = GP100_PROFILE_EQUALIZER,
        [GP100_EFFECT_MODULATION] = GP100_PROFILE_MODULATION,
        [GP100_EFFECT_DELAY] = GP100_PROFILE_DELAY,
        [GP100_EFFECT_REVERB] = GP100_PROFILE_REVERB
    };
    return (unsigned)family < GP100_EFFECT_FAMILY_COUNT && algorithm_index < 64u &&
           (masks[family] & (UINT64_C(1) << algorithm_index)) != 0u;
}

bool gp100_effect_algorithm_id(
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint32_t *algorithm_id)
{
    const family_descriptor_t *descriptor = family_descriptor(family);

    if (descriptor == NULL || algorithm_id == NULL ||
        !gp100_effect_available(family, algorithm_index)) {
        return false;
    }
    *algorithm_id = descriptor->algorithm_ids[algorithm_index];
    return true;
}

const char *gp100_effect_algorithm_name(
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    const family_descriptor_t *descriptor = family_descriptor(family);

    if (descriptor == NULL || !gp100_effect_available(family, algorithm_index)) {
        return NULL;
    }
    return descriptor->algorithm_names[algorithm_index];
}

bool gp100_effect_algorithm_is_user_slot(
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    const family_descriptor_t *descriptor = family_descriptor(family);

    return descriptor != NULL && gp100_effect_available(family, algorithm_index) &&
           descriptor->algorithm_names[algorithm_index] == NULL;
}

int gp100_effect_algorithm_index(
    gp100_effect_family_t family,
    uint32_t algorithm_id)
{
    const family_descriptor_t *descriptor = family_descriptor(family);
    size_t index;

    if (descriptor == NULL) {
        return -1;
    }
    for (index = 0u; index < descriptor->algorithm_count; ++index) {
        if (gp100_effect_available(family, index) && descriptor->algorithm_ids[index] == algorithm_id) {
            return (int)index;
        }
    }
    return -1;
}

uint32_t gp100_effect_default_algorithm_id(
    gp100_effect_family_t family)
{
    const family_descriptor_t *descriptor = family_descriptor(family);
    if (descriptor != NULL) {
        for (size_t i = 0u; i < descriptor->algorithm_count; ++i) {
            if (gp100_effect_available(family, i)) return descriptor->algorithm_ids[i];
        }
    }
    return 0u;
}

size_t gp100_effect_parameter_count(
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    const family_descriptor_t *descriptor = family_descriptor(family);

    if (descriptor == NULL || !gp100_effect_available(family, algorithm_index)) {
        return 0u;
    }
    return descriptor->parameter_counts[algorithm_index];
}

size_t gp100_effect_parameter_offset(gp100_effect_family_t family, size_t algorithm_index)
{
    const family_descriptor_t *descriptor = family_descriptor(family);
    size_t offset = 0u;
    for (size_t i = 0u; i < algorithm_index; ++i) {
        offset += descriptor->parameter_counts[i];
    }
    return offset;
}

size_t gp100_effect_parameter_count_for_id(
    gp100_effect_family_t family,
    uint32_t algorithm_id)
{
    int algorithm_index = gp100_effect_algorithm_index(family,
                                                            algorithm_id);
    return algorithm_index < 0 ? 0u :
           gp100_effect_parameter_count(family, (size_t)algorithm_index);
}

bool gp100_effect_parameter_storage_slot(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    uint8_t *storage_slot)
{
    size_t parameter_count = gp100_effect_parameter_count(
        family, algorithm_index);
    size_t slot = parameter_index;

    if (storage_slot == NULL || parameter_index >= parameter_count) {
        return false;
    }

    if (family == GP100_EFFECT_CABINET) {
        slot = 1u;
    } else if ((family == GP100_EFFECT_PRE_FX &&
                algorithm_index == 2u && parameter_index == 1u) ||
               (family == GP100_EFFECT_DISTORTION &&
                algorithm_index == 17u && parameter_index == 1u)) {
        slot = 2u;
    } else if (family == GP100_EFFECT_AMPLIFIER &&
               algorithm_index == 22u) {
        slot = parameter_index + 1u;
    } else if (family == GP100_EFFECT_AMPLIFIER &&
               algorithm_index == 29u && parameter_index == 5u) {
        slot = 6u;
    } else if (family == GP100_EFFECT_DELAY &&
               algorithm_index == 6u && parameter_index == 3u) {
        slot = 4u;
    } else if (family == GP100_EFFECT_DELAY &&
               algorithm_index == 10u && parameter_index >= 4u) {
        slot = parameter_index + 1u;
    }

    *storage_slot = (uint8_t)slot;
    return true;
}

int gp100_effect_parameter_index_for_storage_slot(
    gp100_effect_family_t family,
    size_t algorithm_index,
    uint8_t storage_slot)
{
    size_t parameter_count = gp100_effect_parameter_count(
        family, algorithm_index);
    size_t parameter_index;

    if (storage_slot >= GP100_PARAMETERS_PER_FAMILY) {
        return -1;
    }
    for (parameter_index = 0u; parameter_index < parameter_count;
         ++parameter_index) {
        uint8_t candidate;
        if (gp100_effect_parameter_storage_slot(
                family, algorithm_index, parameter_index, &candidate) &&
            candidate == storage_slot) {
            return (int)parameter_index;
        }
    }
    return -1;
}

static bool catalog_algorithm_is_valid(void *context,
                                       gp100_effect_family_t family,
                                       uint32_t algorithm_id)
{
    (void)context;
    return gp100_effect_algorithm_index(family, algorithm_id) >= 0;
}

static uint32_t catalog_default_algorithm(void *context,
                                          gp100_effect_family_t family)
{
    (void)context;
    return gp100_effect_default_algorithm_id(family);
}

static size_t catalog_parameter_count(void *context,
                                      gp100_effect_family_t family,
                                      uint32_t algorithm_id)
{
    (void)context;
    return gp100_effect_parameter_count_for_id(family, algorithm_id);
}

static bool catalog_parameter_rule(
    void *context,
    gp100_effect_family_t family,
    uint32_t algorithm_id,
    size_t parameter_index,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    gp100_parameter_rule_t *rule)
{
    int algorithm_index;

    (void)context;
    algorithm_index = gp100_effect_algorithm_index(family, algorithm_id);
    return algorithm_index >= 0 &&
           gp100_effect_parameter_rule(family, (size_t)algorithm_index,
                                            parameter_index,
                                            stored_parameters, rule);
}

gp100_effect_catalog_t gp100_effect_catalog_core(void)
{
    gp100_effect_catalog_t catalog;

    catalog.context = NULL;
    catalog.algorithm_is_valid = catalog_algorithm_is_valid;
    catalog.default_algorithm = catalog_default_algorithm;
    catalog.parameter_count = catalog_parameter_count;
    catalog.parameter_rule = catalog_parameter_rule;
    return catalog;
}
