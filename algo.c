/*
 * algo.c - the algorithm registry: name lookup and random selection.
 */

#include "cmaze.h"

static const Algo *const registry[] = {
    &algo_backtracker,
    &algo_prim,
    &algo_kruskal,
    &algo_wilson,
    &algo_aldous_broder,
    &algo_eller,
    &algo_growing_tree,
    &algo_division,
    &algo_sidewinder,
    &algo_binary_tree,
    &algo_hunt_kill,
};

#define N_REGISTRY ((int)(sizeof registry / sizeof registry[0]))

const Algo *algo_at(int i)
{
    if (i < 0 || i >= N_REGISTRY)
        return NULL;
    return registry[i];
}

int algo_count(void)
{
    return N_REGISTRY;
}

/* Case/dash-insensitive match so `aldous_broder`, `Aldous-Broder` and
 * `aldous-broder` all work. */
static int name_eq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x == '_') x = '-';
        if (y == '_') y = '-';
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y)
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

const Algo *algo_find(const char *name)
{
    int i;

    if (!name)
        return NULL;
    for (i = 0; i < N_REGISTRY; i++)
        if (name_eq(registry[i]->name, name))
            return registry[i];
    return NULL;
}

const Algo *algo_pick(Rng *rng)
{
    return registry[rng_below(rng, (unsigned)N_REGISTRY)];
}
