#include "effect_catalog.h"

/*
 * V2.1 has 639 logical parameters but only 29 constant range/default triples.
 * Each family table below stores a compact rule code in algorithm/parameter
 * order. See docs/effect_catalog.md for the extraction method and evidence.
 */
typedef struct parameter_range {
    float minimum;
    float maximum;
    float default_value;
} parameter_range_t;

typedef struct tempo_sync_rule {
    uint8_t family;
    uint8_t algorithm_index;
    uint8_t parameter_index;
    uint8_t selector_slot;
} tempo_sync_rule_t;

static const parameter_range_t parameter_ranges[] = {
    {0.0f, 99.0f, 20.0f},    /*  0 */
    {0.0f, 99.0f, 50.0f},    /*  1 */
    {0.0f, 99.0f, 60.0f},    /*  2 */
    {0.0f, 99.0f, 10.0f},    /*  3 */
    {0.0f, 1.0f, 0.0f},      /*  4 */
    {0.0f, 3.0f, 2.0f},      /*  5 */
    {0.1f, 10.0f, 0.5f},     /*  6 */
    {0.0f, 99.0f, 25.0f},    /*  7 */
    {0.0f, 99.0f, 70.0f},    /*  8 */
    {0.0f, 24.0f, 12.0f},    /*  9 */
    {-24.0f, 0.0f, -12.0f},  /* 10 */
    {0.0f, 12.0f, 12.0f},    /* 11 */
    {-12.0f, 0.0f, 0.0f},    /* 12 */
    {0.0f, 5.0f, 2.0f},      /* 13 */
    {0.0f, 99.0f, 99.0f},    /* 14 */
    {0.0f, 99.0f, 0.0f},     /* 15 */
    {0.0f, 99.0f, 30.0f},    /* 16 */
    {0.0f, 99.0f, 55.0f},    /* 17 */
    {0.0f, 99.0f, 85.0f},    /* 18 */
    {-50.0f, 50.0f, 0.0f},   /* 19 */
    {0.0f, 1.0f, 1.0f},      /* 20 */
    {0.0f, 99.0f, 40.0f},    /* 21 */
    {0.0f, 2.0f, 0.0f},      /* 22 */
    {0.0f, 4.0f, 1.0f},      /* 23 */
    {-50.0f, 50.0f, -25.0f}, /* 24 */
    {20.0f, 4000.0f, 500.0f},/* 25 */
    {20.0f, 300.0f, 150.0f}, /* 26 */
    {20.0f, 4000.0f, 400.0f},/* 27 */
    {0.0f, 100.0f, 50.0f}    /* 28 */
};

static const uint8_t pre_fx_rule_codes[] = {
    0, 1, /* algorithm 0 */
    0, 2, 1, 3, /* algorithm 1 */
    0, 4, /* algorithm 2 */
    1, /* algorithm 3 */
    1, 1, 1, 5, /* algorithm 4 */
    1, 1, 1, 1, 4, /* algorithm 5 */
    1, 6, 1, 7, 8, 2, 4, /* algorithm 6 */
    1, 1, 1, /* algorithm 7 */
    1, 1, 1, /* algorithm 8 */
    1, 1, 1, /* algorithm 9 */
    9, 10, 1, 1, 1, /* algorithm 10 */
    11, 12, 1, 1, 1, /* algorithm 11 */
    13, 4, 14, 15, /* algorithm 12 */
    1, 1, 1, 1, /* algorithm 13 */
    3, 16, 17, 18, 6, 4, /* algorithm 14 */
    1, 1, 19, 1 /* algorithm 15 */
};

static const uint8_t distortion_rule_codes[] = {
    1, 1, 1, /* algorithm 0 */
    16, 2, 1, 4, 20, /* algorithm 1 */
    1, 1, 1, 1, /* algorithm 2 */
    21, 16, 1, 2, /* algorithm 3 */
    1, 1, /* algorithm 4 */
    1, 1, 1, 1, 1, /* algorithm 5 */
    21, 8, 1, /* algorithm 6 */
    21, 1, /* algorithm 7 */
    1, 1, 1, /* algorithm 8 */
    21, 2, 1, /* algorithm 9 */
    21, 1, 1, /* algorithm 10 */
    1, 1, /* algorithm 11 */
    1, 1, 1, /* algorithm 12 */
    1, 1, 1, 22, 1, /* algorithm 13 */
    1, 1, 1, /* algorithm 14 */
    1, 1, 1, /* algorithm 15 */
    1, 1, 1, 1, 1, /* algorithm 16 */
    0, 4, /* algorithm 17 */
    1 /* algorithm 18 */
};

