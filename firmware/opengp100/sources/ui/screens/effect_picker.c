#include "effect_picker.h"

typedef struct effect_span {
    gp100_effect_family_t family;
    uint8_t first;
    uint8_t count; /* Zero selects the whole family. */
} effect_span_t;

typedef struct effect_group {
    const char *name;
    const effect_span_t *spans;
    size_t span_count;
} effect_group_t;

static const effect_span_t dynamics[] = {
    {GP100_EFFECT_PRE_FX, 0, 2},                    /* COMP, COMP4 */
    {GP100_EFFECT_PRE_FX, GP100_PRE_FX_STUDIO_COMP, 1},
    {GP100_EFFECT_PRE_FX, GP100_PRE_FX_LIMITTER, 1},
    {GP100_EFFECT_NOISE_REDUCTION, 0, 0}
};
static const effect_span_t drive[] = {
    {GP100_EFFECT_PRE_FX, 2, 2},                    /* Boost, 14 Boost */
    {GP100_EFFECT_PRE_FX, 13, 1},                   /* Saturate */
    {GP100_EFFECT_DISTORTION, 0, 17},
    {GP100_EFFECT_DISTORTION, GP100_DISTORTION_MUFF, 1}
};
static const effect_span_t wah[] = {
    {GP100_EFFECT_PRE_FX, 5, 4},
    {GP100_EFFECT_PRE_FX, 14, 1},                   /* Step Filter */
    {GP100_EFFECT_PRE_FX, GP100_PRE_FX_BALLS, 1},
    {GP100_EFFECT_PRE_FX, GP100_PRE_FX_ENVELOPE, 1}
};
static const effect_span_t pitch[] = {
    {GP100_EFFECT_PRE_FX, 9, 4},                    /* OCTA through Hammy */
    {GP100_EFFECT_PRE_FX, GP100_PRE_FX_OCTAVER2, 1},
    {GP100_EFFECT_MODULATION, 3, 1}                /* Detune */
};
static const effect_span_t tone[] = {
    {GP100_EFFECT_EQUALIZER, 0, 0},
    {GP100_EFFECT_PRE_FX, 4, 1}                     /* AC Sim */
};
static const effect_span_t modulation[] = {
    {GP100_EFFECT_MODULATION, 0, 3},                /* Choruses */
    {GP100_EFFECT_MODULATION, 4, 8},                /* Flanger through Bias Trem */
    {GP100_EFFECT_PRE_FX, 15, 1}                    /* Ring Mod */
};
static const effect_span_t amp[] = {{GP100_EFFECT_AMPLIFIER, 0, 0}};
static const effect_span_t cab[] = {{GP100_EFFECT_CABINET, 0, 0}};
static const effect_span_t delay[] = {{GP100_EFFECT_DELAY, 0, 0}};
static const effect_span_t reverb[] = {{GP100_EFFECT_REVERB, 0, 0}};

#define GROUP(name, spans) {name, spans, sizeof(spans) / sizeof((spans)[0])}
static const effect_group_t groups[GP100_EFFECT_GROUP_COUNT] = {
    [GP100_EFFECT_GROUP_DYNAMICS] = GROUP("Dynamics", dynamics),
    [GP100_EFFECT_GROUP_DRIVE] = GROUP("Drive", drive),
    [GP100_EFFECT_GROUP_WAH] = GROUP("Wah", wah),
    [GP100_EFFECT_GROUP_PITCH] = GROUP("Pitch", pitch),
    [GP100_EFFECT_GROUP_TONE] = GROUP("Tone", tone),
    [GP100_EFFECT_GROUP_MODULATION] = GROUP("Modulation", modulation),
    [GP100_EFFECT_GROUP_AMP] = GROUP("AMP", amp),
    [GP100_EFFECT_GROUP_CAB] = GROUP("Cab", cab),
    [GP100_EFFECT_GROUP_DELAY] = GROUP("Delay", delay),
    [GP100_EFFECT_GROUP_REVERB] = GROUP("Reverb", reverb)
};
#undef GROUP

static size_t span_count(const effect_span_t *span)
{
    return span->count != 0u ? span->count : gp100_effect_algorithm_count(span->family);
}

const char *gp100_effect_picker_group_name(gp100_effect_group_t group)
{
    return (unsigned)group < GP100_EFFECT_GROUP_COUNT ? groups[group].name : NULL;
}

size_t gp100_effect_picker_count(gp100_effect_group_t group)
{
    if ((unsigned)group >= GP100_EFFECT_GROUP_COUNT) {
        return 0u;
    }
    size_t count = 0u;
    for (size_t i = 0; i < groups[group].span_count; ++i) {
        const effect_span_t *span = &groups[group].spans[i];
        const size_t end = span->first + span_count(span);
        for (size_t model = span->first; model < end; ++model) {
            count += gp100_effect_available(span->family, model);
        }
    }
    return count;
}

bool gp100_effect_picker_at(gp100_effect_group_t group, size_t index,
                            gp100_picker_effect_t *effect)
{
    if ((unsigned)group >= GP100_EFFECT_GROUP_COUNT || effect == NULL) {
        return false;
    }
    for (size_t i = 0; i < groups[group].span_count; ++i) {
        const effect_span_t *span = &groups[group].spans[i];
        const size_t end = span->first + span_count(span);
        for (size_t model = span->first; model < end; ++model) {
            if (gp100_effect_available(span->family, model)) {
                if (index == 0u) {
                    *effect = (gp100_picker_effect_t){span->family, (uint16_t)model};
                    return true;
                }
                --index;
            }
        }
    }
    return false;
}

bool gp100_effect_picker_find(gp100_picker_effect_t effect,
                              gp100_effect_group_t *group, uint16_t *index)
{
    if (group == NULL || index == NULL) {
        return false;
    }
    /* The two distortion-family boosts share the Pre FX models' DSP and controls. */
    if (effect.family == GP100_EFFECT_DISTORTION &&
        (effect.algorithm == 17u || effect.algorithm == 18u)) {
        effect = (gp100_picker_effect_t){GP100_EFFECT_PRE_FX, (uint16_t)(effect.algorithm - 15u)};
    }
    for (size_t g = 0; g < GP100_EFFECT_GROUP_COUNT; ++g) {
        size_t offset = 0u;
        for (size_t i = 0; i < groups[g].span_count; ++i) {
            const effect_span_t *span = &groups[g].spans[i];
            const size_t end = span->first + span_count(span);
            for (size_t model = span->first; model < end; ++model) {
                if (gp100_effect_available(span->family, model)) {
                    if (effect.family == span->family && effect.algorithm == model) {
                        *group = (gp100_effect_group_t)g;
                        *index = (uint16_t)offset;
                        return true;
                    }
                    ++offset;
                }
            }
        }
    }
    return false;
}
