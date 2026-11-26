#ifndef GP100_ENVELOPE_H
#define GP100_ENVELOPE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct gp100_envelope_settings {
    float dry;
    float wet;
    float stop_hz;
    float resonance;
    float sensitivity;
} gp100_envelope_settings_t;

typedef struct gp100_envelope_channel {
    float band_history;
    float low_history;
} gp100_envelope_channel_t;

typedef struct gp100_envelope {
    gp100_envelope_settings_t settings;
    gp100_envelope_channel_t channel[2];
    float detector;
    bool enabled;
} gp100_envelope_t;

void gp100_envelope_reset(gp100_envelope_t *effect, const float controls[5]);
void gp100_envelope_process(gp100_envelope_t *effect, const float controls[5],
                            float *audio, size_t frames, bool enabled);

#endif