static const uint8_t amplifier_rule_codes[] = {
    0, 1, 1, /* algorithm 0 */
    0, 1, 1, 16, 1, 1, /* algorithm 1 */
    0, 1, 1, 21, 2, 20, /* algorithm 2 */
    21, 1, 1, 1, 1, 1, /* algorithm 3 */
    0, 2, 1, 4, /* algorithm 4 */
    0, 1, 1, 1, 1, 4, /* algorithm 5 */
    1, 1, 1, 1, 4, /* algorithm 6 */
    0, 1, 1, 1, 1, 1, /* algorithm 7 */
    0, 1, 1, 1, 1, 1, /* algorithm 8 */
    0, 1, 1, 1, 1, /* algorithm 9 */
    0, 1, 1, /* algorithm 10 */
    16, 1, 1, 1, 1, 1, /* algorithm 11 */
    16, 1, 1, 1, 1, 1, 1, /* algorithm 12 */
    16, 1, 1, 1, 1, 1, /* algorithm 13 */
    16, 1, 1, 1, 1, 1, /* algorithm 14 */
    16, 1, 1, 1, 1, 1, /* algorithm 15 */
    16, 1, 1, 1, 1, 1, /* algorithm 16 */
    16, 1, 1, 1, 1, 1, /* algorithm 17 */
    16, 1, 1, 1, 1, 4, /* algorithm 18 */
    1, 1, 1, 1, 1, /* algorithm 19 */
    16, 1, 1, 1, 1, 1, /* algorithm 20 */
    16, 1, 1, 1, 1, 1, /* algorithm 21 */
    16, 1, 1, 1, 1, 1, /* algorithm 22 */
    16, 1, 1, 1, 1, 1, /* algorithm 23 */
    16, 1, 1, 1, 1, 1, /* algorithm 24 */
    16, 1, 1, 1, 1, 1, /* algorithm 25 */
    16, 1, 1, 1, 1, /* algorithm 26 */
    1, 1, 1, 1, 1, 1, /* algorithm 27 */
    1, 1, 1, 1, 1, 1, /* algorithm 28 */
    1, 1, 1, 1, 1, 1, /* algorithm 29 */
    1, 1, 1, 1, 1, 1, /* algorithm 30 */
    1, 1, 1, 1, 1, 1, /* algorithm 31 */
    1, 1, 1, 1, 1, 1, /* algorithm 32 */
    1, 1, 1, 1, 1, 1, /* algorithm 33 */
    1, 1, 1, 1, 1, 1, /* algorithm 34 */
    1, 1, 1, 1, 1, /* algorithm 35 */
    1, 1, 1, 1, 1, 1, /* algorithm 36 */
    1, 1, 1, 1, 1, 1, /* algorithm 37 */
    1, 1, 1, 1, 1, 1, /* algorithm 38 */
    1, 1, 1, 1, 1, 1, /* algorithm 39 */
    1, 1, 1, 23, 1, 1, /* algorithm 40 */
    1, 4, 1, 1, 1, /* algorithm 41 */
    1, 1, 1, /* algorithm 42 */
    1, 1, 1, /* algorithm 43 */
    1, 1, 1, 1, 1, /* algorithm 44 */
    1, 1, 1, 1, 1, 1, /* algorithm 45 */
    1, 1, 1, 1, 1, /* algorithm 46 */
    1, 1, 1, 1, 1, /* algorithm 47 */
    1, 1, 1, 1, 1, /* algorithm 48 */
    1, 1, 1, 1, 1, /* algorithm 49 */
    1, 1, 1, 1, 1 /* algorithm 50 */
};

