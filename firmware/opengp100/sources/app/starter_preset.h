#ifndef GP100_STARTER_PRESET_H
#define GP100_STARTER_PRESET_H

#include "storage/patch_chain.h"

#include <stdbool.h>

/* Build the built-in "Starter" preset: Green OD (FS1) -> Bellman 59N -> Bellman 4x10 -> Slapback (FS2, off) -> Spring.
 * Returns false if any step is refused by the chain rules (the caller then falls back to an empty preset). */
bool gp100_make_starter_preset(gp100_preset_record_t *preset);

#endif
