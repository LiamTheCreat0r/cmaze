/*
 * algos.h - one extern declaration per algorithm implementation file.
 */

#ifndef CMAZE_ALGOS_H
#define CMAZE_ALGOS_H

#include "../cmaze.h"

/* Shared helper: random one of `n` already-filtered directions. */
static inline int algo_pick_dir(Rng *rng, const int *dirs, int n)
{
    return dirs[rng_below(rng, (unsigned)n)];
}

#endif