static const uint8_t noise_reduction_rule_codes[] = {
    0, /* algorithm 0 */
    0, 7, 2 /* algorithm 1 */
};

static const uint8_t cabinet_rule_codes[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const uint8_t equalizer_rule_codes[] = {
    19, 19, 19, 19, 19, 1, /* algorithm 0 */
    19, 19, 19, 19, 19, 1, /* algorithm 1 */
    19, 19, 19, 19, 19 /* algorithm 2 */
};

static const uint8_t modulation_rule_codes[] = {
    1, 6, 1, 4, /* algorithm 0 */
    1, 6, 1, 4, /* algorithm 1 */
    1, 6, 1, 4, /* algorithm 2 */
    24, 1, 1, /* algorithm 3 */
    1, 6, 1, 1, 4, /* algorithm 4 */
    1, 6, 4, /* algorithm 5 */
    6, 4, /* algorithm 6: Phaser; parameter 0 is state-dependent */
    1, 6, 4, /* algorithm 7 */
    1, 6, 4, /* algorithm 8 */
    1, 6, 1, 4, /* algorithm 9 */
    1, 6, 1, 4, /* algorithm 10 */
    1, 6, 1, 4, 1 /* algorithm 11 */
};

static const uint8_t delay_rule_codes[] = {
    0, 25, 16, 4, 4, /* algorithm 0 */
    0, 25, 1, 4, 4, /* algorithm 1 */
    0, 25, 1, 4, 4, /* algorithm 2 */
    0, 25, 1, 4, 4, /* algorithm 3 */
    0, 25, 1, 4, 4, /* algorithm 4 */
    0, 1, 25, 4, 4, /* algorithm 5 */
    0, 26, 1, 4, /* algorithm 6 */
    0, 1, 25, 1, 1, 4, 4, /* algorithm 7 */
    0, 1, 25, 1, 1, 4, 4, 4, /* algorithm 8 */
    0, 1, 25, 4, 4, /* algorithm 9 */
    0, 25, 16, 1, 4, 4, /* algorithm 10 */
    0, 1, 25, 0, 1, 27, 4, 4, 4 /* algorithm 11 */
};

static const uint8_t reverb_rule_codes[] = {
    1, 28, 1, 4, /* algorithm 0 */
    1, 28, 1, 4, /* algorithm 1 */
    1, 28, 1, 4, /* algorithm 2 */
    16, 28, 1, 4, /* algorithm 3 */
    16, 28, 1, 4, /* algorithm 4 */
    1, 1, 4, /* algorithm 5 */
    1, 1, 4, /* algorithm 6 */
    16, 28, 1, 19, 19, 4, /* algorithm 7 */
    1, 1, 4, /* algorithm 8 */
    1, 28, 1, 19, 19, 4 /* algorithm 9 */
};

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

_Static_assert(ARRAY_COUNT(parameter_ranges) == 29u,
               "parameter range dictionary must be complete");
_Static_assert(ARRAY_COUNT(pre_fx_rule_codes) == 62u,
               "Pre FX rule table must be complete");
_Static_assert(ARRAY_COUNT(distortion_rule_codes) == 61u,
               "distortion rule table must be complete");
_Static_assert(ARRAY_COUNT(amplifier_rule_codes) == 281u,
               "amplifier rule table must be complete");
_Static_assert(ARRAY_COUNT(noise_reduction_rule_codes) == 4u,
               "noise reduction rule table must be complete");
_Static_assert(ARRAY_COUNT(cabinet_rule_codes) == 60u,
               "cabinet rule table must be complete");
_Static_assert(ARRAY_COUNT(equalizer_rule_codes) == 17u,
               "equalizer rule table must be complete");
_Static_assert(ARRAY_COUNT(modulation_rule_codes) == 44u,
               "modulation rule table must be complete");
_Static_assert(ARRAY_COUNT(delay_rule_codes) == 69u,
               "delay rule table must be complete");
_Static_assert(ARRAY_COUNT(reverb_rule_codes) == 41u,
               "reverb rule table must be complete");
_Static_assert(62u + 61u + 281u + 4u + 60u + 17u + 44u + 69u + 41u ==
                   GP100_TOTAL_EFFECT_PARAMETERS,
               "all logical parameters must have a rule");

static const uint8_t *const family_rule_codes[] = {
    pre_fx_rule_codes,
    distortion_rule_codes,
    amplifier_rule_codes,
    noise_reduction_rule_codes,
    cabinet_rule_codes,
    equalizer_rule_codes,
    modulation_rule_codes,
    delay_rule_codes,
    reverb_rule_codes
};

/*
 * A selector value of 1.0 changes a continuous rate/time parameter to the
 * firmware's 0..10 beat-division scale (default division 4).
 */
static const tempo_sync_rule_t tempo_sync_rules[] = {
    {GP100_EFFECT_PRE_FX, 6u, 1u, 6u},
    {GP100_EFFECT_PRE_FX, 14u, 4u, 5u},

    {GP100_EFFECT_MODULATION, 0u, 1u, 3u},
    {GP100_EFFECT_MODULATION, 1u, 1u, 3u},
    {GP100_EFFECT_MODULATION, 2u, 1u, 3u},
    {GP100_EFFECT_MODULATION, 4u, 1u, 4u},
    {GP100_EFFECT_MODULATION, 5u, 1u, 2u},
    {GP100_EFFECT_MODULATION, 6u, 0u, 1u},
    {GP100_EFFECT_MODULATION, 7u, 1u, 2u},
    {GP100_EFFECT_MODULATION, 8u, 1u, 2u},
    {GP100_EFFECT_MODULATION, 9u, 1u, 3u},
    {GP100_EFFECT_MODULATION, 10u, 1u, 3u},
    {GP100_EFFECT_MODULATION, 11u, 1u, 3u},

    {GP100_EFFECT_DELAY, 0u, 1u, 3u},
    {GP100_EFFECT_DELAY, 1u, 1u, 3u},
    {GP100_EFFECT_DELAY, 2u, 1u, 3u},
    {GP100_EFFECT_DELAY, 3u, 1u, 3u},
    {GP100_EFFECT_DELAY, 4u, 1u, 3u},
    {GP100_EFFECT_DELAY, 5u, 2u, 3u},
    {GP100_EFFECT_DELAY, 7u, 2u, 5u},
    {GP100_EFFECT_DELAY, 8u, 2u, 6u},
    {GP100_EFFECT_DELAY, 8u, 4u, 5u},
    {GP100_EFFECT_DELAY, 9u, 2u, 3u},
    {GP100_EFFECT_DELAY, 10u, 1u, 5u},
    {GP100_EFFECT_DELAY, 11u, 2u, 6u},
    {GP100_EFFECT_DELAY, 11u, 5u, 7u}
};

float gp100_effect_tempo_beats(size_t selector)
{
    static const float beats[] = {
        4.0f, 2.0f, 3.0f, 0x1.555554p+0f, 1.0f, 1.5f,
        0x1.555556p-1f, 0.5f, 0.75f, 0x1.555554p-2f, 0.25f
    };

    return selector < ARRAY_COUNT(beats) ? beats[selector] : 1.0f;
}

float gp100_effect_tempo_beats_for_rate(float rate)
{
    if (rate >= 0.0f && rate <= 10.0f) {
        size_t selector = (size_t)rate;

        if (rate == (float)selector) {
            return gp100_effect_tempo_beats(selector);
        }
    }
    return 1.0f;
}

float gp100_effect_delay_samples(uint32_t time,
                                     bool sync,
                                     uint16_t tempo_period_ms)
{
    float samples = sync
        ? (float)tempo_period_ms * 44.1f * gp100_effect_tempo_beats(time)
        : (float)time * 44.1f;

    if (samples < 882.0f) {
        return 882.0f;
    }
    return samples > 176400.0f ? 176400.0f : samples;
}

static int tempo_sync_selector_slot(gp100_effect_family_t family,
                                    size_t algorithm_index,
                                    size_t parameter_index)
{
    size_t index;

    for (index = 0u; index < ARRAY_COUNT(tempo_sync_rules); ++index) {
        const tempo_sync_rule_t *candidate = &tempo_sync_rules[index];
        if (candidate->family == (uint8_t)family &&
            candidate->algorithm_index == algorithm_index &&
            candidate->parameter_index == parameter_index) {
            return candidate->selector_slot;
        }
    }
    return -1;
}

uint16_t gp100_effect_tempo_sync_selector_slots(
    gp100_effect_family_t family,
    size_t algorithm_index)
{
    uint16_t slots = 0u;
    size_t index;

    if (algorithm_index >= gp100_effect_algorithm_count(family)) {
        return 0u;
    }
    for (index = 0u; index < ARRAY_COUNT(tempo_sync_rules); ++index) {
        const tempo_sync_rule_t *sync = &tempo_sync_rules[index];
        if (sync->family == (uint8_t)family &&
            sync->algorithm_index == algorithm_index) {
            slots |= (uint16_t)(UINT16_C(1) << sync->selector_slot);
        }
    }
    return slots;
}

bool gp100_effect_parameter_rule(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    const float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    gp100_parameter_rule_t *rule)
{
    size_t algorithm_count = gp100_effect_algorithm_count(family);
    size_t parameter_count = gp100_effect_parameter_count(
        family, algorithm_index);
    size_t offset = parameter_index;
    uint8_t rule_code;
    uint8_t storage_slot;
    int sync_selector_slot;
    size_t index;

    if (rule == NULL || algorithm_index >= algorithm_count ||
        parameter_index >= parameter_count ||
        !gp100_effect_parameter_storage_slot(
            family, algorithm_index, parameter_index, &storage_slot)) {
        return false;
    }

    for (index = 0u; index < algorithm_index; ++index) {
        offset += gp100_effect_parameter_count(family, index);
    }
    rule_code = family_rule_codes[(size_t)family][offset];
    rule->storage_slot = storage_slot;
    rule->minimum = parameter_ranges[rule_code].minimum;
    rule->maximum = parameter_ranges[rule_code].maximum;
    rule->default_value = parameter_ranges[rule_code].default_value;

    sync_selector_slot = tempo_sync_selector_slot(family, algorithm_index,
                                                   parameter_index);
    if (sync_selector_slot >= 0 && stored_parameters != NULL &&
        stored_parameters[(size_t)sync_selector_slot] == 1.0f) {
        rule->minimum = 0.0f;
        rule->maximum = 10.0f;
        rule->default_value = 4.0f;
    }
    return true;
}

bool gp100_effect_apply_parameter_change(
    gp100_effect_family_t family,
    size_t algorithm_index,
    size_t parameter_index,
    float value,
    float stored_parameters[GP100_PARAMETERS_PER_FAMILY],
    uint16_t *changed_slots)
{
    gp100_parameter_rule_t changed_rule;
    uint16_t changes = 0u;
    size_t index;

    if (stored_parameters == NULL ||
        !gp100_effect_parameter_rule(family, algorithm_index,
                                          parameter_index,
                                          stored_parameters, &changed_rule) ||
        !(value >= changed_rule.minimum && value <= changed_rule.maximum)) {
        return false;
    }

    if (stored_parameters[changed_rule.storage_slot] != value) {
        stored_parameters[changed_rule.storage_slot] = value;
        changes |= (uint16_t)(UINT16_C(1) << changed_rule.storage_slot);
    }

    for (index = 0u; index < ARRAY_COUNT(tempo_sync_rules); ++index) {
        const tempo_sync_rule_t *sync = &tempo_sync_rules[index];
        gp100_parameter_rule_t dependent_rule;

        if (sync->family != (uint8_t)family ||
            sync->algorithm_index != algorithm_index ||
            sync->selector_slot != changed_rule.storage_slot ||
            !gp100_effect_parameter_rule(
                family, algorithm_index, sync->parameter_index,
                stored_parameters, &dependent_rule)) {
            continue;
        }
        if (stored_parameters[dependent_rule.storage_slot] !=
            dependent_rule.default_value) {
            stored_parameters[dependent_rule.storage_slot] =
                dependent_rule.default_value;
            changes |= (uint16_t)(UINT16_C(1) <<
                                  dependent_rule.storage_slot);
        }
    }

    if (changed_slots != NULL) {
        *changed_slots = changes;
    }
    return true;
}
