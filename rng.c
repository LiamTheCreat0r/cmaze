/*
 * rng.c - splitmix64, a tiny seeded generator that behaves identically
 * everywhere so `-s SEED` always reproduces the same maze.
 */

#include "cmaze.h"

void rng_seed(Rng *r, unsigned long long seed)
{
    r->state = seed * 0x9E3779B97F4A7C15ULL + 0x1234567890ABCDEFULL;
}

unsigned long long rng_next(Rng *r)
{
    unsigned long long z = (r->state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

unsigned rng_below(Rng *r, unsigned n)
{
    if (n <= 1)
        return 0;
    return (unsigned)(rng_next(r) % n);
}

double rng_double(Rng *r)
{
    return (double)(rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}
