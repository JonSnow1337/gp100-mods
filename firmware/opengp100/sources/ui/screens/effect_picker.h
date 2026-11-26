#ifndef GP100_EFFECT_PICKER_H
#define GP100_EFFECT_PICKER_H

#include "audio/effects/effect_catalog.h"

typedef enum gp100_effect_group {
    GP100_EFFECT_GROUP_DYNAMICS,
    GP100_EFFECT_GROUP_DRIVE,
    GP100_EFFECT_GROUP_WAH,
    GP100_EFFECT_GROUP_PITCH,
    GP100_EFFECT_GROUP_TONE,
    GP100_EFFECT_GROUP_MODULATION,
    GP100_EFFECT_GROUP_AMP,
    GP100_EFFECT_GROUP_CAB,
    GP100_EFFECT_GROUP_DELAY,
    GP100_EFFECT_GROUP_REVERB,
    GP100_EFFECT_GROUP_COUNT
} gp100_effect_group_t;

typedef struct gp100_picker_effect {
    gp100_effect_family_t family;
    uint16_t algorithm;
} gp100_picker_effect_t;

const char *gp100_effect_picker_group_name(gp100_effect_group_t group);
size_t gp100_effect_picker_count(gp100_effect_group_t group);
bool gp100_effect_picker_at(gp100_effect_group_t group, size_t index,
                            gp100_picker_effect_t *effect);
bool gp100_effect_picker_find(gp100_picker_effect_t effect,
                              gp100_effect_group_t *group, uint16_t *index);

#endif
