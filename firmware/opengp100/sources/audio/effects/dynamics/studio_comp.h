#ifndef GP100_STUDIO_COMP_H
#define GP100_STUDIO_COMP_H

#include <stdbool.h>
#include <stddef.h>

typedef enum gp100_studio_comp_control {
    GP100_STUDIO_COMP_WET,
    GP100_STUDIO_COMP_DRY,
    GP100_STUDIO_COMP_THRESHOLD,
    GP100_STUDIO_COMP_ATTACK,
    GP100_STUDIO_COMP_RELEASE,
    GP100_STUDIO_COMP_RATIO,
    GP100_STUDIO_COMP_KNEE,
    GP100_STUDIO_COMP_CONTROL_COUNT
} gp100_studio_comp_control_t;

/* The preset format stores finite values for the mute and infinity settings. */
#define GP100_STUDIO_COMP_MUTE_DB (-61.0f)
#define GP100_STUDIO_COMP_MAX_GAIN_DB 12.0f
#define GP100_STUDIO_COMP_MIN_THRESHOLD_DB (-60.0f)
#define GP100_STUDIO_COMP_MAX_THRESHOLD_DB 12.0f
#define GP100_STUDIO_COMP_MAX_ATTACK_MS 500.0f
#define GP100_STUDIO_COMP_MAX_RELEASE_MS 5000.0f
#define GP100_STUDIO_COMP_MAX_RATIO 40.0f
#define GP100_STUDIO_COMP_INFINITE_RATIO 41.0f
#define GP100_STUDIO_COMP_MAX_KNEE_DB 24.0f

typedef struct gp100_studio_comp {
    float controls[GP100_STUDIO_COMP_CONTROL_COUNT];
    float threshold_peak;
    float knee_log2;
    float ratio_slope;
    float attack_step;
    float release_step;
    float wet_target;
    float dry_target;
    float wet_gain;
    float dry_gain;
    float reduction_log2;
    bool active;
} gp100_studio_comp_t;

void gp100_studio_comp_reset(gp100_studio_comp_t *comp,
                             const float controls[GP100_STUDIO_COMP_CONTROL_COUNT]);
void gp100_studio_comp_process(gp100_studio_comp_t *comp,
                               const float controls[GP100_STUDIO_COMP_CONTROL_COUNT],
                               float *samples, size_t frames, bool enabled);

#endif
