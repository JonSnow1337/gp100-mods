#include "audio/effects/wah/step_filter.h"

#include <math.h>
#include <stdio.h>

static float audio[24000];

int main(void)
{
    gp100_step_filter_t filter;
    size_t frame;

    if (!gp100_step_filter_reset(&filter, 0u, 33u, 66u, 99u,
                                      10.0f, false) ||
        !gp100_step_filter_process(&filter, 0u, 33u, 66u, 99u,
                                        10.0f, false, 500u,
                                        audio, 12000u, true) ||
        filter.current_cycle_increment <= 0.0f ||
        filter.selected_step < 0.3f || !isfinite(filter.cutoff_hz)) {
        fprintf(stderr, "Step Filter sequence test failed\n");
        return 1;
    }
    for (frame = 0u; frame < 12000u; ++frame) {
        if (!isfinite(audio[frame * 2u]) ||
            audio[frame * 2u] != audio[frame * 2u + 1u]) {
            fprintf(stderr, "Step Filter output test failed\n");
            return 2;
        }
    }
    audio[0] = 0.25f;
    audio[1] = -0.5f;
    frame = (size_t)filter.phase_counter;
    if (!gp100_step_filter_process(&filter, 99u, 0u, 0u, 0u,
                                        4.0f, true, 500u,
                                        audio, 1u, false) ||
        audio[0] != 0.25f || audio[1] != -0.5f ||
        (size_t)filter.phase_counter != frame ||
        gp100_step_filter_reset(NULL, 0u, 0u, 0u, 0u, 1.0f, false) ||
        gp100_step_filter_reset(&filter, 100u, 0u, 0u, 0u, 1.0f, false) ||
        gp100_step_filter_process(&filter, 0u, 0u, 0u, 0u,
                                       11.0f, true, 500u,
                                       audio, 1u, true) ||
        gp100_step_filter_process(&filter, 0u, 0u, 0u, 0u,
                                       1.0f, false, 500u,
                                       NULL, 1u, true)) {
        fprintf(stderr, "Step Filter bypass/validation test failed\n");
        return 3;
    }
    return 0;
}
