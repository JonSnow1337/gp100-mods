#include "audio/effects/amp/snap_tone.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    static gp100_snap_tone_t state;
    uint8_t blob[GP100_SYSEX_USER_AMP_BLOB_BYTES];
    float samples[256];
    float first_output[256];
    float stereo[128];
    FILE *file;
    size_t slot;
    size_t index;
    const float parameters[5] = {50.0f, 50.0f, 0.0f, 50.0f, 100.0f};
    const float flat_tone[3] = {0.0f, 0.0f, 0.0f};
    gp100_snap_tone_targets_t targets;
    gp100_biquad_coefficients_t tone_coefficients[3];
    const float lattice_input[2] = {1.0f, 2.0f};
    const float lattice_coefficient[1] = {0.5f};
    float lattice_state[1] = {0.0f};
    float lattice_pairs[4];
    float lattice_output[2];
    float lattice_delay = 0.0f;

    if (!gp100_snap_tone_lattice_expand(
            lattice_input, 2u, lattice_coefficient, lattice_state, 1u,
            lattice_pairs) ||
        fabsf(lattice_pairs[0] - 0.5f) > 1.0e-6f ||
        fabsf(lattice_pairs[1] - 1.0f) > 1.0e-6f ||
        fabsf(lattice_pairs[2] - 1.75f) > 1.0e-6f ||
        fabsf(lattice_pairs[3] - 2.0f) > 1.0e-6f) {
        fputs("SnapTone lattice expansion failed\n", stderr);
        return 1;
    }
    lattice_state[0] = 0.0f;
    if (!gp100_snap_tone_lattice_collapse(
            lattice_pairs, 2u, lattice_coefficient, lattice_state, 1u,
            &lattice_delay, lattice_output) ||
        fabsf(lattice_output[0] - 0.125f) > 1.0e-6f ||
        fabsf(lattice_output[1] - 1.125f) > 1.0e-6f ||
        fabsf(lattice_delay - 2.0f) > 1.0e-6f ||
        gp100_snap_tone_lattice_expand(
            NULL, 1u, lattice_coefficient, lattice_state, 1u,
            lattice_pairs) ||
        gp100_snap_tone_lattice_collapse(
            lattice_pairs, 1u, lattice_coefficient, lattice_state, 1u,
            NULL, lattice_output)) {
        fputs("SnapTone lattice collapse failed\n", stderr);
        return 1;
    }

    if (!gp100_snap_tone_control_targets(parameters, &targets) ||
        fabsf(targets.input_gain - 1.0f) > 1.0e-6f ||
        fabsf(targets.output_gain - 1.0f) > 1.0e-6f ||
        targets.tone_db[0] != -6.0f || targets.tone_db[1] != 0.0f ||
        targets.tone_db[2] != 6.0f ||
        gp100_snap_tone_control_targets(NULL, &targets)) {
        fputs("SnapTone control mapping failed\n", stderr);
        return 1;
    }
    if (!gp100_snap_tone_tone_coefficients(
            flat_tone, tone_coefficients)) {
        fputs("SnapTone tone mapping failed\n", stderr);
        return 1;
    }
    for (index = 0u; index < 3u; ++index) {
        if (fabsf(tone_coefficients[index].b0 - 1.0f) > 1.0e-6f ||
            fabsf(tone_coefficients[index].b1 -
                  tone_coefficients[index].a1) > 1.0e-6f ||
            fabsf(tone_coefficients[index].b2 -
                  tone_coefficients[index].a2) > 1.0e-6f) {
            fputs("SnapTone flat tone is not unity\n", stderr);
            return 1;
        }
    }

    if (argc != 2 || (file = fopen(argv[1], "rb")) == NULL) {
        fputs("SnapTone fixture read failed\n", stderr);
        return 2;
    }
    for (slot = 0u; slot < 4u; ++slot) {
        int lattice_nonzero = 0;
        int nonzero = 0;
        long offset = (long)(GP100_USER_AMP_BLOB_FLASH_BASE +
                             slot * sizeof(blob));

        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(blob, 1u, sizeof(blob), file) != sizeof(blob) ||
            !gp100_snap_tone_load(&state, blob, sizeof(blob)) ||
            state.model.first_bank_value_count != 128u ||
            state.model.second_bank_value_count != 1024u) {
            fclose(file);
            fputs("SnapTone model load failed\n", stderr);
            return 3;
        }
        memset(samples, 0, sizeof(samples));
        samples[0] = 0.25f;
        if (!gp100_snap_tone_process_mono(
                &state, samples, sizeof(samples) / sizeof(samples[0]))) {
            fclose(file);
            fputs("SnapTone processing failed\n", stderr);
            return 4;
        }
        for (index = 0u; index < sizeof(samples) / sizeof(samples[0]);
             ++index) {
            if (!isfinite(samples[index])) {
                fclose(file);
                fputs("SnapTone produced a non-finite sample\n", stderr);
                return 5;
            }
            nonzero |= samples[index] != 0.0f;
        }
        for (index = 0u; index < GP100_SNAP_TONE_EXPAND_ORDER;
             ++index) {
            lattice_nonzero |= state.expand_state[index] != 0.0f;
        }
        for (index = 0u; index < GP100_SNAP_TONE_COLLAPSE_ORDER;
             ++index) {
            lattice_nonzero |= state.collapse_state[index] != 0.0f;
        }
        lattice_nonzero |= state.collapse_delay[0] != 0.0f;
        lattice_nonzero |= state.collapse_delay[1] != 0.0f;
        if (!nonzero || !lattice_nonzero ||
            (slot != 0u && memcmp(
                first_output, samples, sizeof(samples)) == 0)) {
            fclose(file);
            fputs("SnapTone output validation failed\n", stderr);
            return 6;
        }
        if (slot == 0u) {
            memcpy(first_output, samples, sizeof(samples));
        }
    }
    if (fseek(file, (long)(GP100_USER_AMP_BLOB_FLASH_BASE +
                          4u * sizeof(blob)), SEEK_SET) != 0 ||
        fread(blob, 1u, sizeof(blob), file) != sizeof(blob)) {
        fclose(file);
        fputs("SnapTone erased fixture read failed\n", stderr);
        return 7;
    }
    fclose(file);
    if (gp100_snap_tone_load(&state, blob, sizeof(blob)) ||
        gp100_snap_tone_process_mono(NULL, samples, 1u) ||
        gp100_snap_tone_process_mono(&state, samples, 63u) ||
        gp100_snap_tone_process_stereo(
            &state, parameters, true, stereo, 63u)) {
        fputs("SnapTone output/validation failed\n", stderr);
        return 8;
    }
    for (index = 0u; index < sizeof(stereo) / sizeof(stereo[0]); index += 2u) {
        stereo[index] = 0.1f;
        stereo[index + 1u] = -0.1f;
    }
    if (!gp100_snap_tone_process_stereo(
            &state, parameters, true, stereo,
            sizeof(stereo) / sizeof(stereo[0]) / 2u) ||
        !gp100_snap_tone_process_stereo(
            &state, parameters, false, stereo,
            sizeof(stereo) / sizeof(stereo[0]) / 2u)) {
        fputs("SnapTone stereo wrapper failed\n", stderr);
        return 9;
    }
    for (index = 0u; index < sizeof(stereo) / sizeof(stereo[0]); ++index) {
        if (!isfinite(stereo[index])) {
            fputs("SnapTone stereo wrapper produced non-finite output\n",
                  stderr);
            return 10;
        }
    }
    return 0;
}
