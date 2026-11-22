/* Host test for patch 0005: the finer MASTER curve. Build against the patched OpenGP100 tree (see tests/test_firmware_parser.py). */
#include "audio/engine/audio_output.h"

#include <math.h>
#include <stdio.h>

static int fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fail = 1; } } while (0)

static double db(uint32_t g) { return 20.0 * log10((double)g / GP100_AUDIO_MASTER_FINE_MAX); }

int main(void)
{
    uint8_t p;
    uint32_t prev = 0;
    gp100_audio_master_fine_t m;
    float buf[128];
    int i;

    CHECK(gp100_audio_master_fine_gain(0) == 0u);
    CHECK(gp100_audio_master_fine_gain(100) == GP100_AUDIO_MASTER_FINE_MAX);
    CHECK(fabs(db(gp100_audio_master_fine_gain(20)) - (-34.0)) < 0.2);     /* about stock 2 */
    CHECK(fabs(db(gp100_audio_master_fine_gain(40)) - (-20.0)) < 0.2);     /* about stock 10 */
    for (p = 1; p <= 100; ++p) {                                           /* strictly increasing: every step is audible */
        uint32_t g = gp100_audio_master_fine_gain(p);
        CHECK(g > prev);
        prev = g;
    }
    CHECK(gp100_audio_master_fine_gain(200) == GP100_AUDIO_MASTER_FINE_MAX);   /* out of range is clamped */

    /* ramp: starts silent, reaches the target and stays; 100 ends at the stock full-scale 255/256 */
    gp100_audio_master_fine_reset(&m);
    m.target_percent = 100;
    for (i = 0; i < 300; ++i) {
        int k;
        for (k = 0; k < 128; ++k) buf[k] = 1.0f;
        CHECK(gp100_audio_master_fine_apply(&m, buf, 64));
    }
    CHECK(m.current_gain == GP100_AUDIO_MASTER_FINE_MAX);
    CHECK(fabsf(buf[0] - 255.0f / 256.0f) < 1e-6f);
    m.target_percent = 0;
    for (i = 0; i < 300; ++i) { int k; for (k = 0; k < 128; ++k) buf[k] = 1.0f; gp100_audio_master_fine_apply(&m, buf, 64); }
    CHECK(m.current_gain == 0u && buf[0] == 0.0f);                         /* 0 is silent */
    m.target_percent = 101;
    CHECK(!gp100_audio_master_fine_apply(&m, buf, 64));
    CHECK(!gp100_audio_master_fine_apply(NULL, buf, 64));
    puts(fail ? "FAILED" : "ok");
    return fail;
}
