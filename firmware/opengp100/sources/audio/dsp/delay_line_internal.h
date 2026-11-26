#ifndef GP100_DELAY_LINE_INTERNAL_H
#define GP100_DELAY_LINE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

/* A clear resets the cursor and full flag. Samples ahead of the cursor
 * read as zero until the ring has been written once. */
static inline void gp100_delay_write(float *line,
                                         size_t length,
                                         size_t *cursor,
                                         bool *full,
                                         float sample)
{
    line[*cursor] = sample;
    ++*cursor;
    if (*cursor == length) {
        *cursor = 0u;
        *full = true;
    }
}

/* Read behind the supplied cursor. Delay is in [1, length] samples. */
static inline float gp100_delay_read_linear_valid(const float *line,
                                                      size_t length,
                                                      size_t cursor,
                                                      float delay,
                                                      bool full)
{
    const size_t whole = (size_t)delay;
    const float fraction = delay - (float)whole;
    const size_t newer = cursor >= whole ? cursor - whole
                                         : cursor + length - whole;
    const size_t older = newer == 0u ? length - 1u : newer - 1u;
    const float now = full || newer < cursor ? line[newer] : 0.0f;
    const float previous = full || older < cursor ? line[older] : 0.0f;

    return now * (1.0f - fraction) + previous * fraction;
}

static inline float gp100_delay_read_linear(const float *line,
                                                size_t length,
                                                size_t cursor,
                                                float delay)
{
    return gp100_delay_read_linear_valid(line, length, cursor, delay, true);
}

#endif
